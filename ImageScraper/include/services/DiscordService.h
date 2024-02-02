#pragma once
#include "services/Service.h"
#include "nlohmann/json.hpp"

#include <string>

namespace ImageScraper
{
    using Json = nlohmann::json;

    class JsonFile;
    class FrontEnd;

    class DiscordService : public Service
    {
    public:
        DiscordService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;

    private:
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;

        std::string m_ClientId{ };
        std::string m_ClientSecret{ };
    protected:
        bool IsCancelled( ) override;

    };

}