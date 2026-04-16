#include "requests/reddit/FetchSubredditPostsRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "requests/RequestTypes.h"
#include "log/Logger.h"

const std::string ImageScraper::Reddit::FetchSubredditPostsRequest::s_BaseUrl = "https://www.reddit.com/r/";
const std::string ImageScraper::Reddit::FetchSubredditPostsRequest::s_AuthBaseUrl = "https://oauth.reddit.com/r/";

ImageScraper::Reddit::FetchSubredditPostsRequest::FetchSubredditPostsRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Reddit::FetchSubredditPostsRequest::FetchSubredditPostsRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Reddit::FetchSubredditPostsRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] FetchSubredditPostsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    const std::string urlEnd = options.m_UrlExt + DownloadHelpers::CreateQueryParamString( options.m_QueryParams );

    HttpRequest request{ };
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    if( options.m_AccessToken != "" )
    {
        request.m_Url = s_AuthBaseUrl + urlEnd;
        request.m_Headers = { "Authorization: Bearer " + options.m_AccessToken };
    }
    else
    {
        request.m_Url = s_BaseUrl + urlEnd;
    }

    LogDebug( "[%s] FetchSubredditPostsRequest, URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request );

    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] FetchSubredditPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsRedditResponseError( result ) )
    {
        LogDebug( "[%s] FetchSubredditPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] FetchSubredditPostsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
