#pragma once

#include "network/IHttpClient.h"

namespace ImageScraper
{
    class CurlHttpClient : public IHttpClient
    {
    public:
        HttpResponse Get( const HttpRequest& request ) override;
        HttpResponse Post( const HttpRequest& request ) override;

    private:
        HttpResponse Execute( const HttpRequest& request, bool isPost );
    };
}
