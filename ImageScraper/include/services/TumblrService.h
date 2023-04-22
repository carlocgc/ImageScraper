#pragma once

#include "services/Service.h"
#include "nlohmann/json.hpp"

#include <string>


namespace ImageScraper
{
    using Json = nlohmann::json;

    class FrontEnd;
    class JsonFile;

    class TumblrService : public Service
    {
    public:
        TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        std::vector<std::string> GetMediaUrlsFromResponse( const Json& response );

        static const std::string s_UserDataKey_ApiKey;
        std::string m_ApiKey{ };
    };
}