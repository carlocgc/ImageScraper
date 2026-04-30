#include "requests/bluesky/GetAuthorFeedRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Bluesky::GetAuthorFeedRequest::s_BaseUrl = "https://public.api.bsky.app/xrpc/app.bsky.feed.getAuthorFeed";

ImageScraper::Bluesky::GetAuthorFeedRequest::GetAuthorFeedRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Bluesky::GetAuthorFeedRequest::GetAuthorFeedRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Bluesky::GetAuthorFeedRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Bluesky::GetAuthorFeedRequest started!", __FUNCTION__ );

    RequestResult result{ };

    HttpRequest request{ };
    request.m_Url = s_BaseUrl + DownloadHelpers::CreateQueryParamString( options.m_QueryParams );
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    LogDebug( "[%s] Bluesky::GetAuthorFeedRequest, URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request, "get_author_feed" );
    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Bluesky::GetAuthorFeedRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;
    result.m_Success = true;

    LogDebug( "[%s] Bluesky::GetAuthorFeedRequest complete!", __FUNCTION__ );
    return result;
}
