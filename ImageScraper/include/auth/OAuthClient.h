#pragma once

#include "requests/RequestTypes.h"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ImageScraper
{
    class JsonFile;

    // Configuration supplied by each OAuth2 provider at construction time.
    struct OAuthConfig
    {
        std::string m_AuthUrl;              // e.g. "https://www.reddit.com/api/v1/authorize"
        std::string m_Scopes;               // e.g. "identity,read"
        std::string m_ClientIdKey;          // userConfig key for the client ID
        std::string m_ClientSecretKey;      // userConfig key for the client secret
        std::string m_StateIdAppKey;        // appConfig key used to persist the state/device ID
        std::string m_RefreshTokenAppKey;   // appConfig key used to persist the refresh token
        std::string m_RedirectUri;          // redirect URI, or "" to omit (Tumblr quirk)
        std::vector<QueryParam> m_ExtraAuthParams; // additional query params appended to the auth URL
    };

    // Shared OAuth2 Authorization Code flow component.
    // Composed (not inherited) into each provider service.
    class OAuthClient
    {
    public:
        using TokenRequestFn = std::function<RequestResult( const RequestOptions& )>;

        OAuthClient( OAuthConfig config,
                     std::shared_ptr<JsonFile> appConfig,
                     std::shared_ptr<JsonFile> userConfig,
                     std::string caBundle,
                     std::string userAgent,
                     TokenRequestFn fetchToken,
                     TokenRequestFn refreshToken );

        // Builds the OAuth authorization URL and launches it in the default browser.
        bool OpenAuth( );

        // Constructs and returns the authorization URL for the given client ID.
        // Separated from OpenAuth so that URL composition can be tested independently.
        std::string BuildAuthUrl( const std::string& clientId ) const;

        // Validates an OAuth callback response string and - if valid - synchronously
        // performs the token exchange HTTP request and stores the resulting tokens.
        // Must be called on a background thread (it blocks on the HTTP request).
        // Returns true only when all checks pass and the tokens are stored.
        bool HandleAuth( const std::string& response, const std::string& providerName );

        // Returns true when a refresh token is stored (user has completed OAuth sign-in).
        bool IsSignedIn( ) const;

        // Returns true when an access token is stored and has not yet expired.
        bool IsAuthenticated( ) const;

        // Synchronously refreshes the access token using the stored refresh token.
        // Clears the refresh token internally on failure.
        // Must be called on a background thread.
        bool TryRefreshToken( );

        // Clears all tokens from memory and removes the refresh token from appConfig.
        void SignOut( );

        // Returns the current access token (thread-safe copy).
        std::string GetAccessToken( ) const;

        // Stores an access token and expiry directly (used for app-only auth flows
        // that do not issue a refresh token, e.g. Reddit app-only auth).
        void StoreAccessToken( const std::string& token, int expireSeconds );

    private:
        void LoadOrGenerateStateId( );
        bool FetchAccessToken( const std::string& code );
        bool ParseAndStoreTokenResponse( const std::string& rawResponse );
        void ClearAccessToken( );
        void ClearRefreshToken( );

        OAuthConfig m_Config;
        std::shared_ptr<JsonFile> m_AppConfig;
        std::shared_ptr<JsonFile> m_UserConfig;
        std::string m_CaBundle;
        std::string m_UserAgent;
        TokenRequestFn m_FetchTokenFn;
        TokenRequestFn m_RefreshTokenFn;

        std::string m_StateId;

        mutable std::mutex m_AccessTokenMutex;
        std::string m_AccessToken;
        std::chrono::seconds m_AuthExpireSeconds{ 0 };
        std::chrono::system_clock::time_point m_TokenReceived;

        mutable std::mutex m_RefreshTokenMutex;
        std::string m_RefreshToken;
    };
}
