#include "CppUnitTest.h"
#include "auth/OAuthClient.h"
#include "io/JsonFile.h"

#include <filesystem>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

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


    TEST_CLASS(OAuthClientTests)
    {
    public:
    TEST_METHOD(IsSignedIn_Returns_False_Before_Auth)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        Assert::IsTrue(  client->IsSignedIn( ) == false );
    }
    
    TEST_METHOD(IsAuthenticated_Returns_False_Before_Auth)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        Assert::IsTrue(  client->IsAuthenticated( ) == false );
    }
    
    // ---------------------------------------------------------------------------
    // HandleAuth - failure cases
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(HandleAuth_Returns_False_For_Favicon_Message)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        Assert::IsTrue(  client->HandleAuth( "GET /favicon.ico HTTP/1.1", "Test" ) == false );
    }
    
    TEST_METHOD(HandleAuth_Returns_False_When_Error_Param_Is_Present)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        Assert::IsTrue(  client->HandleAuth( "GET /?error=access_denied HTTP/1.1", "Test" ) == false );
    }
    
    TEST_METHOD(HandleAuth_Returns_False_When_Error_Param_Appears_After_Ampersand)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        Assert::IsTrue(  client->HandleAuth( "GET /?state=abc&error=access_denied HTTP/1.1", "Test" ) == false );
    }
    
    TEST_METHOD(HandleAuth_Returns_False_On_State_Mismatch)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        Assert::IsTrue(  client->HandleAuth( "GET /?state=WRONG_STATE&code=someCode HTTP/1.1", "Test" ) == false );
    }
    
    TEST_METHOD(HandleAuth_Returns_False_When_Code_Is_Missing)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        // Correct state but no code parameter - code extraction will yield an empty string.
        const std::string response = "GET /?state=" + k_KnownState + " HTTP/1.1";
        Assert::IsTrue(  client->HandleAuth( response, "Test" ) == false );
    }
    
    TEST_METHOD(HandleAuth_Returns_False_When_Token_Fetch_Fails)
    {
        OAuthFixture f;
        auto client = f.MakeClient( OAuthFixture::FailingFetchFn( ) );
        Assert::IsTrue(  client->HandleAuth( f.ValidCallbackResponse( ), "Test" ) == false );
    }
    
    // ---------------------------------------------------------------------------
    // HandleAuth - success path
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(HandleAuth_Returns_True_And_Stores_Tokens_On_Valid_Response)
    {
        OAuthFixture f;
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
        Assert::IsTrue(  client->HandleAuth( f.ValidCallbackResponse( ), "Test" ) == true );
    }
    
    TEST_METHOD(IsSignedIn_Returns_True_After_Successful_HandleAuth)
    {
        OAuthFixture f;
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        Assert::IsTrue(  client->IsSignedIn( ) == true );
    }
    
    TEST_METHOD(IsAuthenticated_Returns_True_After_Successful_HandleAuth)
    {
        OAuthFixture f;
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        Assert::IsTrue(  client->IsAuthenticated( ) == true );
    }
    
    TEST_METHOD(IsAuthenticated_Returns_False_When_Access_Token_Is_Expired)
    {
        OAuthFixture f;
        // expires_in = 0 -> AuthExpireSeconds = 0 - 180 = -180 -> already expired
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( 0 ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        Assert::IsTrue(  client->IsAuthenticated( ) == false );
    }
    
    TEST_METHOD(GetAccessToken_Returns_Stored_Token_After_Successful_HandleAuth)
    {
        OAuthFixture f;
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        Assert::IsTrue(  client->GetAccessToken( ) == "test_access_token" );
    }
    
    // ---------------------------------------------------------------------------
    // TryRefreshToken
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(TryRefreshToken_Returns_False_When_No_Refresh_Token_Is_Set)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        Assert::IsTrue(  client->TryRefreshToken( ) == false );
    }
    
    TEST_METHOD(TryRefreshToken_Returns_True_And_Updates_Access_Token_On_Valid_Mock_Response)
    {
        OAuthFixture f;
        // First, set a refresh token by completing a sign-in.
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ), OAuthFixture::SuccessfulFetchFn( ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        Assert::IsTrue(  client->IsSignedIn( ) );
    
        // Now manually expire the access token and use TryRefreshToken.
        client->StoreAccessToken( "", 0 ); // clear / expire access token
        Assert::IsTrue(  client->IsAuthenticated( ) == false );
    
        Assert::IsTrue(  client->TryRefreshToken( ) == true );
        Assert::IsTrue(  client->IsAuthenticated( ) == true );
        Assert::IsTrue(  client->GetAccessToken( ) == "test_access_token" );
    }
    
    TEST_METHOD(TryRefreshToken_Clears_IsSignedIn_When_Refresh_Request_Fails)
    {
        OAuthFixture f;
        // Sign in successfully, then fail the refresh.
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ), OAuthFixture::FailingFetchFn( ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        Assert::IsTrue(  client->IsSignedIn( ) );
    
        Assert::IsTrue(  client->TryRefreshToken( ) == false );
        Assert::IsTrue(  client->IsSignedIn( ) == false );
    }
    
    // ---------------------------------------------------------------------------
    // SignOut
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(SignOut_Clears_IsSignedIn_And_IsAuthenticated)
    {
        OAuthFixture f;
        auto client = f.MakeClient( OAuthFixture::SuccessfulFetchFn( ) );
        client->HandleAuth( f.ValidCallbackResponse( ), "Test" );
        Assert::IsTrue(  client->IsSignedIn( ) );
        Assert::IsTrue(  client->IsAuthenticated( ) );
    
        client->SignOut( );
    
        Assert::IsTrue(  client->IsSignedIn( ) == false );
        Assert::IsTrue(  client->IsAuthenticated( ) == false );
    }
    
    TEST_METHOD(SignOut_Removes_Refresh_Token_From_AppConfig)
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
        Assert::IsTrue(  storedToken.empty( ) );
    }
    
    // ---------------------------------------------------------------------------
    // BuildAuthUrl
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(BuildAuthUrl_Contains_Client_Id_And_State)
    {
        OAuthFixture f;
        auto client = f.MakeClient( );
        const std::string url = client->BuildAuthUrl( "my_test_client" );
        Assert::IsTrue(  url.find( "client_id=my_test_client" ) != std::string::npos );
        Assert::IsTrue(  url.find( "state=" + k_KnownState ) != std::string::npos );
    }
    
    TEST_METHOD(BuildAuthUrl_Contains_Redirect_Uri_When_Configured)
    {
        OAuthFixture f;
        auto client = f.MakeClient( { }, { }, "http://localhost:8080" );
        const std::string url = client->BuildAuthUrl( "my_client" );
        Assert::IsTrue(  url.find( "redirect_uri=" ) != std::string::npos );
    }
    
    TEST_METHOD(BuildAuthUrl_Omits_Redirect_Uri_When_Config_Field_Is_Empty)
    {
        OAuthFixture f;
        auto client = f.MakeClient( { }, { }, "" );  // empty redirect URI
        const std::string url = client->BuildAuthUrl( "my_client" );
        Assert::IsTrue(  url.find( "redirect_uri=" ) == std::string::npos );
    }
    
    TEST_METHOD(BuildAuthUrl_Appends_Extra_Auth_Params_When_Configured)
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
        Assert::IsTrue(  url.find( "duration=permanent" ) != std::string::npos );
    }
    
    };
}
