#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "utils/DownloadUtils.h"
#include "requests/RequestTypes.h"
#include "requests/reddit/FetchSubredditPostsRequest.h"

const std::string ImageScraper::Reddit::FetchSubredditPostsRequest::s_BaseUrl = "https://www.reddit.com/r/";
const std::string ImageScraper::Reddit::FetchSubredditPostsRequest::s_AuthBaseUrl = "https://oauth.reddit.com/r/";

ImageScraper::RequestResult ImageScraper::Reddit::FetchSubredditPostsRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] FetchSubredditPostsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };
        std::ostringstream response;

        std::string url{ };

        std::string urlEnd{ };
        urlEnd += options.m_UrlExt;
        urlEnd += DownloadHelpers::CreateQueryParamString( options.m_QueryParams );

        if( options.m_AccessToken != "" )
        {
            url = s_AuthBaseUrl + urlEnd;
            std::string accessToken = options.m_AccessToken;
            std::string authHeader = "Authorization: Bearer " + accessToken;
            request.setOpt<curlpp::options::HttpHeader>( std::list<std::string>( { authHeader } ) );
        }
        else
        {
            url = s_BaseUrl + urlEnd;
        }

        // Set the URL to retrieve
        request.setOpt( new curlpp::options::Url( url ) );

        // Follow redirects if any
        request.setOpt( new curlpp::options::FollowLocation( true ) );

        // Set user agent
        request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );

        // Set the output stream for the response
        request.setOpt( new curlpp::options::WriteStream( &response ) );

        // Set the cert bundle for TLS/HTTPS
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );

        request.setOpt( new curlpp::options::Verbose( true ) );

        DebugLog( "[%s] FetchSubredditPostsRequest, URL: %s", __FUNCTION__, url.c_str() );

        // Perform the request, blocks thread
        request.perform( );

        result.m_Response = response.str( );
    }
    catch( curlpp::RuntimeError& error )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] FetchSubredditPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }
    catch( curlpp::LogicError& error )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] FetchSubredditPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str() );
        return result;
    }

    if( DownloadHelpers::IsRedditResponseError( result ) )
    {
        DebugLog( "[%s] FetchSubredditPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] FetchSubredditPostsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}