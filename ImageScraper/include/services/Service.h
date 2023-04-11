#pragma once

#include <string>

namespace ImageScraper
{
    class Service
    {
    public:
        Service( const std::string& userAgent, const std::string& caBundle )
            : m_UserAgent{ userAgent }
            , m_CaBundle{ caBundle }
        {
        }

        virtual ~Service( ) = default;
        virtual bool HandleUrl( const std::string& url ) = 0;

    protected:
        std::string m_UserAgent;
        std::string m_CaBundle;
    };
}