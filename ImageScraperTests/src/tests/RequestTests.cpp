#include "catch2/catch_amalgamated.hpp"
#include "mocks/MockHttpClient.h"

#include "requests/bluesky/GetAuthorFeedRequest.h"
#include "requests/bluesky/ResolveHandleRequest.h"
#include "requests/fourchan/GetBoardsRequest.h"
#include "requests/fourchan/GetThreadsRequest.h"
#include "requests/mastodon/GetAccountStatusesRequest.h"
#include "requests/mastodon/SearchAccountsRequest.h"
#include "requests/reddit/AppOnlyAuthRequest.h"
#include "requests/reddit/FetchAccessTokenRequest.h"
#include "requests/reddit/RefreshAccessTokenRequest.h"
#include "requests/reddit/FetchSubredditPostsRequest.h"
#include "requests/tumblr/RetrievePublishedPostsRequest.h"

#include <algorithm>

using namespace ImageScraper;

static RequestOptions MakeOptions( )
{
    RequestOptions opts{ };
    opts.m_UserAgent = "TestAgent/1.0";
    opts.m_CaBundle  = "ca-bundle.crt";
    return opts;
}

static HttpResponse MakeSuccess( const std::string& body )
{
    HttpResponse r{ };
    r.m_Success    = true;
    r.m_StatusCode = 200;
    r.m_Body       = body;
    return r;
}

static HttpResponse MakeFailure( int statusCode )
{
    HttpResponse r{ };
    r.m_Success    = false;
    r.m_StatusCode = statusCode;
    r.m_Error      = "http error";
    return r;
}

static bool HasHeaderPrefix( const std::vector<std::string>& headers, const std::string& prefix )
{
    return std::any_of( headers.begin( ), headers.end( ), [ & ]( const std::string& h )
    {
        return h.rfind( prefix, 0 ) == 0;
    } );
}

// ---------------------------------------------------------------------------
// FourChan::GetBoardsRequest
// ---------------------------------------------------------------------------
TEST_CASE( "GetBoardsRequest - success maps response body", "[Requests][FourChan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"boards":[]})" );

    FourChan::GetBoardsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"({"boards":[]})" );
    REQUIRE( mock->m_CallCount == 1 );
}

TEST_CASE( "GetBoardsRequest - HTTP failure sets error", "[Requests][FourChan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 404 );

    FourChan::GetBoardsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "GetBoardsRequest - service error JSON sets failure", "[Requests][FourChan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":404})" );

    FourChan::GetBoardsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "GetBoardsRequest - sends correct URL", "[Requests][FourChan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"boards":[]})" );

    FourChan::GetBoardsRequest req{ mock };
    req.Perform( MakeOptions( ) );

    REQUIRE( mock->m_LastRequest.m_Url == "https://a.4cdn.org/boards.json" );
}

// ---------------------------------------------------------------------------
// FourChan::GetThreadsRequest
// ---------------------------------------------------------------------------
TEST_CASE( "GetThreadsRequest - success maps response body", "[Requests][FourChan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([{"threads":[]}])" );

    FourChan::GetThreadsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "po/catalog.json";
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"([{"threads":[]}])" );
}

TEST_CASE( "GetThreadsRequest - URL is base + urlExt", "[Requests][FourChan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([{"threads":[]}])" );

    FourChan::GetThreadsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "po/catalog.json";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url == "https://a.4cdn.org/po/catalog.json" );
}

TEST_CASE( "GetThreadsRequest - HTTP failure sets error", "[Requests][FourChan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 500 );

    FourChan::GetThreadsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE_FALSE( result.m_Success );
}

// ---------------------------------------------------------------------------
// Bluesky::ResolveHandleRequest
// ---------------------------------------------------------------------------
TEST_CASE( "ResolveHandleRequest - success maps response body", "[Requests][Bluesky]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"did":"did:plc:alice"})" );

    Bluesky::ResolveHandleRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_QueryParams = { { "handle", "alice.bsky.social" } };
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"({"did":"did:plc:alice"})" );
}

