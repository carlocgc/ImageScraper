#include "catch2/catch_amalgamated.hpp"
#include "auth/OAuthClient.h"
#include "io/JsonFile.h"

#include <filesystem>
#include <string>

using namespace ImageScraper;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

struct TempOAuthFile
{
    std::string path;

    explicit TempOAuthFile( const char* suffix )
    {
        path = ( std::filesystem::temp_directory_path( ) / suffix ).string( );
    }

    ~TempOAuthFile( )
    {
        std::error_code ec;
        std::filesystem::remove( path, ec );
    }
};

static const std::string k_StateKey        = "oauth_test_state_id";
static const std::string k_RefreshTokenKey = "oauth_test_refresh_token";
static const std::string k_ClientIdKey     = "oauth_test_client_id";
static const std::string k_ClientSecretKey = "oauth_test_client_secret";
static const std::string k_KnownState      = "KNOWN_TEST_STATE_ABC123";

struct OAuthFixture
{
    TempOAuthFile appFile{  "is_oauth_test_app.json" };
    TempOAuthFile userFile{ "is_oauth_test_user.json" };
    std::shared_ptr<JsonFile> appConfig;
    std::shared_ptr<JsonFile> userConfig;

    OAuthFixture( )
    {
        appConfig  = std::make_shared<JsonFile>( appFile.path );
        userConfig = std::make_shared<JsonFile>( userFile.path );

        // Seed a known state so tests can construct valid callback responses.
        appConfig->SetValue<std::string>( k_StateKey, k_KnownState );
        userConfig->SetValue<std::string>( k_ClientIdKey,     "test_client_id_value" );
        userConfig->SetValue<std::string>( k_ClientSecretKey, "test_client_secret_value" );
    }

    OAuthConfig MakeConfig( const std::string& redirectUri = "http://localhost:8080" ) const
    {
        return OAuthConfig{
            "https://example.com/oauth/authorize",
            "read",
            k_ClientIdKey,
            k_ClientSecretKey,
            k_StateKey,
            k_RefreshTokenKey,
            redirectUri,
            { }
        };
    }

    std::unique_ptr<OAuthClient> MakeClient(
        OAuthClient::TokenRequestFn fetchFn   = { },
        OAuthClient::TokenRequestFn refreshFn = { },
        const std::string& redirectUri        = "http://localhost:8080" ) const
    {
        if( !fetchFn )
        {
            fetchFn = []( const RequestOptions& ) -> RequestResult { return { }; };
        }
        if( !refreshFn )
        {
            refreshFn = []( const RequestOptions& ) -> RequestResult { return { }; };
        }
        return std::make_unique<OAuthClient>(
            MakeConfig( redirectUri ),
            appConfig,
            userConfig,
            "",
            "TestAgent",
            fetchFn,
            refreshFn );
    }

    // A well-formed OAuth callback response with the known state and a valid code.
    std::string ValidCallbackResponse( ) const
    {
        return "GET /?state=" + k_KnownState + "&code=test_auth_code HTTP/1.1";
    }

    // Standard OAuth2 token JSON.
    static std::string TokenJson( int expireSeconds = 3600 )
    {
        return R"({"access_token":"test_access_token","expires_in":)"
               + std::to_string( expireSeconds )
               + R"(,"refresh_token":"test_refresh_token"})";
    }

    // A fetch function that always returns a successful token response.
    static OAuthClient::TokenRequestFn SuccessfulFetchFn( int expireSeconds = 3600 )
    {
        return [ expireSeconds ]( const RequestOptions& ) -> RequestResult
            {
                RequestResult r;
                r.m_Success  = true;
                r.m_Response = TokenJson( expireSeconds );
                return r;
            };
    }

    // A fetch function that always returns a network failure.
    static OAuthClient::TokenRequestFn FailingFetchFn( )
    {
        return []( const RequestOptions& ) -> RequestResult
            {
                RequestResult r;
                r.m_Success = false;
                r.SetError( ResponseErrorCode::InternalServerError );
                return r;
            };
    }
};

// ---------------------------------------------------------------------------
// IsSignedIn / IsAuthenticated initial state
// ---------------------------------------------------------------------------

TEST_CASE( "IsSignedIn returns false before auth", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    REQUIRE( client->IsSignedIn( ) == false );
}

TEST_CASE( "IsAuthenticated returns false before auth", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    REQUIRE( client->IsAuthenticated( ) == false );
}

// ---------------------------------------------------------------------------
// HandleAuth - failure cases
// ---------------------------------------------------------------------------

TEST_CASE( "HandleAuth returns false for favicon message", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    REQUIRE( client->HandleAuth( "GET /favicon.ico HTTP/1.1", "Test" ) == false );
}

TEST_CASE( "HandleAuth returns false when error param is present", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    REQUIRE( client->HandleAuth( "GET /?error=access_denied HTTP/1.1", "Test" ) == false );
}

TEST_CASE( "HandleAuth returns false when error param appears after ampersand", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    REQUIRE( client->HandleAuth( "GET /?state=abc&error=access_denied HTTP/1.1", "Test" ) == false );
}

TEST_CASE( "HandleAuth returns false on state mismatch", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    REQUIRE( client->HandleAuth( "GET /?state=WRONG_STATE&code=someCode HTTP/1.1", "Test" ) == false );
}

TEST_CASE( "HandleAuth returns false when code is missing", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    // Correct state but no code parameter - code extraction will yield an empty string.
    const std::string response = "GET /?state=" + k_KnownState + " HTTP/1.1";
    REQUIRE( client->HandleAuth( response, "Test" ) == false );
}

