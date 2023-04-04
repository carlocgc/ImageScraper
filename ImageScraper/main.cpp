#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <filesystem>
#include <string>
#include "Config.h"
#include "Logger.h"


int main( int argc, char* argv[ ] )
{
    const std::filesystem::path configPath = std::filesystem::current_path( ).parent_path( ) / "config.json";
    const std::string configPathString = configPath.generic_string( );
    Config conf{ configPathString };

    const std::string certFileName = conf.GetValue<std::string>( "CertBundle" );
    const std::filesystem::path exe_path = std::filesystem::path( argv[ 0 ] ).parent_path( );
    const std::filesystem::path cert_path = exe_path / certFileName.c_str( );

    const std::string user_agent = conf.GetValue<std::string>( "UserAgent" );

    std::string subreddit = "programming";
    std::string endpoint = "https://www.reddit.com/r/" + subreddit + "/hot.json";
    curlpp::Cleanup cleanup;
    curlpp::Easy request;
    std::ostringstream response;

    // Set the URL to retrieve
    request.setOpt( new curlpp::options::Url( endpoint ) );

    // Follow redirects if any
    request.setOpt( new curlpp::options::FollowLocation( true ) );

    // Set user agent
    request.setOpt( new curlpp::options::UserAgent( user_agent.c_str( ) ) );

    // Set the output stream for the response
    request.setOpt( new curlpp::options::WriteStream( &response ) );

    request.setOpt( new curlpp::options::CaInfo( cert_path.generic_string( ) ) );

    // Perform the request and get the response
    request.perform( );

    // Print the response
    const std::string responseStr{ response.str( ) };
    InfoLog( "[%s] Response: %s", __FUNCTION__, responseStr.c_str( ) );

    return 0;
}