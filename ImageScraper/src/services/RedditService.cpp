#include "services/RedditService.h"
#include "requests/RequestTypes.h"
#include "requests/RedditRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"
#include "parsers/RedditParser.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "async/TaskManager.h"
#include "ui/FrontEnd.h"
#include "io/JsonFile.h"
#include "utils/StringUtils.h"

#include <string>
#include <map>

using json = nlohmann::json;

const std::string ImageScraper::RedditService::s_UserAgent = "Windows:ImageScraper:v0:/u/carlocgc";
const std::string ImageScraper::RedditService::s_AppDataKey_DeviceId = "reddit_device_id";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientId = "reddit_client_id";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientSecret = "reddit_client_secret";

ImageScraper::RedditService::RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
    : Service( appConfig, userConfig, caBundle )
    , m_FrontEnd{ frontEnd }
{
    if( !m_AppConfig->GetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId ) )
    {
        m_DeviceId = StringUtils::CreateGuid( 30 );
        m_AppConfig->SetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId );
        m_AppConfig->Serialise( );
        DebugLog( "[%s] Created Reddit device id: %s", __FUNCTION__, m_DeviceId.c_str( ) );
    }

    DebugLog( "[%s] Loaded Reddit device id: %s", __FUNCTION__, m_DeviceId.c_str( ) );

    if( !m_AppConfig->GetValue<std::string>( s_UserDataKey_ClientId, m_ClientId ) )
    {
        WarningLog( "[%s] Could not find reddit client id, add client id to %s to be able to authenticate with the reddit api!", __FUNCTION__, m_UserConfig->GetFilePath().c_str( ) );
    }
}

bool ImageScraper::RedditService::HandleUrl( const std::string& url )
{
    // TODO Input should be an options struct
    // Service, url, limit, conversion preferences etc
    if( true )
    {
        DownloadHotReddit( url );
    }

    return true;
}

void ImageScraper::RedditService::DownloadHotReddit( const std::string& subreddit )
{
    InfoLog( "[%s] Starting Reddit Hot Media Download!, Subreddit: %s", __FUNCTION__, subreddit );

    auto complete = [ & ]( int urlsProcessed )
    {
        InfoLog( "[%s] Task Complete!, Media Count: %d", __FUNCTION__, urlsProcessed );
        m_FrontEnd->SetInputState( InputState::Free );
    };

    auto fail = [ & ]( )
    {
        ErrorLog( "[%s] Task Failed!, See log for details.", __FUNCTION__ );
        m_FrontEnd->SetInputState( InputState::Free );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_NetworkContext, [ &, subreddit, complete, fail ]( )
        {
            std::thread::id id = std::this_thread::get_id( );
            std::stringstream ss;
            ss << id;
            std::string outLog = ss.str( );
            DebugLog( "[%s] Task Started: invoked from thread: %s", __FUNCTION__, outLog.c_str( ) );

            RequestOptions options{ };
            options.m_CaBundle = m_CaBundle;
            options.m_UserAgent = s_UserAgent;
            options.m_Url = "https://www.reddit.com/r/" + subreddit + "/hot.json?limit=100";

            RedditRequest request{ };
            RequestResult result = request.Perform( options );

            if( !result.m_Success )
            {
                WarningLog( "[%s] RedditRequest failed with error: %i", __FUNCTION__, static_cast< uint16_t >( result.m_Error.m_ErrorCode ) );
                TaskManager::Instance( ).SubmitMain( fail );
                return;
            }

            InfoLog( "[%s] RedditRequest success, response: %s", __FUNCTION__, result.m_Response.c_str( ) );

            // Parse response
            json response = json::parse( result.m_Response );
            const std::vector<json>& posts = response[ "data" ][ "children" ];

            std::vector<std::string> urls{ };

            for( const json& post : posts )
            {
                ImageScraper::RedditParser::GetImageUrlFromRedditPost( post, urls );
            }

            if( urls.empty( ) )
            {
                WarningLog( "[%s] No content to download, nothing was done...", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( complete, 0 );
                return;
            }

            InfoLog( "[%s] Starting downloading content, urls: %i", __FUNCTION__, urls.size( ) );

            // Create download directory
            const std::filesystem::path dir = std::filesystem::current_path( ) / "Downloads" / "Reddit" / subreddit;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                ErrorLog( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                TaskManager::Instance( ).SubmitMain( fail );
                return;
            }

            // Download images
            for( std::string& url : urls )
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
                options.m_BufferPtr = &buffer;

                DownloadRequest request{ };
                DownloadResult result = request.Perform( options );
                if( !result.m_Success )
                {
                    ErrorLog( "[%s] Download failed for url: %s", __FUNCTION__, url.c_str( ) );
                    continue;
                }

                const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( options.m_Url );
                const std::string filepath = dirStr + "/" + filename;

                std::ofstream outfile{ filepath, std::ios::binary };
                if( !outfile.is_open( ) )
                {
                    ErrorLog( "[%s] Failed to open file: %s", __FUNCTION__, filepath.c_str( ) );
                    continue;
                }

                outfile.write( buffer.data( ), buffer.size( ) );
                outfile.close( );

                InfoLog( "[%s] File written successfully: %s", __FUNCTION__, filepath.c_str( ) );
            }

            TaskManager::Instance( ).SubmitMain( complete, static_cast<int>( urls.size( ) ) );
        } );

    ( void )task;
}
