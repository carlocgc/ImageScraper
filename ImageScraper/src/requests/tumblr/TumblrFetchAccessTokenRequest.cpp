#include "requests/tumblr/TumblrFetchAccessTokenRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Tumblr::TumblrFetchAccessTokenRequest::s_AuthUrl = "https://api.tumblr.com/v2/oauth2/token";
const std::string ImageScraper::Tumblr::TumblrFetchAccessTokenRequest::s_AuthData = "grant_type=authorization_code";

ImageScraper::Tumblr::TumblrFetchAccessTokenRequest::TumblrFetchAccessTokenRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Tumblr::TumblrFetchAccessTokenRequest::TumblrFetchAccessTokenRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Tumblr::TumblrFetchAccessTokenRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] TumblrFetchAccessTokenRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_ClientId == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] TumblrFetchAccessTokenRequest failed, Client ID not provided.", __FUNCTION__ );
        return result;
    }

    if( options.m_ClientSecret == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] TumblrFetchAccessTokenRequest failed, Client Secret not provided.", __FUNCTION__ );
        return result;
    }

    // Tumblr uses body-based client credentials, not Basic auth
    std::string postData = s_AuthData;
    postData += "&client_id=" + options.m_ClientId;
    postData += "&client_secret=" + options.m_ClientSecret;
    for( const auto& [key, value] : options.m_QueryParams )
    {
        postData += "&" + key + "=" + value;
    }

    HttpRequest request{ };
    request.m_Url = s_AuthUrl;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    request.m_Body = postData;
    request.m_Headers = {
        "Content-Type: application/x-www-form-urlencoded"
    };

    const HttpResponse response = m_HttpClient->Post( request, "fetch_token" );

    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] TumblrFetchAccessTokenRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsTumblrResponseError( result ) )
    {
        LogDebug( "[%s] TumblrFetchAccessTokenRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    // Verify access_token is present in the response
    try
    {
        const auto payload = nlohmann::json::parse( result.m_Response );
        if( !payload.contains( "access_token" ) )
        {
            result.SetError( ResponseErrorCode::InternalServerError );
            LogDebug( "[%s] TumblrFetchAccessTokenRequest failed, response missing access_token.", __FUNCTION__ );
            return result;
        }
    }
    catch( const nlohmann::json::exception& ex )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] TumblrFetchAccessTokenRequest failed, JSON parse error: %s", __FUNCTION__, ex.what( ) );
        return result;
    }

    LogDebug( "[%s] TumblrFetchAccessTokenRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
