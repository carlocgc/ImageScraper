#pragma once

#include <string>

namespace ImageScraper
{
    class Config;
    class FrontEnd;

    class Service
    {
    public:
        virtual ~Service( ) = default;
        virtual bool HandleUrl( const Config& config, const FrontEnd& frontEnd, const std::string& url ) = 0;
    };
}