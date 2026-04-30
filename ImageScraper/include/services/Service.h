#pragma once

#include "services/ServiceOptionTypes.h"
#include "services/IServiceSink.h"
#include "network/RateLimitTypes.h"

#include <filesystem>
#include <optional>
#include <string>
#include <memory>
#include <functional>
#include <cstdint>
#include <vector>

namespace ImageScraper
{
    class JsonFile;
    class IHttpClient;
    class IUrlResolver;

    class Service
    {
    public:
        using AuthenticateCallback = std::function<void( ContentProvider, bool )>;

        Service( ContentProvider provider, std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, RateLimitTable rateLimits = { }, std::shared_ptr<IUrlResolver> urlResolver = nullptr );

        // The user agent string sent on every HTTP request the services make.
        // Exposed so external code (e.g. App.cpp constructing shared resolvers)
        // can use the same value as the services themselves.
        static constexpr const char* DefaultUserAgent( ) { return "Windows:ImageScraper:v0.1:carlocgc1@gmail.com"; }

        virtual ~Service( ) = default;
        virtual bool HandleUserInput( const UserInputOptions& options ) = 0;
        virtual bool OpenExternalAuth( ) = 0;
        virtual bool HandleExternalAuth( const std::string& response ) = 0;
        virtual bool IsSignedIn( ) const = 0;
        virtual void Authenticate( AuthenticateCallback callback ) = 0;
        virtual void SignOut( ) { }
        virtual std::string GetSignedInUser( ) const { return { }; }
        virtual bool HasRequiredCredentials( ) const { return true; }
        // Returns true when the service has the additional credentials needed to initiate sign-in
        // (e.g. a client secret on top of the api key). Defaults to HasRequiredCredentials().
        virtual bool HasSignInCredentials( ) const { return HasRequiredCredentials( ); }
        virtual std::string GetProviderDisplayName( ) const { return "Service"; }
        virtual std::string GetBrandColor( ) const { return "#888888"; }
        ContentProvider GetContentProvider( ) const { return m_ContentProvider; }

    protected:
        enum class DownloadMethod : uint8_t
        {
            DirectFile = 0,
            HlsVideo = 1
        };

        struct MediaDownload
        {
            std::string m_SourceUrl{ };
            std::string m_FileName{ };
            DownloadMethod m_Method{ DownloadMethod::DirectFile };
        };

        std::optional<int> DownloadMedia( const std::vector<MediaDownload>& downloads, const std::filesystem::path& dir );
        std::optional<int> DownloadMediaUrls( std::vector<std::string>& mediaUrls, const std::filesystem::path& dir );
        virtual bool IsCancelled( ) = 0;

        ContentProvider m_ContentProvider;
        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::string m_UserAgent{ DefaultUserAgent( ) };
        std::string m_CaBundle{ };
        std::string m_OutputDir{ };
        std::shared_ptr<IServiceSink> m_Sink{ nullptr };
        std::shared_ptr<IHttpClient> m_HttpClient{ nullptr };
        std::shared_ptr<IUrlResolver> m_UrlResolver{ nullptr };
    };
}
