#include "requests/fourchan/GetThreadsRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::FourChan::GetThreadsRequest::s_BaseUrl = "https://a.4cdn.org/";

ImageScraper::FourChan::GetThreadsRequest::GetThreadsRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::FourChan::GetThreadsRequest::GetThreadsRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::FourChan::GetThreadsRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] FourChan::GetThreadsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    const std::string url = s_BaseUrl + options.m_UrlExt;

    HttpRequest request{ };
    request.m_Url = url;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    LogDebug( "[%s] FourChan::GetThreadsRequest, URL: %s", __FUNCTION__, url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request, "get_threads" );

    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] FourChan::GetThreadsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsFourChanResponseError( result ) )
    {
        LogDebug( "[%s] FourChan::GetThreadsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] FourChan::GetThreadsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
