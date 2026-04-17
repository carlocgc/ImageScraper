#pragma once

#include "services/Service.h"
#include "nlohmann/json.hpp"

#include <string>
#include <memory>
#include <chrono>
#include <mutex>

namespace ImageScraper
{
    using Json = nlohmann::json;

    class JsonFile;

    class TumblrService : public Service
    {
    public:
        TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;
        void SignOut( ) override;
        std::string GetSignedInUser( ) const override;
        bool HasRequiredCredentials( ) const override;

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        std::vector<std::string> GetMediaUrlsFromResponse( const Json& response, int maxItems );

        void FetchAccessToken( const std::string& authCode );
        bool TryPerformAuthTokenRefresh( );
        void FetchCurrentUser( );
        bool IsAuthenticated( ) const;
        void ClearAccessToken( );
        void ClearRefreshToken( );
        bool TryParseAccessTokenAndExpiry( const Json& response );
        bool TryParseRefreshToken( const Json& response );

        static const std::string s_RedirectUrl;
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;
        static const std::string s_AppDataKey_RefreshToken;
        static const std::string s_AppDataKey_StateId;
        static const std::string s_UserDataKey_ApiKey;

        std::string m_StateId{ };

        mutable std::mutex m_UsernameMutex{ };
        std::string m_Username{ };

        mutable std::mutex m_RefreshTokenMutex{ };
        std::string m_RefreshToken{ };
        mutable std::mutex m_AccessTokenMutex{ };
        std::string m_AccessToken{ };
        std::chrono::seconds m_AuthExpireSeconds{ };
        std::chrono::system_clock::time_point m_TokenReceived{ };
    };
}
