#pragma once

#include "services/Service.h"

#include <string>
#include <vector>

namespace ImageScraper
{
    class JsonFile;

    class RedgifsService : public Service
    {
    public:
        RedgifsService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver = nullptr );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;
        std::string GetProviderDisplayName( ) const override { return "Redgifs"; }
        std::string GetBrandColor( ) const override { return "#E8000F"; }

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
    };
}
