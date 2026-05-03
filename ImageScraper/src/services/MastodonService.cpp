#include "services/MastodonService.h"

#include "async/TaskManager.h"
#include "requests/RequestTypes.h"
#include "requests/mastodon/GetAccountStatusesRequest.h"
#include "requests/mastodon/SearchAccountsRequest.h"
#include "log/Logger.h"
#include "utils/DownloadUtils.h"

#include <algorithm>
#include <filesystem>

using Json = nlohmann::json;

namespace
{
    constexpr int s_MastodonAccountSearchLimit = 40;
    constexpr int s_MastodonStatusesPageLimit = 40;

    // Mastodon's reference default is 300 requests per 5 minutes for unauthenticated calls; varies by instance.
    const ImageScraper::RateLimitTable s_Limits =
    {
        { "search_accounts",                   { { 300, 300 } } },
        { "get_account_statuses",              { { 300, 300 } } },
        { ImageScraper::s_DefaultRateLimitKey, { { 300, 300 } } },
    };
}

ImageScraper::MastodonService::MastodonService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver )
    : Service( ContentProvider::Mastodon, appConfig, userConfig, caBundle, outputDir, sink, s_Limits, std::move( urlResolver ) )
{
}

bool ImageScraper::MastodonService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Mastodon )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::MastodonService::OpenExternalAuth( )
{
    LogError( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
    return false;
}

bool ImageScraper::MastodonService::HandleExternalAuth( const std::string& response )
{
    ( void )response;
    return false;
}

bool ImageScraper::MastodonService::IsSignedIn( ) const
{
    return false;
}

void ImageScraper::MastodonService::Authenticate( AuthenticateCallback callback )
{
    callback( m_ContentProvider, true );
}

bool ImageScraper::MastodonService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

std::optional<ImageScraper::MastodonUtils::Account> ImageScraper::MastodonService::ResolveAccount( const std::string& instanceUrl, const std::string& accountInput )
{
    const MastodonUtils::NormalizedAccount normalizedAccount = MastodonUtils::NormalizeAccountInput( accountInput );
    if( normalizedAccount.m_SearchQuery.empty( ) )
    {
        LogError( "[%s] Mastodon account input was empty or invalid.", __FUNCTION__ );
        return std::nullopt;
    }

    RequestOptions options{ };
    options.m_CaBundle = m_CaBundle;
    options.m_UserAgent = m_UserAgent;
    options.m_UrlExt = instanceUrl;
    options.m_QueryParams.push_back( { "q", normalizedAccount.m_SearchQuery } );
    options.m_QueryParams.push_back( { "limit", std::to_string( s_MastodonAccountSearchLimit ) } );

    Mastodon::SearchAccountsRequest request{ m_HttpClient };
    const RequestResult result = request.Perform( options );
    if( !result.m_Success )
    {
        LogError( "[%s] Failed to search Mastodon account %s: %s", __FUNCTION__, normalizedAccount.m_SearchQuery.c_str( ), result.m_Error.m_ErrorString.c_str( ) );
        return std::nullopt;
    }

    try
    {
        const Json response = Json::parse( result.m_Response );
        const std::optional<MastodonUtils::Account> account = MastodonUtils::SelectAccountFromSearchResponse( response, accountInput, instanceUrl );
        if( !account.has_value( ) )
        {
            LogError( "[%s] Could not find an exact Mastodon account match for %s on %s.", __FUNCTION__, normalizedAccount.m_SearchQuery.c_str( ), instanceUrl.c_str( ) );
            return std::nullopt;
        }

        if( account->m_Id.empty( ) )
        {
            LogError( "[%s] Mastodon account match for %s did not include an account ID.", __FUNCTION__, normalizedAccount.m_SearchQuery.c_str( ) );
            return std::nullopt;
        }

        return account;
    }
    catch( const Json::exception& ex )
    {
        LogError( "[%s] Failed to parse Mastodon account search response: %s", __FUNCTION__, ex.what( ) );
        return std::nullopt;
    }
}

std::vector<ImageScraper::MastodonUtils::MediaItem> ImageScraper::MastodonService::FetchAccountStatusMedia( const std::string& instanceUrl, const std::string& accountId, const std::string& accountInput, int maxItems )
{
    std::vector<MastodonUtils::MediaItem> mediaItems{ };
    std::string maxId{ };
    int pageNum = 1;

    while( !IsCancelled( ) && static_cast<int>( MastodonUtils::PrepareMediaDownloads( mediaItems, maxItems, instanceUrl, accountInput ).size( ) ) < maxItems )
    {
        RequestOptions options{ };
        options.m_CaBundle = m_CaBundle;
        options.m_UserAgent = m_UserAgent;
        options.m_UrlExt = instanceUrl;
        options.m_ResourceId = accountId;
        options.m_QueryParams.push_back( { "only_media", "true" } );
        options.m_QueryParams.push_back( { "limit", std::to_string( s_MastodonStatusesPageLimit ) } );
        options.m_QueryParams.push_back( { "exclude_replies", "true" } );
        options.m_QueryParams.push_back( { "exclude_reblogs", "true" } );

        if( !maxId.empty( ) )
        {
            options.m_QueryParams.push_back( { "max_id", maxId } );
        }

        Mastodon::GetAccountStatusesRequest request{ m_HttpClient };
        const RequestResult result = request.Perform( options );
        if( !result.m_Success )
        {
            LogError( "[%s] Failed to fetch Mastodon statuses page %i: %s", __FUNCTION__, pageNum, result.m_Error.m_ErrorString.c_str( ) );
            break;
        }

        try
        {
            const Json response = Json::parse( result.m_Response );
            const std::vector<MastodonUtils::MediaItem> pageItems = MastodonUtils::GetMediaItemsFromStatusesResponse( response, maxItems );
            mediaItems.insert( mediaItems.end( ), pageItems.begin( ), pageItems.end( ) );

            const int queuedDownloads = static_cast<int>( MastodonUtils::PrepareMediaDownloads( mediaItems, maxItems, instanceUrl, accountInput ).size( ) );
            InfoLog( "[%s] Mastodon statuses page %i parsed, %i media items found, %i unique media downloads queued.", __FUNCTION__, pageNum, static_cast<int>( pageItems.size( ) ), queuedDownloads );

            const std::string nextMaxId = MastodonUtils::GetLastStatusIdFromStatusesResponse( response );
            if( nextMaxId.empty( ) || nextMaxId == maxId )
            {
                break;
            }

            maxId = nextMaxId;
        }
        catch( const Json::exception& ex )
        {
            LogError( "[%s] Failed to parse Mastodon statuses response: %s", __FUNCTION__, ex.what( ) );
            break;
        }

        ++pageNum;
    }

    return mediaItems;
}

void ImageScraper::MastodonService::DownloadContent( const UserInputOptions& inputOptions )
{
    auto onComplete = [ this ]( int filesDownloaded )
    {
        SuccessLog( "[%s] Content download complete!, files downloaded: %i", __FUNCTION__, filesDownloaded );
        m_Sink->OnRunComplete( );
    };

    auto onFail = [ this ]( )
    {
        LogError( "[%s] Failed to download Mastodon media!, See log for details.", __FUNCTION__ );
        m_Sink->OnRunComplete( );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, inputOptions, onComplete, onFail ]( )
        {
            const std::string instanceUrl = MastodonUtils::NormalizeInstanceUrl( inputOptions.m_MastodonInstance );
            const std::string accountInput = inputOptions.m_MastodonAccount;
            const int maxItems = std::clamp( inputOptions.m_MastodonMaxMediaItems, MASTODON_LIMIT_MIN, MASTODON_LIMIT_MAX );

            InfoLog( "[%s] Starting Mastodon media download for instance: %s, account: %s", __FUNCTION__, instanceUrl.c_str( ), accountInput.c_str( ) );

            if( instanceUrl.empty( ) )
            {
                LogError( "[%s] Missing or invalid Mastodon instance URL: %s", __FUNCTION__, inputOptions.m_MastodonInstance.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const std::optional<MastodonUtils::Account> account = ResolveAccount( instanceUrl, accountInput );
            if( !account.has_value( ) )
            {
                LogError( "[%s] Could not resolve Mastodon account.", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            const std::string accountLabel = account->m_Acct.empty( ) ? accountInput : account->m_Acct;
            InfoLog( "[%s] Mastodon account resolved to ID: %s (%s)", __FUNCTION__, account->m_Id.c_str( ), accountLabel.c_str( ) );

            const std::vector<MastodonUtils::MediaItem> mediaItems = FetchAccountStatusMedia( instanceUrl, account->m_Id, accountLabel, maxItems );
            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            if( mediaItems.empty( ) )
            {
                WarningLog( "[%s] No Mastodon media items were found for account: %s", __FUNCTION__, accountLabel.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const std::vector<MastodonUtils::PreparedDownload> preparedDownloads = MastodonUtils::PrepareMediaDownloads( mediaItems, maxItems, instanceUrl, accountLabel );
            if( preparedDownloads.empty( ) )
            {
                WarningLog( "[%s] No downloadable Mastodon media items were found for account: %s", __FUNCTION__, accountLabel.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const int imageCount = static_cast<int>( std::count_if( mediaItems.begin( ), mediaItems.end( ), []( const MastodonUtils::MediaItem& item )
                {
                    return item.m_Kind == MastodonUtils::MediaKind::Image;
                } ) );
            const int gifvCount = static_cast<int>( std::count_if( mediaItems.begin( ), mediaItems.end( ), []( const MastodonUtils::MediaItem& item )
                {
                    return item.m_Kind == MastodonUtils::MediaKind::Gifv;
                } ) );
            const int videoCount = static_cast<int>( mediaItems.size( ) ) - imageCount - gifvCount;

            InfoLog( "[%s] Prepared %i unique Mastodon downloads for %s from %i fetched media items (%i images, %i gifv, %i videos).", __FUNCTION__, static_cast<int>( preparedDownloads.size( ) ), accountLabel.c_str( ), static_cast<int>( mediaItems.size( ) ), imageCount, gifvCount, videoCount );

            const std::filesystem::path dir = std::filesystem::path( m_OutputDir ) / "Mastodon" / preparedDownloads.front( ).m_InstanceDirectory / preparedDownloads.front( ).m_AccountDirectory;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                LogError( "[%s] Failed to create download directory: %s", __FUNCTION__, dirStr.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            std::vector<MediaDownload> downloads{ };
            downloads.reserve( preparedDownloads.size( ) );
            for( const MastodonUtils::PreparedDownload& preparedDownload : preparedDownloads )
            {
                downloads.push_back( {
                    preparedDownload.m_SourceUrl,
                    preparedDownload.m_FileName,
                    DownloadMethod::DirectFile
                    } );
            }

            const std::optional<int> filesDownloaded = DownloadMedia( downloads, dir );
            if( !filesDownloaded.has_value( ) )
            {
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            TaskManager::Instance( ).SubmitMain( onComplete, *filesDownloaded );
        } );

    ( void )task;
}
