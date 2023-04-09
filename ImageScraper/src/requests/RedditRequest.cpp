#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "utils/DownloadUtils.h"
#include "requests/RequestTypes.h"
#include "requests/RedditRequest.h"

ImageScraper::RequestResult ImageScraper::RedditRequest::Perform( const RequestOptions& options )
{
    curlpp::Cleanup cleanup{ };
    curlpp::Easy request{ };
    std::ostringstream response;

    // Set the URL to retrieve
    request.setOpt( new curlpp::options::Url( options.m_Url ) );

    // Follow redirects if any
    request.setOpt( new curlpp::options::FollowLocation( true ) );

    // Set user agent
    request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );

    // Set the output stream for the response
    request.setOpt( new curlpp::options::WriteStream( &response ) );

    // Set the cert bundle for TLS/HTTPS
    request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );

    // Perform the request, blocks thread
    request.perform( );

    RequestResult result{ };
    result.m_Response = response.str( );

    if( DownloadHelpers::IsResponseError( result ) )
    {
        WarningLog( "[%s] Failed with error: %i", __FUNCTION__, static_cast< uint16_t >( result.m_Error.m_ErrorCode ) );
    }
    else
    {
        InfoLog( "[%s] Completed successfully!", __FUNCTION__ );
        result.m_Success = true;
    }

    return result;
}