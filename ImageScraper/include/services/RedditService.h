#pragma once
#include "services/Service.h"
#include "auth/OAuthClient.h"
#include "nlohmann/json.hpp"

#include <string>
#include <memory>
#include <mutex>

namespace ImageScraper
{
    using Json = nlohmann::json;

    class JsonFile;

    class RedditService : public Service
    {
    public:
        RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;
        void SignOut( ) override;
        std::string GetSignedInUser( ) const override;
        bool HasRequiredCredentials( ) const override;
        std::string GetProviderDisplayName( ) const override { return "Reddit"; }
        std::string GetBrandColor( ) const override { return "#FF4500"; }

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        bool TryPerformAppOnlyAuth( );
        std::vector<std::string> GetMediaUrls( const Json& postData );
        void FetchCurrentUser( );

        static const std::string s_RedirectUrl;
        static const std::string s_AppDataKey_DeviceId;
        static const std::string s_AppDataKey_RefreshToken;
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;

        // Used for pagination
        std::string m_AfterParam{ };

        mutable std::mutex m_UsernameMutex{ };
        std::string m_Username{ };

        std::unique_ptr<OAuthClient> m_OAuthClient;
    };
}
