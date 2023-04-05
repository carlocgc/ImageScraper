#include "RedditRequest.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

RedditRequest::RedditRequest( const Config& config, const std::string& endpoint )
    : Request( config, endpoint )
{
    Configure( config );
}

bool RedditRequest::Perform( )
{
    // Blocks
    m_Easy.perform( );

    return true;
}

bool RedditRequest::Configure( const Config& config )
{
    // Set the URL to retrieve
    m_Easy.setOpt( new curlpp::options::Url( m_EndPoint ) );

    // Follow redirects if any
    m_Easy.setOpt( new curlpp::options::FollowLocation( true ) );

    // Set user agent
    m_Easy.setOpt( new curlpp::options::UserAgent( m_UserAgent ) );

    // Set the output stream for the response
    m_Easy.setOpt( new curlpp::options::WriteStream( &m_Response ) );

    // Set the cert bundle for TLS/HTTPS
    m_Easy.setOpt( new curlpp::options::CaInfo( m_CaBundle ) );

    return true;
}
