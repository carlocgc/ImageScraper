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
    class ListenServer;

    class App : public NonCopyMovable
    {
    public:
        App( );
        int Run( );

        static const std::string s_AppConfigFile;
        static const std::string s_UserConfigFile;
        static const std::string s_CaBundleFile;

    private:
        void AuthenticateServices( );

        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };
        std::shared_ptr<ListenServer> m_ListenServer{ nullptr };

        std::vector<std::shared_ptr<Service>> m_Services{ };

        int m_AuthenticatingCount{ 0 };
    };
}


