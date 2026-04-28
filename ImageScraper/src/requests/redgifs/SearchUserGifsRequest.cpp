#include "requests/redgifs/SearchUserGifsRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

const std::string ImageScraper::Redgifs::SearchUserGifsRequest::s_BaseUrlPrefix = "https://api.redgifs.com/v2/users/";
const std::string ImageScraper::Redgifs::SearchUserGifsRequest::s_BaseUrlSuffix = "/search";

ImageScraper::Redgifs::SearchUserGifsRequest::SearchUserGifsRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Redgifs::SearchUserGifsRequest::SearchUserGifsRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Redgifs::SearchUserGifsRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Redgifs::SearchUserGifsRequest started for user: %s", __FUNCTION__, options.m_UrlExt.c_str( ) );

    RequestResult result{ };

    if( options.m_UrlExt.empty( ) )
    {
        result.SetError( ResponseErrorCode::BadRequest );
        return result;
    }

    HttpRequest request{ };
    request.m_Url = s_BaseUrlPrefix + options.m_UrlExt + s_BaseUrlSuffix + DownloadHelpers::CreateQueryParamString( options.m_QueryParams );
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    if( !options.m_AccessToken.empty( ) )
    {
        request.m_Headers.push_back( "Authorization: Bearer " + options.m_AccessToken );
    }

    LogDebug( "[%s] URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request );
    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Redgifs::SearchUserGifsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;
    result.m_Success = true;

    LogDebug( "[%s] Redgifs::SearchUserGifsRequest complete!", __FUNCTION__ );
    return result;
}
