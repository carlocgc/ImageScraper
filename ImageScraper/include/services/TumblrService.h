#pragma once

#include "services/Service.h"

#include <string>

namespace ImageScraper
{
    class FrontEnd;
    class JsonFile;

    class TumblrService : public Service
    {
    public:
        TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUserInput( const UserInputOptions& options ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        static const std::string s_UserDataKey_ApiKey;
        std::string m_ApiKey{ };
    };
}