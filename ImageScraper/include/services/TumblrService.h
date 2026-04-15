#pragma once

#include "services/Service.h"
#include "nlohmann/json.hpp"

#include <string>


namespace ImageScraper
{
    using Json = nlohmann::json;

    class JsonFile;

    class TumblrService : public Service
    {
    public:
        TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<IServiceSink> sink );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;
        bool HasRequiredCredentials( ) const override;

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        std::vector<std::string> GetMediaUrlsFromResponse( const Json& response, int maxItems );

        static const std::string s_UserDataKey_ApiKey;
    };
}