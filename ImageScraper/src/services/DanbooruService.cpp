#include "services/DanbooruService.h"

#include "async/TaskManager.h"
#include "log/Logger.h"
#include "requests/danbooru/GetPostRequest.h"
#include "requests/danbooru/SearchPostsRequest.h"
#include "utils/DownloadUtils.h"

#include <unordered_set>

namespace
{
    constexpr int s_DanbooruPostsPageLimit = 200;

    const ImageScraper::RateLimitTable s_Limits =
    {
        { "search_posts",                     { { 2, 1 } } },
        { "get_post",                         { { 2, 1 } } },
        { ImageScraper::s_DefaultRateLimitKey, { { 2, 1 } } },
    };
}

ImageScraper::DanbooruService::DanbooruService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver )
    : Service( ContentProvider::Danbooru, appConfig, userConfig, caBundle, outputDir, sink, s_Limits, std::move( urlResolver ) )
{
}

bool ImageScraper::DanbooruService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Danbooru )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::DanbooruService::OpenExternalAuth( )
{
    LogError( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
    return false;
}

bool ImageScraper::DanbooruService::HandleExternalAuth( const std::string& response )
{
    ( void )response;
    return false;
}

bool ImageScraper::DanbooruService::IsSignedIn( ) const
{
    return false;
}

void ImageScraper::DanbooruService::Authenticate( AuthenticateCallback callback )
{
    callback( m_ContentProvider, true );
}

bool ImageScraper::DanbooruService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

bool ImageScraper::DanbooruService::FetchSinglePost( const std::string& postId, std::optional<DanbooruUtils::PreparedDownload>& downloadOut )
{
    RequestOptions options{ };
    options.m_CaBundle = m_CaBundle;
    options.m_UserAgent = m_UserAgent;
    options.m_ResourceId = postId;

    Danbooru::GetPostRequest request{ m_HttpClient };
    const RequestResult result = request.Perform( options );
    if( !result.m_Success )
    {
        LogError( "[%s] Failed to fetch Danbooru post %s: %s", __FUNCTION__, postId.c_str( ), result.m_Error.m_ErrorString.c_str( ) );
        return false;
    }

    try
    {
        const DanbooruUtils::Json response = DanbooruUtils::Json::parse( result.m_Response );
        downloadOut = DanbooruUtils::PreparedDownloadFromPost( response );
        return true;
    }
    catch( const DanbooruUtils::Json::exception& ex )
    {
        LogError( "[%s] Failed to parse Danbooru post response: %s", __FUNCTION__, ex.what( ) );
        return false;
    }
}

std::optional<std::vector<ImageScraper::DanbooruUtils::PreparedDownload>> ImageScraper::DanbooruService::FetchQueryPosts( const std::string& query, int maxItems )
{
    std::vector<DanbooruUtils::PreparedDownload> downloads{ };
    std::unordered_set<std::string> seenPostIds{ };
    int page = 1;

    while( !IsCancelled( ) && static_cast<int>( downloads.size( ) ) < maxItems )
    {
        RequestOptions options{ };
        options.m_CaBundle = m_CaBundle;
        options.m_UserAgent = m_UserAgent;
        options.m_QueryParams.push_back( { "tags", query } );
        options.m_QueryParams.push_back( { "limit", std::to_string( s_DanbooruPostsPageLimit ) } );
        options.m_QueryParams.push_back( { "page", std::to_string( page ) } );

        Danbooru::SearchPostsRequest request{ m_HttpClient };
        const RequestResult result = request.Perform( options );
        if( !result.m_Success )
        {
            LogError( "[%s] Failed to fetch Danbooru search page %i: %s", __FUNCTION__, page, result.m_Error.m_ErrorString.c_str( ) );
            return std::nullopt;
        }

        try
        {
            const DanbooruUtils::Json response = DanbooruUtils::Json::parse( result.m_Response );
            const int beforeCount = static_cast<int>( downloads.size( ) );
            DanbooruUtils::AppendDownloadablePosts( response, maxItems, seenPostIds, downloads );
            const int addedCount = static_cast<int>( downloads.size( ) ) - beforeCount;
            InfoLog( "[%s] Danbooru search page %i parsed, %i downloads queued from this page, %i/%i total.", __FUNCTION__, page, addedCount, static_cast<int>( downloads.size( ) ), maxItems );

            if( !response.is_array( ) || response.empty( ) || addedCount <= 0 )
            {
                break;
            }
        }
        catch( const DanbooruUtils::Json::exception& ex )
        {
            LogError( "[%s] Failed to parse Danbooru search response: %s", __FUNCTION__, ex.what( ) );
            return std::nullopt;
        }

        ++page;
    }

    return downloads;
}

void ImageScraper::DanbooruService::DownloadContent( const UserInputOptions& inputOptions )
{
    auto onComplete = [ this ]( int filesDownloaded )
    {
        SuccessLog( "[%s] Content download complete!, files downloaded: %i", __FUNCTION__, filesDownloaded );
        m_Sink->OnRunComplete( );
    };

    auto onCancelled = [ this ]( )
    {
        InfoLog( "[%s] Content download cancelled by user.", __FUNCTION__ );
        m_Sink->OnRunComplete( );
    };

    auto onFail = [ this ]( )
    {
        LogError( "[%s] Failed to download Danbooru media!, See log for details.", __FUNCTION__ );
        m_Sink->OnRunComplete( );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, query = inputOptions.m_DanbooruQuery, maxItems = inputOptions.m_DanbooruMaxMediaItems, onComplete, onCancelled, onFail ]( )
        {
            InfoLog( "[%s] Starting Danbooru media download for input: %s", __FUNCTION__, query.c_str( ) );

            const DanbooruUtils::NormalizedInput normalized = DanbooruUtils::NormalizeInput( query );
            if( normalized.m_Type == DanbooruUtils::InputType::Invalid )
            {
                LogError( "[%s] Invalid Danbooru input: %s", __FUNCTION__, normalized.m_Error.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            std::vector<DanbooruUtils::PreparedDownload> preparedDownloads{ };
            std::filesystem::path dir{ };

            if( normalized.m_Type == DanbooruUtils::InputType::SinglePost )
            {
                std::optional<DanbooruUtils::PreparedDownload> post{ };
                if( !FetchSinglePost( normalized.m_PostId, post ) )
                {
                    TaskManager::Instance( ).SubmitMain( onFail );
                    return;
                }

                if( post.has_value( ) )
                {
                    preparedDownloads.push_back( *post );
                }

                dir = std::filesystem::path( m_OutputDir ) / "Danbooru" / "Posts";
            }
            else
            {
                const std::optional<std::vector<DanbooruUtils::PreparedDownload>> queryDownloads = FetchQueryPosts( normalized.m_Query, maxItems );
                if( !queryDownloads.has_value( ) )
                {
                    TaskManager::Instance( ).SubmitMain( onFail );
                    return;
                }

                preparedDownloads = *queryDownloads;
                dir = std::filesystem::path( m_OutputDir ) / "Danbooru" / "Query" / DanbooruUtils::SanitizePathComponent( normalized.m_Query );
            }

            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onCancelled );
                return;
            }

            if( preparedDownloads.empty( ) )
            {
                WarningLog( "[%s] No downloadable Danbooru media was found.", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                LogError( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.string( ).c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            std::vector<MediaDownload> downloads{ };
            downloads.reserve( preparedDownloads.size( ) );
            for( const DanbooruUtils::PreparedDownload& preparedDownload : preparedDownloads )
            {
                downloads.push_back( { preparedDownload.m_SourceUrl, preparedDownload.m_FileName, DownloadMethod::DirectFile } );
            }

            const std::optional<int> filesDownloaded = DownloadMedia( downloads, dir );
            if( !filesDownloaded.has_value( ) )
            {
                if( IsCancelled( ) )
                {
                    TaskManager::Instance( ).SubmitMain( onCancelled );
                }
                else
                {
                    TaskManager::Instance( ).SubmitMain( onFail );
                }
                return;
            }

            TaskManager::Instance( ).SubmitMain( onComplete, *filesDownloaded );
        } );

    ( void )task;
}
