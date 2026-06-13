#include "CppUnitTest.h"
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

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

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

    TEST_CLASS(RequestTests)
    {
    public:
    TEST_METHOD(GetBoardsRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"boards":[]})" );
    
        FourChan::GetBoardsRequest req{ mock };
        const auto result = req.Perform( MakeOptions( ) );
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"({"boards":[]})" );
        Assert::IsTrue(  mock->m_CallCount == 1 );
    }
    
    TEST_METHOD(GetBoardsRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 404 );
    
        FourChan::GetBoardsRequest req{ mock };
        const auto result = req.Perform( MakeOptions( ) );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    TEST_METHOD(GetBoardsRequest_Service_Error_JSON_Sets_Failure)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"error":404})" );
    
        FourChan::GetBoardsRequest req{ mock };
        const auto result = req.Perform( MakeOptions( ) );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    TEST_METHOD(GetBoardsRequest_Sends_Correct_URL)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"boards":[]})" );
    
        FourChan::GetBoardsRequest req{ mock };
        req.Perform( MakeOptions( ) );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url == "https://a.4cdn.org/boards.json" );
    }
    
    // ---------------------------------------------------------------------------
    // FourChan::GetThreadsRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(GetThreadsRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"([{"threads":[]}])" );
    
        FourChan::GetThreadsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "po/catalog.json";
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"([{"threads":[]}])" );
    }
    
    TEST_METHOD(GetThreadsRequest_URL_Is_Base_UrlExt)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"([{"threads":[]}])" );
    
        FourChan::GetThreadsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "po/catalog.json";
        req.Perform( opts );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url == "https://a.4cdn.org/po/catalog.json" );
    }
    
    TEST_METHOD(GetThreadsRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 500 );
    
        FourChan::GetThreadsRequest req{ mock };
        const auto result = req.Perform( MakeOptions( ) );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    // ---------------------------------------------------------------------------
    // Bluesky::ResolveHandleRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(ResolveHandleRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"did":"did:plc:alice"})" );
    
        Bluesky::ResolveHandleRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_QueryParams = { { "handle", "alice.bsky.social" } };
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"({"did":"did:plc:alice"})" );
    }
    
    TEST_METHOD(ResolveHandleRequest_Sends_Correct_URL)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"did":"did:plc:alice"})" );
    
        Bluesky::ResolveHandleRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_QueryParams = { { "handle", "alice.bsky.social" } };
        req.Perform( opts );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url == "https://public.api.bsky.app/xrpc/com.atproto.identity.resolveHandle?handle=alice.bsky.social" );
    }
    
    TEST_METHOD(ResolveHandleRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 400 );
    
        Bluesky::ResolveHandleRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_QueryParams = { { "handle", "alice.bsky.social" } };
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    // ---------------------------------------------------------------------------
    // Bluesky::GetAuthorFeedRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(GetAuthorFeedRequest_Success_Maps_Response_Body)
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
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"({"feed":[]})" );
    }
    
    TEST_METHOD(GetAuthorFeedRequest_URL_Is_Base_Plus_Query_Params)
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
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url == "https://public.api.bsky.app/xrpc/app.bsky.feed.getAuthorFeed?actor=did:plc:alice&filter=posts_with_media&limit=100&cursor=cursor-123" );
    }
    
    TEST_METHOD(GetAuthorFeedRequest_HTTP_Failure_Sets_Error)
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
    
        Assert::IsFalse(  result.m_Success );
    }
    
    // ---------------------------------------------------------------------------
    // Mastodon::SearchAccountsRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(SearchAccountsRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"accounts":[]})" );
    
        Mastodon::SearchAccountsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_QueryParams = { { "q", "alice@example.com" } };
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"({"accounts":[]})" );
        Assert::IsTrue(  mock->m_CallCount == 1 );
    }
    
    TEST_METHOD(SearchAccountsRequest_Normalizes_Instance_And_URL_Encodes_Query_Params)
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
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url == "https://mastodon.social/api/v2/search?q=alice%40example.com&limit=5&type=accounts" );
    }
    
    TEST_METHOD(SearchAccountsRequest_Sends_Bearer_Header_When_Token_Is_Present)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"accounts":[]})" );
    
        Mastodon::SearchAccountsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_AccessToken = "token";
        opts.m_QueryParams = { { "q", "alice" } };
        req.Perform( opts );
    
        Assert::IsTrue(  HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Bearer " ) );
    }
    
    TEST_METHOD(SearchAccountsRequest_Missing_Instance_Fails_Without_Network_Call)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"accounts":[]})" );
    
        Mastodon::SearchAccountsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_QueryParams = { { "q", "alice" } };
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  mock->m_CallCount == 0 );
    }
    
    TEST_METHOD(SearchAccountsRequest_Missing_Query_Fails_Without_Network_Call)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"accounts":[]})" );
    
        Mastodon::SearchAccountsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  mock->m_CallCount == 0 );
    }
    
    TEST_METHOD(SearchAccountsRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 401 );
    
        Mastodon::SearchAccountsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_QueryParams = { { "q", "alice" } };
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "http error" );
    }
    
    TEST_METHOD(SearchAccountsRequest_Service_Error_JSON_Sets_Failure)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"error":"The access token is invalid"})" );
    
        Mastodon::SearchAccountsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_QueryParams = { { "q", "alice" } };
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::BadRequest );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "The access token is invalid" );
    }
    
    // ---------------------------------------------------------------------------
    // Mastodon::GetAccountStatusesRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(GetAccountStatusesRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"([])" );
    
        Mastodon::GetAccountStatusesRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_ResourceId = "123";
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"([])" );
    }
    
    TEST_METHOD(GetAccountStatusesRequest_Builds_Account_Statuses_URL)
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
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url == "https://mastodon.social/api/v1/accounts/123/statuses?only_media=true&limit=40&exclude_replies=true&exclude_reblogs=true&max_id=999" );
    }
    
    TEST_METHOD(GetAccountStatusesRequest_URL_Encodes_Account_Id)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"([])" );
    
        Mastodon::GetAccountStatusesRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_ResourceId = "id/with space";
        req.Perform( opts );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url == "https://mastodon.social/api/v1/accounts/id%2Fwith%20space/statuses" );
    }
    
    TEST_METHOD(GetAccountStatusesRequest_Sends_Bearer_Header_When_Token_Is_Present)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"([])" );
    
        Mastodon::GetAccountStatusesRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_ResourceId = "123";
        opts.m_AccessToken = "token";
        req.Perform( opts );
    
        Assert::IsTrue(  HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Bearer " ) );
    }
    
    TEST_METHOD(GetAccountStatusesRequest_Missing_Account_Id_Fails_Without_Network_Call)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"([])" );
    
        Mastodon::GetAccountStatusesRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  mock->m_CallCount == 0 );
    }
    
    TEST_METHOD(GetAccountStatusesRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 404 );
    
        Mastodon::GetAccountStatusesRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "https://mastodon.social";
        opts.m_ResourceId = "123";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::NotFound );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "http error" );
    }
    
    // ---------------------------------------------------------------------------
    // Reddit::AppOnlyAuthRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(AppOnlyAuthRequest_Empty_ClientId_Fails_Without_Network_Call)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"t","expires_in":3600})" );
    
        Reddit::AppOnlyAuthRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId = "";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  mock->m_CallCount == 0 );
    }
    
    TEST_METHOD(AppOnlyAuthRequest_Empty_ClientSecret_Fails_Without_Network_Call)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"t","expires_in":3600})" );
    
        Reddit::AppOnlyAuthRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId = "id";
        opts.m_ClientSecret = "";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  mock->m_CallCount == 0 );
    }
    
    TEST_METHOD(AppOnlyAuthRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );
    
        Reddit::AppOnlyAuthRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"({"access_token":"tok","expires_in":3600})" );
    }
    
    TEST_METHOD(AppOnlyAuthRequest_Sends_Basic_Authorization_Header)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );
    
        Reddit::AppOnlyAuthRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        req.Perform( opts );
    
        Assert::IsTrue(  HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Basic " ) );
    }
    
    TEST_METHOD(AppOnlyAuthRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 401 );
    
        Reddit::AppOnlyAuthRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    TEST_METHOD(AppOnlyAuthRequest_Service_Error_JSON_Sets_Failure)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"error":401})" );
    
        Reddit::AppOnlyAuthRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    // ---------------------------------------------------------------------------
    // Reddit::FetchAccessTokenRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(FetchAccessTokenRequest_Empty_ClientId_Fails_Without_Network_Call)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"t","expires_in":3600})" );
    
        Reddit::FetchAccessTokenRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId = "";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  mock->m_CallCount == 0 );
    }
    
    TEST_METHOD(FetchAccessTokenRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );
    
        Reddit::FetchAccessTokenRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
    }
    
    TEST_METHOD(FetchAccessTokenRequest_Query_Params_Are_Appended_To_POST_Body)
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
        Assert::IsTrue(  body.find( "grant_type=authorization_code" ) != std::string::npos );
        Assert::IsTrue(  body.find( "code=abc123" ) != std::string::npos );
        Assert::IsTrue(  body.find( "redirect_uri=http://localhost" ) != std::string::npos );
    }
    
    TEST_METHOD(FetchAccessTokenRequest_Sends_Basic_Authorization_Header)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"tok","expires_in":3600})" );
    
        Reddit::FetchAccessTokenRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        req.Perform( opts );
    
        Assert::IsTrue(  HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Basic " ) );
    }
    
    // ---------------------------------------------------------------------------
    // Reddit::RefreshAccessTokenRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(RefreshAccessTokenRequest_Empty_ClientId_Fails_Without_Network_Call)
    {
        auto mock = std::make_shared<MockHttpClient>( );
    
        Reddit::RefreshAccessTokenRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId = "";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
        Assert::IsTrue(  mock->m_CallCount == 0 );
    }
    
    TEST_METHOD(RefreshAccessTokenRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"newtok","expires_in":3600})" );
    
        Reddit::RefreshAccessTokenRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
    }
    
    TEST_METHOD(RefreshAccessTokenRequest_POST_Body_Starts_With_Refresh_Token_Grant)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"access_token":"newtok","expires_in":3600})" );
    
        Reddit::RefreshAccessTokenRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
        opts.m_QueryParams  = { { "refresh_token", "mytoken" } };
        req.Perform( opts );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Body.find( "grant_type=refresh_token" ) != std::string::npos );
        Assert::IsTrue(  mock->m_LastRequest.m_Body.find( "refresh_token=mytoken" ) != std::string::npos );
    }
    
    // ---------------------------------------------------------------------------
    // Reddit::FetchSubredditPostsRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(FetchSubredditPostsRequest_No_Token_Uses_Public_URL)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"data":{}})" );
    
        Reddit::FetchSubredditPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt      = "r/pics/hot.json";
        opts.m_AccessToken = "";
        req.Perform( opts );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url.find( "www.reddit.com" ) != std::string::npos );
        Assert::IsTrue(  mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) == std::string::npos );
    }
    
    TEST_METHOD(FetchSubredditPostsRequest_With_Token_Uses_OAuth_URL_And_Bearer_Header)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"data":{}})" );
    
        Reddit::FetchSubredditPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt      = "r/pics/hot.json";
        opts.m_AccessToken = "mytoken";
        req.Perform( opts );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) != std::string::npos );
        Assert::IsTrue(  HasHeaderPrefix( mock->m_LastRequest.m_Headers, "Authorization: Bearer " ) );
    }
    
    TEST_METHOD(FetchSubredditPostsRequest_User_Listing_Path_Uses_Public_User_URL)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"data":{}})" );
    
        Reddit::FetchSubredditPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "user/spez/submitted.json";
        req.Perform( opts );
    
        Assert::IsTrue(  mock->m_LastRequest.m_Url.find( "www.reddit.com/user/spez/submitted.json" ) != std::string::npos );
        Assert::IsTrue(  mock->m_LastRequest.m_Url.find( "oauth.reddit.com" ) == std::string::npos );
    }
    
    TEST_METHOD(FetchSubredditPostsRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 403 );
    
        Reddit::FetchSubredditPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "r/pics/hot.json";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    TEST_METHOD(FetchSubredditPostsRequest_Service_Error_JSON_Sets_Failure)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"error":403})" );
    
        Reddit::FetchSubredditPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "r/pics/hot.json";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    // ---------------------------------------------------------------------------
    // Tumblr::RetrievePublishedPostsRequest
    // ---------------------------------------------------------------------------
    TEST_METHOD(RetrievePublishedPostsRequest_Success_Maps_Response_Body)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"response":{}})" );
    
        Tumblr::RetrievePublishedPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
        const auto result = req.Perform( opts );
    
        Assert::IsTrue(  result.m_Success );
        Assert::IsTrue(  result.m_Response == R"({"response":{}})" );
    }
    
    TEST_METHOD(RetrievePublishedPostsRequest_URL_Is_Base_UrlExt_Query_Params)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"response":{}})" );
    
        Tumblr::RetrievePublishedPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt     = "testblog.tumblr.com/posts/photo";
        opts.m_QueryParams = { { "api_key", "mykey" } };
        req.Perform( opts );
    
        const std::string& url = mock->m_LastRequest.m_Url;
        Assert::IsTrue(  url.find( "https://api.tumblr.com/v2/blog/" ) != std::string::npos );
        Assert::IsTrue(  url.find( "testblog.tumblr.com/posts/photo" ) != std::string::npos );
        Assert::IsTrue(  url.find( "api_key=mykey" ) != std::string::npos );
    }
    
    TEST_METHOD(RetrievePublishedPostsRequest_HTTP_Failure_Sets_Error)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeFailure( 401 );
    
        Tumblr::RetrievePublishedPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    TEST_METHOD(RetrievePublishedPostsRequest_Service_Error_JSON_Sets_Failure)
    {
        auto mock = std::make_shared<MockHttpClient>( );
        mock->m_Response = MakeSuccess( R"({"error":401})" );
    
        Tumblr::RetrievePublishedPostsRequest req{ mock };
        auto opts = MakeOptions( );
        opts.m_UrlExt = "testblog.tumblr.com/posts/photo";
        const auto result = req.Perform( opts );
    
        Assert::IsFalse(  result.m_Success );
    }
    
    };
}
