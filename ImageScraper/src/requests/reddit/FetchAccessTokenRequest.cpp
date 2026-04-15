#include "requests/reddit/FetchAccessTokenRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "cppcodec/base64_rfc4648.hpp"

const std::string ImageScraper::Reddit::FetchAccessTokenRequest::s_AuthUrl = "https://www.reddit.com/api/v1/access_token";
const std::string ImageScraper::Reddit::FetchAccessTokenRequest::s_AuthData = "grant_type=authorization_code";

ImageScraper::Reddit::FetchAccessTokenRequest::FetchAccessTokenRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Reddit::FetchAccessTokenRequest::FetchAccessTokenRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Reddit::FetchAccessTokenRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] FetchAccessTokenRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_ClientId == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        DebugLog( "[%s] FetchAccessTokenRequest failed, Client ID not provided.", __FUNCTION__ );
        return result;
    }

    if( options.m_ClientSecret == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        DebugLog( "[%s] FetchAccessTokenRequest failed, Client Secret not provided.", __FUNCTION__ );
        return result;
    }

    std::string postData = s_AuthData;
    for( const auto& [key, value] : options.m_QueryParams )
    {
        postData += "&" + key + "=" + value;
    }

    const std::string authCredentials = options.m_ClientId + ":" + options.m_ClientSecret;
    const std::string authHeaderValue = "Basic " + cppcodec::base64_rfc4648::encode( authCredentials );

    HttpRequest request{ };
    request.m_Url = s_AuthUrl;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    request.m_Body = postData;
    request.m_Headers = {
        "Authorization: " + authHeaderValue,
        "Content-Type: application/x-www-form-urlencoded"
    };

    const HttpResponse response = m_HttpClient->Post( request );

    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        DebugLog( "[%s] FetchAccessTokenRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsRedditResponseError( result ) )
    {
        DebugLog( "[%s] FetchAccessTokenRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] FetchAccessTokenRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
