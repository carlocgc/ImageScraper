#pragma once

#include <string>
#include <memory>

#include "services/ServiceOptionTypes.h"

namespace ImageScraper
{
    class JsonFile;

    class Service
    {
    public:
        Service( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle )
            : m_AppConfig{ appConfig }
            , m_UserConfig{ userConfig }
            , m_CaBundle{ caBundle }
        {
        }

        virtual ~Service( ) = default;
        virtual bool HandleUserInput( const UserInputOptions& options ) = 0;

    protected:
        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::string m_CaBundle{ };
    };
}