#include "requests/reddit/RevokeAccessTokenRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "log/Logger.h"
#include "cppcodec/base64_rfc4648.hpp"

const std::string ImageScraper::Reddit::RevokeAccessTokenRequest::s_RevokeUrl = "https://www.reddit.com/api/v1/revoke_token";

ImageScraper::Reddit::RevokeAccessTokenRequest::RevokeAccessTokenRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Reddit::RevokeAccessTokenRequest::RevokeAccessTokenRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Reddit::RevokeAccessTokenRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] RevokeAccessTokenRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_ClientId.empty( ) )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] RevokeAccessTokenRequest failed, Client ID not provided.", __FUNCTION__ );
        return result;
    }

    if( options.m_ClientSecret.empty( ) )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] RevokeAccessTokenRequest failed, Client Secret not provided.", __FUNCTION__ );
        return result;
    }

    if( options.m_AccessToken.empty( ) )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] RevokeAccessTokenRequest failed, token not provided.", __FUNCTION__ );
        return result;
    }

    const std::string credentials = options.m_ClientId + ":" + options.m_ClientSecret;
    const std::string encodedCredentials = cppcodec::base64_rfc4648::encode( credentials );

    HttpRequest request{ };
    request.m_Url = s_RevokeUrl;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    request.m_Body = "token=" + options.m_AccessToken + "&token_type_hint=refresh_token";
    request.m_Headers = {
        "Content-Type: application/x-www-form-urlencoded",
        "Authorization: Basic " + encodedCredentials
    };

    const HttpResponse response = m_HttpClient->Post( request, "revoke_token" );

    // Reddit returns 204 No Content on success - no body to parse
    if( response.m_StatusCode == 204 || response.m_Success )
    {
        LogDebug( "[%s] RevokeAccessTokenRequest complete!", __FUNCTION__ );
        result.m_Success = true;
        return result;
    }

    result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
    result.m_Error.m_ErrorString = response.m_Error;
    LogDebug( "[%s] RevokeAccessTokenRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
    return result;
}
