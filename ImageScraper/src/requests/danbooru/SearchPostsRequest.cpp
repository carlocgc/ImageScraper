#include "requests/danbooru/SearchPostsRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/StringUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Danbooru::SearchPostsRequest::s_BaseUrl = "https://danbooru.donmai.us/posts.json";

namespace
{
    std::string CreateEncodedQueryParamString( const std::vector<ImageScraper::QueryParam>& params )
    {
        std::string paramString{ };
        if( !params.empty( ) )
        {
            paramString += '?';
        }

        int paramCount = 0;
        for( const ImageScraper::QueryParam& param : params )
        {
            if( paramCount++ > 0 )
            {
                paramString += '&';
            }

            paramString += ImageScraper::StringUtils::UrlEncode( param.m_Key );
            paramString += '=';
            paramString += ImageScraper::StringUtils::UrlEncode( param.m_Value );
        }

        return paramString;
    }
}

ImageScraper::Danbooru::SearchPostsRequest::SearchPostsRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Danbooru::SearchPostsRequest::SearchPostsRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Danbooru::SearchPostsRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Danbooru::SearchPostsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    HttpRequest request{ };
    request.m_Url = s_BaseUrl + CreateEncodedQueryParamString( options.m_QueryParams );
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    LogDebug( "[%s] Danbooru::SearchPostsRequest, URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request, "search_posts" );
    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Danbooru::SearchPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;
    result.m_Success = true;
    LogDebug( "[%s] Danbooru::SearchPostsRequest complete!", __FUNCTION__ );
    return result;
}
