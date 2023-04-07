/*
Project Name: ImageScraper
Author: Carlo Mongiello
Date: 2023

Description:
The purpose of this program is to scrape media content from a website URL, including social media feeds, and store it locally.
The program allows for asynchronous downloading of all media at the specified URL, with a graphical user interface (GUI) that allows the user to input the URL and configure options.
The program also displays log output to the user.
*/

#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <filesystem>
#include <string>
#include "Config.h"
#include "Logger.h"
#include "requests/RedditRequest.h"
#include "requests/DownloadRequest.h"
#include "parsers/RedditParser.h"
#include "utils/DownloadUtils.h"
#include "asyncgc/ThreadPool.h"

using namespace nlohmann;


void DownloadHotReddit( const Config& config )
{
    Asyncgc::ThreadPool threadPool{ 3 };

    Asyncgc::ThreadContext context = 0;

    auto task = threadPool.Submit( context, [ config ]( )
        {
            // Create request for subreddit post data
            const std::string subreddit = "BravelyDefault";

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
                RedditParser::GetImageUrlFromRedditPost( post, urls );
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



int main( int argc, char* argv[ ] )
{
    Config config{ };

    DownloadHotReddit( config );

    return 0;
}