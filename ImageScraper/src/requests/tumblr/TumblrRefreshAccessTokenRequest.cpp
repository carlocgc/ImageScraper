#include "requests/tumblr/TumblrRefreshAccessTokenRequest.h"
#include "requests/AuthRequestHelpers.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Tumblr::TumblrRefreshAccessTokenRequest::s_AuthUrl = "https://api.tumblr.com/v2/oauth2/token";
const std::string ImageScraper::Tumblr::TumblrRefreshAccessTokenRequest::s_AuthData = "grant_type=refresh_token";

ImageScraper::Tumblr::TumblrRefreshAccessTokenRequest::TumblrRefreshAccessTokenRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Tumblr::TumblrRefreshAccessTokenRequest::TumblrRefreshAccessTokenRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Tumblr::TumblrRefreshAccessTokenRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] TumblrRefreshAccessTokenRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( !AuthRequestHelpers::ValidateOAuthCredentials( options, result ) )
    {
        LogDebug( "[%s] TumblrRefreshAccessTokenRequest failed, credentials not provided.", __FUNCTION__ );
        return result;
    }

    // Tumblr uses body-based client credentials, not Basic auth
    const std::string tumblrBase = s_AuthData + "&client_id=" + options.m_ClientId + "&client_secret=" + options.m_ClientSecret;
    const std::string postData = AuthRequestHelpers::BuildQueryString( tumblrBase, options );

    HttpRequest request{ };
    request.m_Url = s_AuthUrl;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    request.m_Body = postData;
    request.m_Headers = {
        "Content-Type: application/x-www-form-urlencoded"
    };

    const HttpResponse response = m_HttpClient->Post( request, "refresh_token" );

    if( AuthRequestHelpers::HandleHttpError( response, result ) )
    {
        LogDebug( "[%s] TumblrRefreshAccessTokenRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsTumblrResponseError( result ) )
    {
        LogDebug( "[%s] TumblrRefreshAccessTokenRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] TumblrRefreshAccessTokenRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
