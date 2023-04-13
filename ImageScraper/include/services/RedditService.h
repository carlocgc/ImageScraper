#pragma once

#include "services/Service.h"

#include <string>
#include <memory>

namespace ImageScraper
{
    class JsonFile;
    class FrontEnd;
    class FileConverter;

    class RedditService : public Service
    {
    public:
        RedditService( const std::string& userAgent, const std::string& caBundle, std::shared_ptr<JsonFile> appConfig, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUrl( const std::string& url ) override;

    private:
        void DownloadHotReddit( const std::string& subreddit );

        static const std::string s_UserAgent;

        static const std::string s_AppDataKey_DeviceId;
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;

        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };

        std::string m_DeviceId{ };
        std::string m_ClientSecret{ };
    };
}
