#include "requests/redgifs/GetGifRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "log/Logger.h"

const std::string ImageScraper::Redgifs::GetGifRequest::s_BaseUrl = "https://api.redgifs.com/v2/gifs/";

ImageScraper::Redgifs::GetGifRequest::GetGifRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Redgifs::GetGifRequest::GetGifRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Redgifs::GetGifRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Redgifs::GetGifRequest started for slug: %s", __FUNCTION__, options.m_UrlExt.c_str( ) );

    RequestResult result{ };

    if( options.m_UrlExt.empty( ) )
    {
        result.SetError( ResponseErrorCode::BadRequest );
        return result;
    }

    HttpRequest request{ };
    request.m_Url = s_BaseUrl + options.m_UrlExt;
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    if( !options.m_AccessToken.empty( ) )
    {
        request.m_Headers.push_back( "Authorization: Bearer " + options.m_AccessToken );
    }

    const HttpResponse response = m_HttpClient->Get( request, "get_gif" );
    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Redgifs::GetGifRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;
    result.m_Success = true;

    LogDebug( "[%s] Redgifs::GetGifRequest complete!", __FUNCTION__ );
    return result;
}
