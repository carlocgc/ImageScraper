#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <filesystem>
#include <string>
#include "Config.h"
#include "Logger.h"
#include "RedditRequest.h"


int main( int argc, char* argv[ ] )
{
    const std::filesystem::path configPath = std::filesystem::current_path( ) / "config.json";
    const std::string configPathString = configPath.generic_string( );
    Config config{ configPathString };

    const std::string subreddit = "programming";
    const std::string endpoint = "https://www.reddit.com/r/" + subreddit + "/hot.json";

    RedditRequest request{ config, endpoint };

    request.Perform( );

    // Print the response
    const std::string responseStr{ request.Response() };
    InfoLog( "[%s] Response: %s", __FUNCTION__, responseStr.c_str( ) );

    return 0;
}