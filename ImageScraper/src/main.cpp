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

using namespace nlohmann;

int main( int argc, char* argv[ ] )
{
    Config config{ };

    const std::string subreddit = "CrappyArt";

    RequestOptions options{ };
    options.m_CaBundle = config.CaBundle( );
    options.m_UserAgent = config.UserAgent( );
    options.m_Url = "https://www.reddit.com/r/" + subreddit + "/hot.json";

    RedditRequest request{ };
    RequestResult result = request.Perform( options );

    if( !result.m_Success )
    {
        WarningLog( "[%s] RedditRequest failed with error: %i", __FUNCTION__, static_cast<uint16_t>( result.m_Error.m_ErrorCode ) );
        return 1;
    }

    InfoLog( "[%s] Response: %s", __FUNCTION__, result.m_Response.c_str() );

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
        return 1;
    }

    for( const std::string& url : urls )
    {
        RequestOptions options{ };
        options.m_CaBundle = config.CaBundle( );
        options.m_UserAgent = config.UserAgent( );
        options.m_Url = url;

        DownloadRequest request{ };

        RequestResult result = request.Perform( options );
        if( result.m_Success )
        {
            InfoLog( "[%s] Download complete: %s", __FUNCTION__, result.m_Response.c_str( ) );
        }
        else
        {
            WarningLog( "[%s] Download failed with error: %i", __FUNCTION__, static_cast< uint16_t >( result.m_Error.m_ErrorCode ) );
        }
    }

    return 0;
}