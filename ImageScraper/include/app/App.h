#pragma once

#include "traits/TypeTraits.h"

#include <memory>
#include <vector>
#include <string>

namespace ImageScraper
{
    class JsonFile;
    class FrontEnd;
    class Service;
    class FileConverter;

    class App : public NonCopyMovable
    {
    public:
        App( );
        int Run( );

        static const std::string s_AppConfigFile;
        static const std::string s_UserConfigFile;
        static const std::string s_CaBundleFile;

    private:
        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };

        std::vector<std::shared_ptr<Service>> m_Services{ };
    };
}


