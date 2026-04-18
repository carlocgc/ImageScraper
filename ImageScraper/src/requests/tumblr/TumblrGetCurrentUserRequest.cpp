#include "requests/tumblr/TumblrGetCurrentUserRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Tumblr::TumblrGetCurrentUserRequest::s_Url = "https://api.tumblr.com/v2/user/info";

ImageScraper::Tumblr::TumblrGetCurrentUserRequest::TumblrGetCurrentUserRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Tumblr::TumblrGetCurrentUserRequest::TumblrGetCurrentUserRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Tumblr::TumblrGetCurrentUserRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] TumblrGetCurrentUserRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_AccessToken.empty( ) )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] TumblrGetCurrentUserRequest failed, access token not provided.", __FUNCTION__ );
        return result;
    }

    HttpRequest request{ };
    request.m_Url       = s_Url;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle  = options.m_CaBundle;
    request.m_Headers   = { "Authorization: Bearer " + options.m_AccessToken };

    const HttpResponse response = m_HttpClient->Get( request );

    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] TumblrGetCurrentUserRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsTumblrResponseError( result ) )
    {
        LogDebug( "[%s] TumblrGetCurrentUserRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] TumblrGetCurrentUserRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
