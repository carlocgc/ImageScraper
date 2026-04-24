#include "requests/mastodon/GetAccountStatusesRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "utils/MastodonUtils.h"
#include "utils/StringUtils.h"
#include "log/Logger.h"

#include <utility>
#include <vector>

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

    ImageScraper::RequestResult MakeBadRequest( const std::string& message )
    {
        ImageScraper::RequestResult result{ };
        result.m_Error.m_ErrorCode = ImageScraper::ResponseErrorCode::BadRequest;
        result.m_Error.m_ErrorString = message;
        return result;
    }
}

const std::string ImageScraper::Mastodon::GetAccountStatusesRequest::s_PathPrefix = "/api/v1/accounts/";
const std::string ImageScraper::Mastodon::GetAccountStatusesRequest::s_PathSuffix = "/statuses";

ImageScraper::Mastodon::GetAccountStatusesRequest::GetAccountStatusesRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Mastodon::GetAccountStatusesRequest::GetAccountStatusesRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Mastodon::GetAccountStatusesRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Mastodon::GetAccountStatusesRequest started!", __FUNCTION__ );

    const std::string instanceUrl = MastodonUtils::NormalizeInstanceUrl( options.m_UrlExt );
    if( instanceUrl.empty( ) )
    {
        LogDebug( "[%s] Mastodon::GetAccountStatusesRequest missing or invalid instance URL.", __FUNCTION__ );
        return MakeBadRequest( "Missing or invalid Mastodon instance URL" );
    }

    if( options.m_ResourceId.empty( ) )
    {
        LogDebug( "[%s] Mastodon::GetAccountStatusesRequest missing account ID.", __FUNCTION__ );
        return MakeBadRequest( "Missing Mastodon account ID" );
    }

    RequestResult result{ };

    HttpRequest request{ };
    request.m_Url = instanceUrl
        + s_PathPrefix
        + StringUtils::UrlEncode( options.m_ResourceId )
        + s_PathSuffix
        + CreateEncodedQueryParamString( options.m_QueryParams );
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;

    if( !options.m_AccessToken.empty( ) )
    {
        request.m_Headers = { "Authorization: Bearer " + options.m_AccessToken };
    }

    LogDebug( "[%s] Mastodon::GetAccountStatusesRequest, URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request );
    if( !response.m_Success )
    {
        result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
        result.m_Error.m_ErrorString = response.m_Error;
        LogDebug( "[%s] Mastodon::GetAccountStatusesRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Response = response.m_Body;
    if( DownloadHelpers::IsMastodonResponseError( result ) )
    {
        LogDebug( "[%s] Mastodon::GetAccountStatusesRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    result.m_Success = true;

    LogDebug( "[%s] Mastodon::GetAccountStatusesRequest complete!", __FUNCTION__ );
    return result;
}
