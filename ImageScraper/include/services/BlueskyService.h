#pragma once

#include "services/Service.h"
#include "utils/BlueskyUtils.h"

#include <optional>
#include <vector>

namespace ImageScraper
{
    class JsonFile;

    class BlueskyService : public Service
    {
    public:
        BlueskyService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver = nullptr );
        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;
        std::string GetProviderDisplayName( ) const override { return "Bluesky"; }
        std::string GetBrandColor( ) const override { return "#1185FE"; }

    protected:
        bool IsCancelled( ) override;

    private:
        std::optional<std::string> ResolveActorToDid( const std::string& actor );
        std::vector<BlueskyUtils::MediaItem> FetchAuthorFeedMedia( const std::string& actorDid, int maxItems );
        void DownloadContent( const UserInputOptions& inputOptions );
    };
}
