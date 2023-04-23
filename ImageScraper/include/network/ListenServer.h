#pragma once
#include "services/Service.h"
#include "traits/TypeTraits.h"

#include <vector>
#include <memory>
#include <atomic>

namespace ImageScraper
{
    class ListenServer : public NonCopyMovable
    {
    public:
        void Init( std::vector<std::shared_ptr<Service>> services, int port );
        void Start( );
        void Stop( );
        void Reset( );

    private:
        bool m_Initialised{ false };
        int m_Port{ 8080 };
        int m_CurrentRetries{ 0 };
        const int m_MaxRetries{ 20 };
        std::vector<std::shared_ptr<Service>> m_Services;
        std::atomic_bool m_Running{ false };
    };

}


