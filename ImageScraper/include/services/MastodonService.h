#pragma once

#include "services/Service.h"

namespace ImageScraper
{
    class JsonFile;

    class MastodonService : public Service
    {
    public:
        MastodonService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;
        std::string GetProviderDisplayName( ) const override { return "Mastodon"; }
        std::string GetBrandColor( ) const override { return "#6364FF"; }

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
    };
}
