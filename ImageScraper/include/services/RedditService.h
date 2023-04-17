#pragma once

#include "services/Service.h"

#include <string>
#include <memory>
#include <chrono>

namespace ImageScraper
{
    class JsonFile;
    class FrontEnd;
    class FileConverter;

    class RedditService : public Service
    {
    public:
        RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUserInput( const UserInputOptions& options ) override;

    private:
        const bool IsAuthenticated( ) const;;
        void DownloadContent( const UserInputOptions& inputOptions );

        static const std::string s_UserAgent;
        static const std::string s_AppDataKey_DeviceId;
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;

        std::string m_DeviceId{ };
        std::string m_ClientId{ };
        std::string m_ClientSecret{ };

        std::string m_AuthAccessToken{ };
        std::chrono::seconds m_AuthExpireSeconds{ };
        std::chrono::system_clock::time_point m_TokenReceived{ };

        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };
    };
}
