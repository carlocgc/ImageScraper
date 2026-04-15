#include "requests/fourchan/GetBoardsRequest.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::FourChan::GetBoardsRequest::s_BaseUrl = "https://a.4cdn.org/boards.json";

ImageScraper::FourChan::GetBoardsRequest::GetBoardsRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::FourChan::GetBoardsRequest::GetBoardsRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::FourChan::GetBoardsRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] FourChan::GetBoardsRequest started!", __FUNCTION__ );

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
        DebugLog( "[%s] FourChan::GetBoardsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;

    if( DownloadHelpers::IsFourChanResponseError( result ) )
    {
        DebugLog( "[%s] FourChan::GetBoardsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] FourChan::GetBoardsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
