#pragma once

#include "services/Service.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>

namespace ImageScraper
{
    using Json = nlohmann::json;

    class FrontEnd;
    class JsonFile;

    class FourChanService : public Service
    {
    public:
        FourChanService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        int GetPageCountForBoard( const std::string& board, const Json& response );
        std::vector<std::string> GetFileNamesFromResponse( const Json& response );
    };
}