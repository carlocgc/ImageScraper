#pragma once

#include "network/HttpClientTypes.h"

#include <string>

namespace ImageScraper
{
    class IHttpClient
    {
    public:
        virtual ~IHttpClient( ) = default;
        virtual HttpResponse Get( const HttpRequest& request, const std::string& rateLimitKey = "" ) = 0;
        virtual HttpResponse Post( const HttpRequest& request, const std::string& rateLimitKey = "" ) = 0;
    };
}