TEST_CASE( "ResolveHandleRequest - sends correct URL", "[Requests][Bluesky]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"did":"did:plc:alice"})" );

    Bluesky::ResolveHandleRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_QueryParams = { { "handle", "alice.bsky.social" } };
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url == "https://public.api.bsky.app/xrpc/com.atproto.identity.resolveHandle?handle=alice.bsky.social" );
}

TEST_CASE( "ResolveHandleRequest - HTTP failure sets error", "[Requests][Bluesky]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 400 );

    Bluesky::ResolveHandleRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_QueryParams = { { "handle", "alice.bsky.social" } };
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

// ---------------------------------------------------------------------------
// Bluesky::GetAuthorFeedRequest
// ---------------------------------------------------------------------------
TEST_CASE( "GetAuthorFeedRequest - success maps response body", "[Requests][Bluesky]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"feed":[]})" );

    Bluesky::GetAuthorFeedRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_QueryParams = {
        { "actor", "did:plc:alice" },
        { "filter", "posts_with_media" },
        { "limit", "100" }
    };
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"({"feed":[]})" );
}

TEST_CASE( "GetAuthorFeedRequest - URL is base plus query params", "[Requests][Bluesky]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"feed":[]})" );

    Bluesky::GetAuthorFeedRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_QueryParams = {
        { "actor", "did:plc:alice" },
        { "filter", "posts_with_media" },
        { "limit", "100" },
        { "cursor", "cursor-123" }
    };
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url == "https://public.api.bsky.app/xrpc/app.bsky.feed.getAuthorFeed?actor=did:plc:alice&filter=posts_with_media&limit=100&cursor=cursor-123" );
}

TEST_CASE( "GetAuthorFeedRequest - HTTP failure sets error", "[Requests][Bluesky]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 500 );

    Bluesky::GetAuthorFeedRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_QueryParams = {
        { "actor", "did:plc:alice" },
        { "filter", "posts_with_media" },
        { "limit", "100" }
    };
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

// ---------------------------------------------------------------------------
// Mastodon::SearchAccountsRequest
// ---------------------------------------------------------------------------
TEST_CASE( "SearchAccountsRequest - success maps response body", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"accounts":[]})" );

    Mastodon::SearchAccountsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_QueryParams = { { "q", "alice@example.com" } };
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"({"accounts":[]})" );
    REQUIRE( mock->m_CallCount == 1 );
}

TEST_CASE( "SearchAccountsRequest - normalizes instance and URL-encodes query params", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"accounts":[]})" );

    Mastodon::SearchAccountsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "Mastodon.Social/@Gargron";
    opts.m_QueryParams = {
        { "q", "alice@example.com" },
        { "limit", "5" }
    };
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url == "https://mastodon.social/api/v2/search?q=alice%40example.com&limit=5&type=accounts" );
}

TEST_CASE( "SearchAccountsRequest - sends Bearer header when token is present", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"accounts":[]})" );

    Mastodon::SearchAccountsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_AccessToken = "token";
    opts.m_QueryParams = { { "q", "alice" } };
    req.Perform( opts );

    REQUIRE( HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Bearer " ) );
}

TEST_CASE( "SearchAccountsRequest - missing instance fails without network call", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"accounts":[]})" );

    Mastodon::SearchAccountsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_QueryParams = { { "q", "alice" } };
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( mock->m_CallCount == 0 );
}

TEST_CASE( "SearchAccountsRequest - missing query fails without network call", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"accounts":[]})" );

    Mastodon::SearchAccountsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( mock->m_CallCount == 0 );
}

TEST_CASE( "SearchAccountsRequest - HTTP failure sets error", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 401 );

    Mastodon::SearchAccountsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_QueryParams = { { "q", "alice" } };
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
    REQUIRE( result.m_Error.m_ErrorString == "http error" );
}

TEST_CASE( "SearchAccountsRequest - service error JSON sets failure", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":"The access token is invalid"})" );

    Mastodon::SearchAccountsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_QueryParams = { { "q", "alice" } };
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::BadRequest );
    REQUIRE( result.m_Error.m_ErrorString == "The access token is invalid" );
}

