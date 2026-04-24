#include "requests/mastodon/SearchAccountsRequest.h"

#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "requests/mastodon/MastodonRequestHelpers.h"
#include "utils/MastodonUtils.h"
#include "log/Logger.h"

#include <utility>
#include <vector>

const std::string ImageScraper::Mastodon::SearchAccountsRequest::s_Path = "/api/v2/search";

ImageScraper::Mastodon::SearchAccountsRequest::SearchAccountsRequest( )
    : m_HttpClient( std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) )
{
}

ImageScraper::Mastodon::SearchAccountsRequest::SearchAccountsRequest( std::shared_ptr<IHttpClient> client )
    : m_HttpClient( std::move( client ) )
{
}

ImageScraper::RequestResult ImageScraper::Mastodon::SearchAccountsRequest::Perform( const RequestOptions& options )
{
    LogDebug( "[%s] Mastodon::SearchAccountsRequest started!", __FUNCTION__ );

    const std::string instanceUrl = MastodonUtils::NormalizeInstanceUrl( options.m_UrlExt );
    if( instanceUrl.empty( ) )
    {
        LogDebug( "[%s] Mastodon::SearchAccountsRequest missing or invalid instance URL.", __FUNCTION__ );
        return RequestHelpers::MakeBadRequest( "Missing or invalid Mastodon instance URL" );
    }

    if( !RequestHelpers::HasQueryParam( options.m_QueryParams, "q" ) )
    {
        LogDebug( "[%s] Mastodon::SearchAccountsRequest missing query parameter: q", __FUNCTION__ );
        return RequestHelpers::MakeBadRequest( "Missing Mastodon account search query" );
    }

    std::vector<QueryParam> queryParams = options.m_QueryParams;
    if( !RequestHelpers::HasQueryParam( queryParams, "type" ) )
    {
        queryParams.push_back( { "type", "accounts" } );
    }

    HttpRequest request{ };
    request.m_Url = instanceUrl + s_Path + RequestHelpers::CreateEncodedQueryParamString( queryParams );
    request.m_UserAgent = options.m_UserAgent;
    request.m_CaBundle = options.m_CaBundle;
    RequestHelpers::ApplyBearerAuthHeader( request, options.m_AccessToken );

    LogDebug( "[%s] Mastodon::SearchAccountsRequest, URL: %s", __FUNCTION__, request.m_Url.c_str( ) );

    const HttpResponse response = m_HttpClient->Get( request );
    const RequestResult result = RequestHelpers::CreateResultFromResponse( response );
    if( !result.m_Success )
    {
        LogDebug( "[%s] Mastodon::SearchAccountsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    LogDebug( "[%s] Mastodon::SearchAccountsRequest complete!", __FUNCTION__ );
    return result;
}
