#pragma once
#include "config/Config.h"
#include "services/Service.h"
#include "traits/TypeTraits.h"
#include "ui/FrontEnd.h"

#include <memory>

namespace ImageScraper
{
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


