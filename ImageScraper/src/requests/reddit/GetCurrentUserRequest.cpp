#include "requests/reddit/GetCurrentUserRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Reddit::GetCurrentUserRequest::s_Url = "https://oauth.reddit.com/api/v1/me";

ImageScraper::Reddit::GetCurrentUserRequest::GetCurrentUserRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Reddit::GetCurrentUserRequest::GetCurrentUserRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Reddit::GetCurrentUserRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] GetCurrentUserRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_AccessToken.empty( ) )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        DebugLog( "[%s] GetCurrentUserRequest failed, access token not provided.", __FUNCTION__ );
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
        DebugLog( "[%s] GetCurrentUserRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsRedditResponseError( result ) )
    {
        DebugLog( "[%s] GetCurrentUserRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] GetCurrentUserRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
