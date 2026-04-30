#pragma once

#include "network/IHttpClient.h"

namespace ImageScraper
{
    class CurlHttpClient : public IHttpClient
    {
    public:
        HttpResponse Get( const HttpRequest& request, const std::string& rateLimitKey = "" ) override;
        HttpResponse Post( const HttpRequest& request, const std::string& rateLimitKey = "" ) override;

    private:
        HttpResponse Execute( const HttpRequest& request, bool isPost );
    };
}