// ---------------------------------------------------------------------------
// Mastodon::GetAccountStatusesRequest
// ---------------------------------------------------------------------------
TEST_CASE( "GetAccountStatusesRequest - success maps response body", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([])" );

    Mastodon::GetAccountStatusesRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_ResourceId = "123";
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"([])" );
}

TEST_CASE( "GetAccountStatusesRequest - builds account statuses URL", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([])" );

    Mastodon::GetAccountStatusesRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://Mastodon.Social/";
    opts.m_ResourceId = "123";
    opts.m_QueryParams = {
        { "only_media", "true" },
        { "limit", "40" },
        { "exclude_replies", "true" },
        { "exclude_reblogs", "true" },
        { "max_id", "999" }
    };
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url == "https://mastodon.social/api/v1/accounts/123/statuses?only_media=true&limit=40&exclude_replies=true&exclude_reblogs=true&max_id=999" );
}

TEST_CASE( "GetAccountStatusesRequest - URL-encodes account id", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([])" );

    Mastodon::GetAccountStatusesRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_ResourceId = "id/with space";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url == "https://mastodon.social/api/v1/accounts/id%2Fwith%20space/statuses" );
}

TEST_CASE( "GetAccountStatusesRequest - sends Bearer header when token is present", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([])" );

    Mastodon::GetAccountStatusesRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_ResourceId = "123";
    opts.m_AccessToken = "token";
    req.Perform( opts );

    REQUIRE( HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Bearer " ) );
}

TEST_CASE( "GetAccountStatusesRequest - missing account id fails without network call", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([])" );

    Mastodon::GetAccountStatusesRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( mock->m_CallCount == 0 );
}

TEST_CASE( "GetAccountStatusesRequest - HTTP failure sets error", "[Requests][Mastodon]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 404 );

    Mastodon::GetAccountStatusesRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "https://mastodon.social";
    opts.m_ResourceId = "123";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::NotFound );
    REQUIRE( result.m_Error.m_ErrorString == "http error" );
}

// ---------------------------------------------------------------------------
// Reddit::AppOnlyAuthRequest
// ---------------------------------------------------------------------------
TEST_CASE( "AppOnlyAuthRequest - empty clientId fails without network call", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"t","expires_in":3600})" );

    Reddit::AppOnlyAuthRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId = "";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( mock->m_CallCount == 0 );
}

TEST_CASE( "AppOnlyAuthRequest - empty clientSecret fails without network call", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"t","expires_in":3600})" );

    Reddit::AppOnlyAuthRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId = "id";
    opts.m_ClientSecret = "";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( mock->m_CallCount == 0 );
}

TEST_CASE( "AppOnlyAuthRequest - success maps response body", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );

    Reddit::AppOnlyAuthRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"({"access_token":"tok","expires_in":3600})" );
}

TEST_CASE( "AppOnlyAuthRequest - sends Basic Authorization header", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );

    Reddit::AppOnlyAuthRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    req.Perform( opts );

    REQUIRE( HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Basic " ) );
}

TEST_CASE( "AppOnlyAuthRequest - HTTP failure sets error", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 401 );

    Reddit::AppOnlyAuthRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "AppOnlyAuthRequest - service error JSON sets failure", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":401})" );

    Reddit::AppOnlyAuthRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

// ---------------------------------------------------------------------------
// Reddit::FetchAccessTokenRequest
// ---------------------------------------------------------------------------
TEST_CASE( "FetchAccessTokenRequest - empty clientId fails without network call", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"t","expires_in":3600})" );

    Reddit::FetchAccessTokenRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId = "";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( mock->m_CallCount == 0 );
}

TEST_CASE( "FetchAccessTokenRequest - success maps response body", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );

    Reddit::FetchAccessTokenRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
}

TEST_CASE( "FetchAccessTokenRequest - query params are appended to POST body", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );

    Reddit::FetchAccessTokenRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    opts.m_QueryParams  = { { "code", "abc123" }, { "redirect_uri", "http://localhost" } };
    req.Perform( opts );

    const std::string& body = mock->m_LastRequest.m_Body;
    REQUIRE( body.find( "grant_type=authorization_code" ) != std::string::npos );
    REQUIRE( body.find( "code=abc123" ) != std::string::npos );
    REQUIRE( body.find( "redirect_uri=http://localhost" ) != std::string::npos );
}

