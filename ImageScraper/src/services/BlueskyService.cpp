#include "services/BlueskyService.h"

#include "async/TaskManager.h"
#include "requests/RequestTypes.h"
#include "requests/bluesky/GetAuthorFeedRequest.h"
#include "requests/bluesky/ResolveHandleRequest.h"
#include "log/Logger.h"
#include "utils/DownloadUtils.h"

#include <algorithm>

using Json = nlohmann::json;

namespace
{
    constexpr int s_BlueskyAuthorFeedPageLimit = 100;
    constexpr const char* s_BlueskyAuthorFeedFilter = "posts_with_media";
}

ImageScraper::BlueskyService::BlueskyService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver )
    : Service( ContentProvider::Bluesky, appConfig, userConfig, caBundle, outputDir, sink, std::move( urlResolver ) )
{
}

bool ImageScraper::BlueskyService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Bluesky )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::BlueskyService::OpenExternalAuth( )
{
    LogError( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
    return false;
}

bool ImageScraper::BlueskyService::HandleExternalAuth( const std::string& response )
{
    ( void )response;
    return false;
}

bool ImageScraper::BlueskyService::IsSignedIn( ) const
{
    return false;
}

void ImageScraper::BlueskyService::Authenticate( AuthenticateCallback callback )
{
    callback( m_ContentProvider, true );
}

bool ImageScraper::BlueskyService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

std::optional<std::string> ImageScraper::BlueskyService::ResolveActorToDid( const std::string& actor )
{
    if( actor.empty( ) )
    {
        LogError( "[%s] Bluesky actor input was empty.", __FUNCTION__ );
        return std::nullopt;
    }

    if( BlueskyUtils::IsDid( actor ) )
    {
        return actor;
    }

    RequestOptions options{ };
    options.m_CaBundle = m_CaBundle;
    options.m_UserAgent = m_UserAgent;
    options.m_QueryParams.push_back( { "handle", actor } );

    Bluesky::ResolveHandleRequest request{ m_HttpClient };
    const RequestResult result = request.Perform( options );
    if( !result.m_Success )
    {
        LogError( "[%s] Failed to resolve Bluesky handle %s: %s", __FUNCTION__, actor.c_str( ), result.m_Error.m_ErrorString.c_str( ) );
        return std::nullopt;
    }

    try
    {
        const Json response = Json::parse( result.m_Response );
        return BlueskyUtils::ParseDidFromResolveHandleResponse( response );
    }
    catch( const Json::exception& ex )
    {
        LogError( "[%s] Failed to parse Bluesky handle resolution response: %s", __FUNCTION__, ex.what( ) );
        return std::nullopt;
    }
}

std::vector<ImageScraper::BlueskyUtils::MediaItem> ImageScraper::BlueskyService::FetchAuthorFeedMedia( const std::string& actorDid, int maxItems )
{
    std::vector<BlueskyUtils::MediaItem> mediaItems{ };
    std::string cursor{ };
    int pageNum = 1;

    while( !IsCancelled( ) && static_cast<int>( BlueskyUtils::PrepareMediaDownloads( mediaItems, maxItems, actorDid ).size( ) ) < maxItems )
    {
        RequestOptions options{ };
        options.m_CaBundle = m_CaBundle;
        options.m_UserAgent = m_UserAgent;
        options.m_QueryParams.push_back( { "actor", actorDid } );
        options.m_QueryParams.push_back( { "filter", s_BlueskyAuthorFeedFilter } );
        options.m_QueryParams.push_back( { "limit", std::to_string( s_BlueskyAuthorFeedPageLimit ) } );

        if( !cursor.empty( ) )
        {
            options.m_QueryParams.push_back( { "cursor", cursor } );
        }

        Bluesky::GetAuthorFeedRequest request{ m_HttpClient };
        const RequestResult result = request.Perform( options );
        if( !result.m_Success )
        {
            LogError( "[%s] Failed to fetch Bluesky author feed page %i: %s", __FUNCTION__, pageNum, result.m_Error.m_ErrorString.c_str( ) );
            break;
        }

        try
        {
            const Json response = Json::parse( result.m_Response );
            BlueskyUtils::MediaItemsPage page = BlueskyUtils::GetMediaItemsFromAuthorFeedResponse( response, s_BlueskyAuthorFeedPageLimit );
            mediaItems.insert( mediaItems.end( ), page.m_Items.begin( ), page.m_Items.end( ) );
            cursor = page.m_Cursor;

            const int queuedDownloads = static_cast<int>( BlueskyUtils::PrepareMediaDownloads( mediaItems, maxItems, actorDid ).size( ) );
            InfoLog( "[%s] Bluesky author feed page %i parsed, %i media items found, %i unique media downloads queued.", __FUNCTION__, pageNum, static_cast<int>( page.m_Items.size( ) ), queuedDownloads );
        }
        catch( const Json::exception& ex )
        {
            LogError( "[%s] Failed to parse Bluesky author feed response: %s", __FUNCTION__, ex.what( ) );
            break;
        }

        if( cursor.empty( ) )
        {
            break;
        }

        ++pageNum;
    }

    return mediaItems;
}

void ImageScraper::BlueskyService::DownloadContent( const UserInputOptions& inputOptions )
{
    auto onComplete = [ this ]( int filesDownloaded )
    {
        SuccessLog( "[%s] Content download complete!, files downloaded: %i", __FUNCTION__, filesDownloaded );
        m_Sink->OnRunComplete( );
    };

    auto onFail = [ this ]( )
    {
        LogError( "[%s] Failed to download Bluesky media!, See log for details.", __FUNCTION__ );
        m_Sink->OnRunComplete( );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, actor = inputOptions.m_BlueskyActor, maxItems = inputOptions.m_BlueskyMaxMediaItems, onComplete, onFail ]( )
        {
            InfoLog( "[%s] Starting Bluesky media download for actor: %s", __FUNCTION__, actor.c_str( ) );

            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const std::optional<std::string> actorDid = ResolveActorToDid( actor );
            if( !actorDid.has_value( ) )
            {
                LogError( "[%s] Could not resolve Bluesky actor to a DID.", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            InfoLog( "[%s] Bluesky actor resolved to DID: %s", __FUNCTION__, actorDid->c_str( ) );

            const std::vector<BlueskyUtils::MediaItem> mediaItems = FetchAuthorFeedMedia( *actorDid, maxItems );
            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            if( mediaItems.empty( ) )
            {
                WarningLog( "[%s] No Bluesky media items were found for actor: %s", __FUNCTION__, actor.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const int imageCount = static_cast<int>( std::count_if( mediaItems.begin( ), mediaItems.end( ), []( const BlueskyUtils::MediaItem& item )
                {
                    return item.m_Kind == BlueskyUtils::MediaKind::Image;
                } ) );
            const int videoCount = static_cast<int>( mediaItems.size( ) ) - imageCount;

            const std::vector<BlueskyUtils::PreparedDownload> preparedDownloads = BlueskyUtils::PrepareMediaDownloads( mediaItems, maxItems, actor );
            if( preparedDownloads.empty( ) )
            {
                WarningLog( "[%s] No downloadable Bluesky media items were found for actor: %s", __FUNCTION__, actor.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const int preparedImageCount = static_cast<int>( std::count_if( preparedDownloads.begin( ), preparedDownloads.end( ), []( const BlueskyUtils::PreparedDownload& download )
                {
                    return download.m_Kind == BlueskyUtils::MediaKind::Image;
                } ) );
            const int preparedVideoCount = static_cast<int>( preparedDownloads.size( ) ) - preparedImageCount;

            InfoLog( "[%s] Prepared %i unique Bluesky downloads for %s from %i fetched media items (%i images, %i videos -> %i image downloads, %i video downloads).", __FUNCTION__, static_cast<int>( preparedDownloads.size( ) ), actor.c_str( ), static_cast<int>( mediaItems.size( ) ), imageCount, videoCount, preparedImageCount, preparedVideoCount );

            const std::filesystem::path dir = std::filesystem::path( m_OutputDir ) / "Downloads" / "Bluesky" / preparedDownloads.front( ).m_ActorDirectory;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                LogError( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            std::vector<MediaDownload> downloads{ };
            downloads.reserve( preparedDownloads.size( ) );
            for( const BlueskyUtils::PreparedDownload& preparedDownload : preparedDownloads )
            {
                downloads.push_back( {
                    preparedDownload.m_SourceUrl,
                    preparedDownload.m_FileName,
                    preparedDownload.m_Kind == BlueskyUtils::MediaKind::Video ? DownloadMethod::HlsVideo : DownloadMethod::DirectFile
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
