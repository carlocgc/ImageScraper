#include "services/RedditService.h"
#include "asyncgc/ThreadPool.h"
#include "requests/RequestTypes.h"
#include "requests/RedditRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"
#include "parsers/RedditParser.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "async/ThreadPool.h"

using json = nlohmann::json;

void ImageScraper::RedditService::DownloadHotReddit( const Config& config, const std::string& subreddit )
{
    auto task = ThreadPool::Instance().Submit( ThreadPool::s_NetworkContext, [ config, subreddit ]( )
        {
            RequestOptions options{ };
            options.m_CaBundle = config.CaBundle( );
            options.m_UserAgent = config.UserAgent( );
            options.m_Url = "https://www.reddit.com/r/" + subreddit + "/hot.json";

            RedditRequest request{ };
            RequestResult result = request.Perform( options );

            if( !result.m_Success )
            {
                WarningLog( "[%s] RedditRequest failed with error: %i", __FUNCTION__, static_cast< uint16_t >( result.m_Error.m_ErrorCode ) );
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
                InfoLog( "[%s] No content to download, exiting...", __FUNCTION__ );
                return;
            }

            InfoLog( "[%s] Starting downloading content, urls: %i", __FUNCTION__, urls.size( ) );

            // Create download directory
            const std::string folder = "output";
            const std::string dir = DownloadHelpers::CreateFolder( folder );
            if( dir.empty( ) )
            {
                ErrorLog( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                return;
            }

            // Download images
            for( const std::string& url : urls )
            {
                std::vector<char> buffer{ };

                DownloadOptions options{ };
                options.m_CaBundle = config.CaBundle( );
                options.m_Url = url;
                options.m_BufferPtr = &buffer;

                DownloadRequest request{ };
                DownloadResult result = request.Perform( options );
                if( !result.m_Success )
                {
                    ErrorLog( "[%s] Download failed for url: %s", __FUNCTION__, url.c_str( ) );
                    continue;
                }

                std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( options.m_Url );
                const std::string filepath = dir + "/" + filename;

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
        } );

    bool complete = false;
    while( !complete )
    {
        auto status = task.wait_for( std::chrono::milliseconds( 16 ) );
        complete = status == std::future_status::ready;
        if( complete )
        {
            InfoLog( "[%s] Task complete!", __FUNCTION__ );
        }
        else
        {
            InfoLog( "[%s] Waiting for task...", __FUNCTION__ );
        }
    }
}

bool ImageScraper::RedditService::HandleUrl( const Config& config, const std::string& url )
{
    // TODO Check url for reddit address
    if( true )
    {
        DownloadHotReddit( config, url );
    }

    return true;
}
