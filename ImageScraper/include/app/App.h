#pragma once
#include "config/Config.h"
#include "ui/ConsoleInput.h"
#include "services/Service.h"
#include "traits/TypeTraits.h"
#include <memory>

class App : public NonCopyMovable
{
public:
    App( );
    int Run( );

private:
    Config m_Config{ };
    ConsoleInput m_UI{ };
    std::vector<std::shared_ptr<Service>> m_Services{ };
};

