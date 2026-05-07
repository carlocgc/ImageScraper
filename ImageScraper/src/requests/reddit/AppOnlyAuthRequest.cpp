#include "requests/reddit/AppOnlyAuthRequest.h"
#include "requests/AuthRequestHelpers.h"
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

    if( !AuthRequestHelpers::ValidateOAuthCredentials( options, result ) )
    {
        LogDebug( "[%s] AppOnlyAuthRequest failed, credentials not provided.", __FUNCTION__ );
        return result;
    }

    const std::string encodedCredentials = cppcodec::base64_rfc4648::encode( options.m_ClientId + ":" + options.m_ClientSecret );

    HttpRequest request{ };
    request.m_Url = s_AuthUrl;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    request.m_Body = s_AuthData;
    request.m_Headers = {
        "Content-Type: application/x-www-form-urlencoded",
        "Authorization: Basic " + encodedCredentials
    };

    const HttpResponse response = m_HttpClient->Post( request, "app_only_auth" );

    if( AuthRequestHelpers::HandleHttpError( response, result ) )
    {
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
