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
        Service( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
            : m_AppConfig{ appConfig }
            , m_UserConfig{ userConfig }
            , m_CaBundle{ caBundle }
            , m_FrontEnd{ frontEnd }
        {
        }

        virtual ~Service( ) = default;
        virtual bool HandleUserInput( const UserInputOptions& options ) = 0;

    protected:
        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::string m_UserAgent{ "Windows:ImageScraper:v0.1:carlocgc1@gmail.com" };
        std::string m_CaBundle{ };
        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };
    };
}