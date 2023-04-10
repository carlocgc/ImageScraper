#pragma once

#include "traits/TypeTraits.h"

#include <memory>
#include <vector>

namespace ImageScraper
{
    class Config;
    class FrontEnd;
    class Service;

    class App : public NonCopyMovable
    {
    public:
        App( );
        int Run( );

    private:
        std::shared_ptr<Config> m_Config{ nullptr };
        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };
        std::vector<std::shared_ptr<Service>> m_Services{ };
    };
}


