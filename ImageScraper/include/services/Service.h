#pragma once

#include "services/ServiceOptionTypes.h"

#include <string>
#include <memory>

namespace ImageScraper
{
    class JsonFile;
    class FrontEnd;

    class Service
    {
    public:
        Service( ContentProvider provider, std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
            : m_ContentProvider{ provider }
            , m_AppConfig{ appConfig }
            , m_UserConfig{ userConfig }
            , m_CaBundle{ caBundle }
            , m_FrontEnd{ frontEnd }
        {
        }

        virtual ~Service( ) = default;
        virtual bool HandleUserInput( const UserInputOptions& options ) = 0;
        virtual void OpenSignInWindow( ) = 0;
        ContentProvider GetContentProvider( ) { return m_ContentProvider; }

    protected:
        virtual bool IsCancelled( ) = 0;

        ContentProvider m_ContentProvider;
        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::string m_UserAgent{ "Windows:ImageScraper:v0.1:carlocgc1@gmail.com" };
        std::string m_CaBundle{ };
        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };
        bool m_IsAuthenticated{ false };
    };
}