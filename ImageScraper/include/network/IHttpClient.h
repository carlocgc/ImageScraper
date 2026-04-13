#pragma once

#include "network/HttpClientTypes.h"

namespace ImageScraper
{
    class IHttpClient
    {
    public:
        virtual ~IHttpClient( ) = default;
        virtual HttpResponse Get( const HttpRequest& request ) = 0;
        virtual HttpResponse Post( const HttpRequest& request ) = 0;
    };
}
