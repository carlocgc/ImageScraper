#include "catch2/catch_amalgamated.hpp"
#include "mocks/MockHttpClient.h"

#include "requests/fourchan/GetBoardsRequest.h"
#include "requests/fourchan/GetThreadsRequest.h"
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
TEST_CASE( "GetBoardsRequest — success maps response body", "[requests][fourchan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"boards":[]})" );

    FourChan::GetBoardsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE( result.m_Success );
    REQUIRE( result.m_Response == R"({"boards":[]})" );
    REQUIRE( mock->m_CallCount == 1 );
}

TEST_CASE( "GetBoardsRequest — HTTP failure sets error", "[requests][fourchan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 404 );

    FourChan::GetBoardsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "GetBoardsRequest — service error JSON sets failure", "[requests][fourchan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":404})" );

    FourChan::GetBoardsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "GetBoardsRequest — sends correct URL", "[requests][fourchan]" )
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
TEST_CASE( "GetThreadsRequest — success maps response body", "[requests][fourchan]" )
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

TEST_CASE( "GetThreadsRequest — URL is base + urlExt", "[requests][fourchan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"([{"threads":[]}])" );

    FourChan::GetThreadsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "po/catalog.json";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url == "https://a.4cdn.org/po/catalog.json" );
}

TEST_CASE( "GetThreadsRequest — HTTP failure sets error", "[requests][fourchan]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 500 );

    FourChan::GetThreadsRequest req{ mock };
    const auto result = req.Perform( MakeOptions( ) );

    REQUIRE_FALSE( result.m_Success );
}

// ---------------------------------------------------------------------------
// Reddit::AppOnlyAuthRequest
// ---------------------------------------------------------------------------
TEST_CASE( "AppOnlyAuthRequest — empty clientId fails without network call", "[requests][reddit]" )
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

TEST_CASE( "AppOnlyAuthRequest — empty clientSecret fails without network call", "[requests][reddit]" )
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

TEST_CASE( "AppOnlyAuthRequest — success maps response body", "[requests][reddit]" )
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

TEST_CASE( "AppOnlyAuthRequest — sends Basic Authorization header", "[requests][reddit]" )
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

TEST_CASE( "AppOnlyAuthRequest — HTTP failure sets error", "[requests][reddit]" )
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

TEST_CASE( "AppOnlyAuthRequest — service error JSON sets failure", "[requests][reddit]" )
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
TEST_CASE( "FetchAccessTokenRequest — empty clientId fails without network call", "[requests][reddit]" )
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

TEST_CASE( "FetchAccessTokenRequest — success maps response body", "[requests][reddit]" )
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

TEST_CASE( "FetchAccessTokenRequest — query params are appended to POST body", "[requests][reddit]" )
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

TEST_CASE( "FetchAccessTokenRequest — sends Basic Authorization header", "[requests][reddit]" )
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
TEST_CASE( "RefreshAccessTokenRequest — empty clientId fails without network call", "[requests][reddit]" )
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

TEST_CASE( "RefreshAccessTokenRequest — success maps response body", "[requests][reddit]" )
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

TEST_CASE( "RefreshAccessTokenRequest — POST body starts with refresh_token grant", "[requests][reddit]" )
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
TEST_CASE( "FetchSubredditPostsRequest — no token uses public URL", "[requests][reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"data":{}})" );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt      = "pics/hot.json";
    opts.m_AccessToken = "";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url.find( "www.reddit.com" ) != std::string::npos );
    REQUIRE( mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) == std::string::npos );
}

TEST_CASE( "FetchSubredditPostsRequest — with token uses OAuth URL and Bearer header", "[requests][reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"data":{}})" );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt      = "pics/hot.json";
    opts.m_AccessToken = "mytoken";
    req.Perform( opts );

    REQUIRE( mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) != std::string::npos );
    REQUIRE( HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Bearer " ) );
}

TEST_CASE( "FetchSubredditPostsRequest — HTTP failure sets error", "[requests][reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 403 );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "pics/hot.json";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "FetchSubredditPostsRequest — service error JSON sets failure", "[requests][reddit]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":403})" );

    Reddit::FetchSubredditPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "pics/hot.json";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

// ---------------------------------------------------------------------------
// Tumblr::RetrievePublishedPostsRequest
// ---------------------------------------------------------------------------
TEST_CASE( "RetrievePublishedPostsRequest — success maps response body", "[requests][tumblr]" )
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

TEST_CASE( "RetrievePublishedPostsRequest — URL is base + urlExt + query params", "[requests][tumblr]" )
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

TEST_CASE( "RetrievePublishedPostsRequest — HTTP failure sets error", "[requests][tumblr]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeFailure( 401 );

    Tumblr::RetrievePublishedPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "RetrievePublishedPostsRequest — service error JSON sets failure", "[requests][tumblr]" )
{
    auto mock = std::make_shared<MockHttpClient>( );
    mock->m_Response = MakeSuccess( R"({"error":401})" );

    Tumblr::RetrievePublishedPostsRequest req{ mock };
    auto opts = MakeOptions( );
    opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
    const auto result = req.Perform( opts );

    REQUIRE_FALSE( result.m_Success );
}
