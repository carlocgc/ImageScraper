/*
Project Name: ImageScraper
Author: Carlo Mongiello
Date: 2023

Description:
The purpose of this program is to scrape media content from a website URL,including social media feeds, and store it locally.
The program allows for asynchronous downloading of all media at the specified URL,with a graphical user interface (GUI) that allows the user to input the URL and configure options.
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
#include "RedditRequest.h"
#include "DownloadRequest.h"

int main( int argc, char* argv[ ] )
{
    Config config{ };

    /*
    const std::string subreddit = "CrappyArt";
    const std::string endpoint = "https://www.reddit.com/r/" + subreddit + "/hot.json";

    RedditRequest request{ config, endpoint };

    request.Perform( );

    // Print the response
    const std::string responseStr{ request.Response() };
    InfoLog( "[%s] Response: %s", __FUNCTION__, responseStr.c_str( ) );
    */

    const std::string url = "https://www.boredpanda.com/blog/wp-content/uploads/2017/10/59d490df85312_lpj7hm6t9zly__700.jpg";

    DownloadRequest request{ config, url };
    request.Perform( );


    return 0;
}