#pragma once
#include "config/Config.h"

#include <string>

namespace ImageScraper
{
    class Service
    {
    public:
        virtual ~Service( ) { };
        virtual bool HandleUrl( const Config& config, const std::string& url ) = 0;
    };
}