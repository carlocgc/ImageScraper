#pragma once

#include "services/Service.h"
#include "utils/MastodonUtils.h"

#include <optional>
#include <vector>

namespace ImageScraper
{
    class JsonFile;

    class MastodonService : public Service
    {
    public:
        MastodonService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver = nullptr );
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
        std::optional<MastodonUtils::Account> ResolveAccount( const std::string& instanceUrl, const std::string& accountInput );
        std::vector<MastodonUtils::MediaItem> FetchAccountStatusMedia( const std::string& instanceUrl, const std::string& accountId, const std::string& accountInput, int maxItems );
        void DownloadContent( const UserInputOptions& inputOptions );
    };
}
