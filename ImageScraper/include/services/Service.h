#pragma once

#include "services/ServiceOptionTypes.h"
#include "services/IServiceSink.h"

#include <string>
#include <memory>
#include <functional>

namespace ImageScraper
{
    class JsonFile;

    class Service
    {
    public:
        using AuthenticateCallback = std::function<void( ContentProvider, bool )>;

        Service( ContentProvider provider, std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
            : m_ContentProvider{ provider }
            , m_AppConfig{ appConfig }
            , m_UserConfig{ userConfig }
            , m_CaBundle{ caBundle }
            , m_OutputDir{ outputDir }
            , m_Sink{ sink }
        {
        }

        virtual ~Service( ) = default;
        virtual bool HandleUserInput( const UserInputOptions& options ) = 0;
        virtual bool OpenExternalAuth( ) = 0;
        virtual bool HandleExternalAuth( const std::string& response ) = 0;
        virtual bool IsSignedIn( ) const = 0;
        virtual void Authenticate( AuthenticateCallback callback ) = 0;
        virtual void SignOut( ) { }
        virtual std::string GetSignedInUser( ) const { return { }; }
        virtual bool HasRequiredCredentials( ) const { return true; }
        ContentProvider GetContentProvider( ) const { return m_ContentProvider; }

    protected:
        virtual bool IsCancelled( ) = 0;

        ContentProvider m_ContentProvider;
        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::string m_UserAgent{ "Windows:ImageScraper:v0.1:carlocgc1@gmail.com" };
        std::string m_CaBundle{ };
        std::string m_OutputDir{ };
        std::shared_ptr<IServiceSink> m_Sink{ nullptr };
    };
}