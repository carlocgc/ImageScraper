#include "services/TumblrService.h"
#include "io/JsonFile.h"
#include "log/Logger.h"
#include "ui/FrontEnd.h"
#include "async/TaskManager.h"
#include "requests/RequestTypes.h"
#include "requests/tumblr/RetrievePublishedPostsRequest.h"
#include "utils/DownloadUtils.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"

const std::string ImageScraper::TumblrService::s_UserDataKey_ApiKey = "tumblr_api_key";

ImageScraper::TumblrService::TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
    : Service( appConfig, userConfig, caBundle, frontEnd )
{
    if( !m_UserConfig->GetValue<std::string>( s_UserDataKey_ApiKey, m_ApiKey ) )
    {
        WarningLog( "[%s] Could not find tumblr api key, add api key to %s to be able to download tumblr content!", __FUNCTION__, m_UserConfig->GetFilePath( ).c_str( ) );
    }
}

bool ImageScraper::TumblrService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Tumblr )
    {
        return false;
    }

    if( m_ApiKey == "" )
    {
        ErrorLog( "[%s] Could not find tumblr api key, add api key to %s to be able to download tumblr content!", __FUNCTION__, m_UserConfig->GetFilePath( ).c_str( ) );
        return false;
    }

    DownloadContent( options );
    return true;
}

void ImageScraper::TumblrService::DownloadContent( const UserInputOptions& inputOptions )
{
    InfoLog( "[%s] Starting Tumblr media download!", __FUNCTION__ );
    DebugLog( "[%s] User: %s", __FUNCTION__, inputOptions.m_TumblrUser.c_str( ) );

    auto onComplete = [ & ]( int filesDownloaded )
    {
        InfoLog( "[%s] Content download complete!, files downloaded: %i", __FUNCTION__, filesDownloaded );
        m_FrontEnd->SetInputState( InputState::Free );
    };

    auto onFail = [ & ]( )
    {
        ErrorLog( "[%s] Failed to download media!, See log for details.", __FUNCTION__ );
        m_FrontEnd->SetInputState( InputState::Free );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_NetworkContext, [ &, options = inputOptions, onComplete, onFail ]( )
        {
            RequestOptions retrievePostsOptions{ };
            retrievePostsOptions.m_QueryParams.push_back( { "api_key", m_ApiKey } );
            retrievePostsOptions.m_UrlExt = options.m_TumblrUser + ".tumblr.com/posts";
            retrievePostsOptions.m_CaBundle = m_CaBundle;
            retrievePostsOptions.m_UserAgent = m_UserAgent;

            Tumblr::RetrievePublishedPostsRequest retrievePostsRequest{ };
            RequestResult fetchResult = retrievePostsRequest.Perform( retrievePostsOptions );

            if( !fetchResult.m_Success )
            {
                WarningLog( "[%s] Failed to get tumblr blog post data, error: %s", __FUNCTION__, fetchResult.m_Error.m_ErrorString.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            InfoLog( "[%s] Tumblr posts retrieved successfully.", __FUNCTION__ );
            DebugLog( "[%s] Response: %s", __FUNCTION__, fetchResult.m_Response.c_str( ) );

            // Parse response
            json response = json::parse( fetchResult.m_Response );
            std::vector<std::string> mediaUrls{ };
            mediaUrls = GetMediaUrlsFromResponse( response );

            if( mediaUrls.empty( ) )
            {
                WarningLog( "[%s] No content to download, nothing was done...", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            // Create download directory
            const std::filesystem::path dir = std::filesystem::current_path( ) / "Downloads" / "Tumblr" / options.m_TumblrUser;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                ErrorLog( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            int filesDownloaded = 0;

            InfoLog( "[%s] Started downloading content, urls: %i", __FUNCTION__, mediaUrls.size( ) );

            std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

            // Download images
            for( std::string& url : mediaUrls )
            {
                const std::string newUrl = DownloadHelpers::RedirectToPreferredFileTypeUrl( url );

                if( newUrl != "" )
                {
                    url = newUrl;
                }

                std::vector<char> buffer{ };

                DownloadOptions options{ };
                options.m_CaBundle = m_CaBundle;
                options.m_Url = url;
                options.m_UserAgent = m_UserAgent;
                options.m_BufferPtr = &buffer;

                DownloadRequest request{ };
                RequestResult result = request.Perform( options );
                if( !result.m_Success )
                {
                    ErrorLog( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), url.c_str( ) );
                    continue;
                }

                const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( options.m_Url );
                const std::string filepath = dirStr + "/" + filename;

                std::ofstream outfile{ filepath, std::ios::binary };
                if( !outfile.is_open( ) )
                {
                    ErrorLog( "[%s] Download failed, could not open file for write: %s", __FUNCTION__, filepath.c_str( ) );
                    continue;
                }

                outfile.write( buffer.data( ), buffer.size( ) );
                outfile.close( );

                ++filesDownloaded;
                InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, mediaUrls.size( ), filepath.c_str( ) );

                std::this_thread::sleep_for( std::chrono::seconds{ 1 } );
            }

            TaskManager::Instance( ).SubmitMain( onComplete, filesDownloaded );
        } );

    ( void )task;
}

std::vector<std::string> ImageScraper::TumblrService::GetMediaUrlsFromResponse( const Json& response )
{
    std::vector<std::string> mediaUrls{ };

    for( const auto& post : response[ "response" ][ "posts" ] )
    {
        if( post[ "type" ] == "photo" )
        {
            for( const auto& photo : post[ "photos" ] )
            {
                if( photo.contains( "original_size" ) )
                {
                    mediaUrls.push_back( photo[ "original_size" ][ "url" ] );
                }
            }
        }
        else if( post[ "type" ] == "video" )
        {
            if( post.contains( "video_url" ) )
            {
                mediaUrls.push_back( post[ "video_url" ] );
            }
        }
    }

    return mediaUrls;
}
