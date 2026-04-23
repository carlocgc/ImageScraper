#include "requests/tumblr/RetrievePublishedPostsRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Tumblr::RetrievePublishedPostsRequest::s_BaseUrl = "https://api.tumblr.com/v2/blog/";

ImageScraper::Tumblr::RetrievePublishedPostsRequest::RetrievePublishedPostsRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Tumblr::RetrievePublishedPostsRequest::RetrievePublishedPostsRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Tumblr::RetrievePublishedPostsRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Tumblr::RetrievePublishedPostsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    const std::string url = s_BaseUrl + options.m_UrlExt + DownloadHelpers::CreateQueryParamString( options.m_QueryParams );

    HttpRequest request{ };
    request.m_Url = url;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    if( !options.m_AccessToken.empty( ) )
    {
        request.m_Headers = { "Authorization: Bearer " + options.m_AccessToken };
    }

    LogDebug( "[%s] Tumblr::RetrievePublishedPostsRequest, URL: %s", __FUNCTION__, url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request );

    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Tumblr::RetrievePublishedPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsTumblrResponseError( result ) )
    {
        LogDebug( "[%s] Tumblr::RetrievePublishedPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] Tumblr::RetrievePublishedPostsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
