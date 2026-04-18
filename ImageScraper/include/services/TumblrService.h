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
        bool HasSignInCredentials( ) const override;
        std::string GetProviderDisplayName( ) const override { return "Tumblr"; }
        std::string GetBrandColor( ) const override { return "#35465C"; }

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        std::vector<std::string> GetMediaUrlsFromResponse( const Json& response, int maxItems );
        void FetchCurrentUser( );

        static const std::string s_RedirectUrl;
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;
        static const std::string s_AppDataKey_RefreshToken;
        static const std::string s_AppDataKey_StateId;

        mutable std::mutex m_UsernameMutex{ };
        std::string m_Username{ };

        std::unique_ptr<OAuthClient> m_OAuthClient;
    };
}