TEST_CASE( "FetchAccessTokenRequest - sends Basic Authorization header", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );

    Reddit::FetchAccessTokenRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    req.Perform( opts );

    REQUIRE( HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Basic " ) );
}

// ---------------------------------------------------------------------------
// Reddit::RefreshAccessTokenRequest
// ---------------------------------------------------------------------------
TEST_CASE( "RefreshAccessTokenRequest - empty clientId fails without network call", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );

    Reddit::RefreshAccessTokenRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId = "";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
    REQUIRE( mock->m_CallCount == 0 );
}

TEST_CASE( "RefreshAccessTokenRequest - success maps response body", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"newtok","expires_in":3600})" );

    Reddit::RefreshAccessTokenRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
}

TEST_CASE( "RefreshAccessTokenRequest - POST body starts with refresh_token grant", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"access_token":"newtok","expires_in":3600})" );

    Reddit::RefreshAccessTokenRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";
    opts.m_QueryParams  = { { "refresh_token", "mytoken" } };
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Body.find( "grant_type=refresh_token" ) != std::string::npos );
    REQUIRE( mock->m_LastRequest.m_Body.find( "refresh_token=mytoken" ) != std::string::npos );
}

// ---------------------------------------------------------------------------
// Reddit::FetchSubredditPostsRequest
// ---------------------------------------------------------------------------
TEST_CASE( "FetchSubredditPostsRequest - no token uses public URL", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"data":{}})" );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt      = "r/pics/hot.json";
    opts.m_AccessToken = "";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url.find( "www.reddit.com" ) != std::string::npos );
    REQUIRE( mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) == std::string::npos );
}

TEST_CASE( "FetchSubredditPostsRequest - with token uses OAuth URL and Bearer header", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"data":{}})" );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt      = "r/pics/hot.json";
    opts.m_AccessToken = "mytoken";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) != std::string::npos );
    REQUIRE( HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Bearer " ) );
}

TEST_CASE( "FetchSubredditPostsRequest - user listing path uses public user URL", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"data":{}})" );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "user/spez/submitted.json";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url.find( "www.reddit.com/user/spez/submitted.json" ) != std::string::npos );
    REQUIRE( mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) == std::string::npos );
}

TEST_CASE( "FetchSubredditPostsRequest - HTTP failure sets error", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 403 );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "r/pics/hot.json";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "FetchSubredditPostsRequest - service error JSON sets failure", "[Requests][Reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":403})" );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "r/pics/hot.json";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

// ---------------------------------------------------------------------------
// Tumblr::RetrievePublishedPostsRequest
// ---------------------------------------------------------------------------
TEST_CASE( "RetrievePublishedPostsRequest - success maps response body", "[Requests][Tumblr]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"response":{}})" );

    Tumblr::RetrievePublishedPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
    const auto result = req.Perform( opts );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"({"response":{}})" );
}

TEST_CASE( "RetrievePublishedPostsRequest - URL is base + urlExt + query params", "[Requests][Tumblr]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"response":{}})" );

    Tumblr::RetrievePublishedPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt     = "testblog.tumblr.com/posts/photo";
    opts.m_QueryParams = { { "api_key", "mykey" } };
    req.Perform( opts );

    const std::string& url = mock->m_LastRequest.m_Url;
    REQUIRE( url.find( "https://api.tumblr.com/v2/blog/" ) != std::string::npos );
    REQUIRE( url.find( "testblog.tumblr.com/posts/photo" ) != std::string::npos );
    REQUIRE( url.find( "api_key=mykey" ) != std::string::npos );
}

TEST_CASE( "RetrievePublishedPostsRequest - HTTP failure sets error", "[Requests][Tumblr]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 401 );

    Tumblr::RetrievePublishedPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "RetrievePublishedPostsRequest - service error JSON sets failure", "[Requests][Tumblr]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":401})" );

    Tumblr::RetrievePublishedPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}
