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
        RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUrl( const std::string& url ) override;

    private:
        void DownloadHotReddit( const std::string& subreddit );

        static const std::string s_UserAgent;
        static const std::string s_AppDataKey_DeviceId;
        static const std::string s_UserDataKey_ClientId;
        static const std::string s_UserDataKey_ClientSecret;

        std::string m_DeviceId{ };
        std::string m_ClientId{ };
        std::string m_ClientSecret{ };

        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };
    };
}