TEST_CASE( "HandleAuth returns false when token fetch fails", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( OAuthFixture::FailingFetchFn( ) );
    REQUIRE( client->HandleAuth( f.ValidCallbackResponse( ), "Test" ) == false );
}

// ---------------------------------------------------------------------------
// HandleAuth - success path
// ---------------------------------------------------------------------------

TEST_CASE( "HandleAuth returns true and stores tokens on valid response", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
    REQUIRE( client->HandleAuth( f.ValidCallbackResponse( ), "Test" ) == true );
}

TEST_CASE( "IsSignedIn returns true after successful HandleAuth", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
    client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
    REQUIRE( client->IsSignedIn( ) == true );
}

TEST_CASE( "IsAuthenticated returns true after successful HandleAuth", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
    client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
    REQUIRE( client->IsAuthenticated( ) == true );
}

TEST_CASE( "IsAuthenticated returns false when access token is expired", "[OAuthClient]" )
{
    OAuthFixture f;
    // expires_in = 0 -> AuthExpireSeconds = 0 - 180 = -180 -> already expired
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( 0 ) );
    client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
    REQUIRE( client->IsAuthenticated( ) == false );
}

TEST_CASE( "GetAccessToken returns stored token after successful HandleAuth", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
    client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
    REQUIRE( client->GetAccessToken( ) == "test_access_token" );
}

// ---------------------------------------------------------------------------
// TryRefreshToken
// ---------------------------------------------------------------------------

TEST_CASE( "TryRefreshToken returns false when no refresh token is set", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    REQUIRE( client->TryRefreshToken( ) == false );
}

TEST_CASE( "TryRefreshToken returns true and updates access token on valid mock response", "[OAuthClient]" )
{
    OAuthFixture f;
    // First, set a refresh token by completing a sign-in.
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ), OAuthFixture::SuccessfulFetchFn( ) );
    client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
    REQUIRE( client->IsSignedIn( ) );

    // Now manually expire the access token and use TryRefreshToken.
    client->StoreAccessToken( "", 0 ); // clear / expire access token
    REQUIRE( client->IsAuthenticated( ) == false );

    REQUIRE( client->TryRefreshToken( ) == true );
    REQUIRE( client->IsAuthenticated( ) == true );
    REQUIRE( client->GetAccessToken( ) == "test_access_token" );
}

TEST_CASE( "TryRefreshToken clears IsSignedIn when refresh request fails", "[OAuthClient]" )
{
    OAuthFixture f;
    // Sign in successfully, then fail the refresh.
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ), OAuthFixture::FailingFetchFn( ) );
    client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
    REQUIRE( client->IsSignedIn( ) );

    REQUIRE( client->TryRefreshToken( ) == false );
    REQUIRE( client->IsSignedIn( ) == false );
}

// ---------------------------------------------------------------------------
// SignOut
// ---------------------------------------------------------------------------

TEST_CASE( "SignOut clears IsSignedIn and IsAuthenticated", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
    client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
    REQUIRE( client->IsSignedIn( ) );
    REQUIRE( client->IsAuthenticated( ) );

    client->SignOut( );

    REQUIRE( client->IsSignedIn( ) == false );
    REQUIRE( client->IsAuthenticated( ) == false );
}

TEST_CASE( "SignOut removes refresh token from appConfig", "[OAuthClient]" )
{
    OAuthFixture f;
    {
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        client->SignOut( );
    }

    // Reload config from disk and verify the key is empty.
    JsonFile reloaded( f.appFile.path );
    reloaded.Deserialise( );
    std::string storedToken;
    reloaded.GetValue<std::string>( k_RefreshTokenKey, storedToken );
    REQUIRE( storedToken.empty( ) );
}

// ---------------------------------------------------------------------------
// BuildAuthUrl
// ---------------------------------------------------------------------------

TEST_CASE( "BuildAuthUrl contains client_id and state", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( );
    const std::string url = client->BuildAuthUrl( "my_test_client" );
    REQUIRE( url.find( "client_id=my_test_client" ) != std::string::npos );
    REQUIRE( url.find( "state=" + k_KnownState ) != std::string::npos );
}

TEST_CASE( "BuildAuthUrl contains redirect_uri when configured", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( { }, { }, "http://localhost:8080" );
    const std::string url = client->BuildAuthUrl( "my_client" );
    REQUIRE( url.find( "redirect_uri=" ) != std::string::npos );
}

TEST_CASE( "BuildAuthUrl omits redirect_uri when config field is empty", "[OAuthClient]" )
{
    OAuthFixture f;
    auto client = f.MakeClient( { }, { }, "" );  // empty redirect URI
    const std::string url = client->BuildAuthUrl( "my_client" );
    REQUIRE( url.find( "redirect_uri=" ) == std::string::npos );
}

TEST_CASE( "BuildAuthUrl appends extra auth params when configured", "[OAuthClient]" )
{
    OAuthFixture f;
    OAuthConfig config = f.MakeConfig( );
    config.m_ExtraAuthParams = { { "duration", "permanent" } };

    auto client = std::make_unique<OAuthClient>(
        config,
        f.appConfig,
        f.userConfig,
        "",
        "TestAgent",
        []( const RequestOptions& ) -> RequestResult { return { }; },
        []( const RequestOptions& ) -> RequestResult { return { }; } );

    const std::string url = client->BuildAuthUrl( "client" );
    REQUIRE( url.find( "duration=permanent" ) != std::string::npos );
}
