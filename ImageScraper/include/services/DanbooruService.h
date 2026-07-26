#pragma once

#include "services/Service.h"
#include "utils/DanbooruUtils.h"

#include <optional>
#include <string>
#include <vector>

namespace ImageScraper
{
    class JsonFile;

    class DanbooruService : public Service
    {
    public:
        DanbooruService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver = nullptr );

        bool HandleUserInput( const UserInputOptions& options ) override;
        bool OpenExternalAuth( ) override;
        bool HandleExternalAuth( const std::string& response ) override;
        bool IsSignedIn( ) const override;
        void Authenticate( AuthenticateCallback callback ) override;
        std::string GetProviderDisplayName( ) const override { return "Danbooru"; }
        std::string GetBrandColor( ) const override { return "#4C89D9"; }

    protected:
        bool IsCancelled( ) override;

    private:
        void DownloadContent( const UserInputOptions& inputOptions );
        bool FetchSinglePost( const std::string& postId, std::optional<DanbooruUtils::PreparedDownload>& downloadOut );
        std::optional<std::vector<DanbooruUtils::PreparedDownload>> FetchQueryPosts( const std::string& query, int maxItems );
    };
}
