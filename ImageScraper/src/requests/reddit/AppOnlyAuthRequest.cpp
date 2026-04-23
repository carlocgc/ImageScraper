#include "requests/reddit/AppOnlyAuthRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "cppcodec/base64_rfc4648.hpp"

const std::string ImageScraper::Reddit::AppOnlyAuthRequest::s_AuthUrl = "https://www.reddit.com/api/v1/access_token";
const std::string ImageScraper::Reddit::AppOnlyAuthRequest::s_AuthData = "grant_type=client_credentials";

ImageScraper::Reddit::AppOnlyAuthRequest::AppOnlyAuthRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Reddit::AppOnlyAuthRequest::AppOnlyAuthRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Reddit::AppOnlyAuthRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] AppOnlyAuthRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_ClientId == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] AppOnlyAuthRequest failed, Client ID not provided.", __FUNCTION__ );
        return result;
    }

    if( options.m_ClientSecret == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] AppOnlyAuthRequest failed, Client Secret not provided.", __FUNCTION__ );
        return result;
    }

    const std::string credentials = options.m_ClientId + ":" + options.m_ClientSecret;
    const std::string encodedCredentials = cppcodec::base64_rfc4648::encode( credentials );

    HttpRequest request{ };
    request.m_Url = s_AuthUrl;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    request.m_Body = s_AuthData;
    request.m_Headers = {
        "Content-Type: application/x-www-form-urlencoded",
        "Authorization: Basic " + encodedCredentials
    };

    const HttpResponse response = m_HttpClient->Post( request );

    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] AppOnlyAuthRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsRedditResponseError( result ) )
    {
        LogDebug( "[%s] AppOnlyAuthRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] AppOnlyAuthRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
