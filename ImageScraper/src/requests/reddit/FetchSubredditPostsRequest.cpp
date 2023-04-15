#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "utils/DownloadUtils.h"
#include "requests/RequestTypes.h"
#include "requests/reddit/FetchSubredditPostsRequest.h"

ImageScraper::RequestResult ImageScraper::Reddit::FetchSubredditPostsRequest::Perform( const RequestOptions& options )
{
    RequestResult result{ };

    try
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

        result.m_Response = response.str( );
    }
    catch( curlpp::RuntimeError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        return result;
    }
    catch( curlpp::LogicError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        return result;
    }

    if( DownloadHelpers::IsResponseError( result ) )
    {
        ErrorLog( "[%s] Failed to fetch subreddit post data, error: %i", __FUNCTION__, static_cast< uint16_t >( result.m_Error.m_ErrorCode ) );
        return result;
    }

    InfoLog( "[%s] Fetched subreddit post data successfully!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}