#include "RedditRequest.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

RedditRequest::RedditRequest( const Config& config, const std::string& endpoint ) : Request( config, endpoint )
{
    Configure( config );
}

void RedditRequest::Perform( )
{
    // Blocks
    m_Easy.perform( );
}

void RedditRequest::Configure( const Config& config )
{
    // Set the URL to retrieve
    m_Easy.setOpt( new curlpp::options::Url( m_EndPoint ) );

    // Follow redirects if any
    m_Easy.setOpt( new curlpp::options::FollowLocation( true ) );

    // Set user agent
    m_Easy.setOpt( new curlpp::options::UserAgent( m_UserAgent.c_str( ) ) );

    // Set the output stream for the response
    m_Easy.setOpt( new curlpp::options::WriteStream( &m_Response ) );

    const std::string certFileName = config.CaBundle();
    const std::filesystem::path exe_path = std::filesystem::current_path( );
    const std::filesystem::path cert_path = exe_path / certFileName.c_str( );

    m_Easy.setOpt( new curlpp::options::CaInfo( cert_path.generic_string( ) ) );
}
