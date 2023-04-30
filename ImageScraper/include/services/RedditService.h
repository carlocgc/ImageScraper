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
    class FrontEnd;

    class RedditService : public Service
    {
    public:
        RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;

    protected:
        bool IsCancelled( ) override;

    private:
        const bool IsAuthenticated( ) const;
        void FetchAccessToken( const std::string& authCode );
        void DownloadContent( const UserInputOptions& inputOptions );
        bool TryPerformAppOnlyAuth( );
        bool TryPerformAuthTokenRefresh( );
        std::vector<std::string> GetMediaUrls( const Json& postData );
        bool TryParseAccessTokenAndExpiry( const Json& response );
        bool TryParseRefreshToken( const Json& response );
        void ClearRefreshToken( );
        void ClearAccessToken( );

        static const std::string s_RedirectUrl;
        static const std::string s_AppDataKey_DeviceId;
        static const std::string s_AppDataKey_RefreshToken;
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;

        std::string m_DeviceId{ };
        std::string m_ClientId{ };
        std::string m_ClientSecret{ };

        // Used for pagination
        std::string m_AfterParam{ };

        std::mutex m_RefreshTokenMutex{ };
        std::string m_RefreshToken{ };
        mutable std::mutex m_AccessTokenMutex{ };
        std::string m_AccessToken{ };
        std::chrono::seconds m_AuthExpireSeconds{ };
        std::chrono::system_clock::time_point m_TokenReceived{ };
    };
}
