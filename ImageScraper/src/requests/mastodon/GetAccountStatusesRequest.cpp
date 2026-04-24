#include "requests/mastodon/GetAccountStatusesRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "requests/mastodon/MastodonRequestHelpers.h"
#include "utils/MastodonUtils.h"
#include "utils/StringUtils.h"
#include "log/Logger.h"

#include <utility>

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
        return RequestHelpers::MakeBadRequest( "Missing or invalid Mastodon instance URL" );
    }

    if( options.m_ResourceId.empty( ) )
    {
        LogDebug( "[%s] Mastodon::GetAccountStatusesRequest missing account ID.", __FUNCTION__ );
        return RequestHelpers::MakeBadRequest( "Missing Mastodon account ID" );
    }

    HttpRequest request{ };
    request.m_Url = instanceUrl
        + s_PathPrefix
        + StringUtils::UrlEncode( options.m_ResourceId )
        + s_PathSuffix
        + RequestHelpers::CreateEncodedQueryParamString( options.m_QueryParams );
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    RequestHelpers::ApplyBearerAuthHeader( request, options.m_AccessToken );

    LogDebug( "[%s] Mastodon::GetAccountStatusesRequest, URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request );
    const RequestResult result = RequestHelpers::CreateResultFromResponse( response );
    if( !result.m_Success )
    {
        LogDebug( "[%s] Mastodon::GetAccountStatusesRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] Mastodon::GetAccountStatusesRequest complete!", __FUNCTION__ );
    return result;
}
