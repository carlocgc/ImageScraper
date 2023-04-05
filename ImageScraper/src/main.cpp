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