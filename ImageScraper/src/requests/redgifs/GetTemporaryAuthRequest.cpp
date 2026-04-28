#include "requests/redgifs/GetTemporaryAuthRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "log/Logger.h"

const std::string ImageScraper::Redgifs::GetTemporaryAuthRequest::s_BaseUrl = "https://api.redgifs.com/v2/auth/temporary";

ImageScraper::Redgifs::GetTemporaryAuthRequest::GetTemporaryAuthRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Redgifs::GetTemporaryAuthRequest::GetTemporaryAuthRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Redgifs::GetTemporaryAuthRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Redgifs::GetTemporaryAuthRequest started!", __FUNCTION__ );

    RequestResult result{ };

    HttpRequest request{ };
    request.m_Url = s_BaseUrl;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    const HttpResponse response = m_HttpClient->Get( request );
    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Redgifs::GetTemporaryAuthRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;
    result.m_Success = true;

    LogDebug( "[%s] Redgifs::GetTemporaryAuthRequest complete!", __FUNCTION__ );
    return result;
}
