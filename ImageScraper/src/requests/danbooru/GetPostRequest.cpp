#include "requests/danbooru/GetPostRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/StringUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Danbooru::GetPostRequest::s_BaseUrl = "https://danbooru.donmai.us/posts/";

ImageScraper::Danbooru::GetPostRequest::GetPostRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Danbooru::GetPostRequest::GetPostRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Danbooru::GetPostRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Danbooru::GetPostRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_ResourceId.empty( ) )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCode::BadRequest;
        result.m_Error.m_ErrorString = "Danbooru post id is required.";
        return result;
    }

    HttpRequest request{ };
    request.m_Url = s_BaseUrl + StringUtils::UrlEncode( options.m_ResourceId ) + ".json";
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    LogDebug( "[%s] Danbooru::GetPostRequest, URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request, "get_post" );
    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Danbooru::GetPostRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;
    result.m_Success = true;
    LogDebug( "[%s] Danbooru::GetPostRequest complete!", __FUNCTION__ );
    return result;
}
