#pragma once

#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper
{
    class RetryHttpClient : public IHttpClient
    {
    public:
        RetryHttpClient( std::shared_ptr<IHttpClient> inner, int maxRetries = 3, int retryDelaySeconds = 1 );

        HttpResponse Get( const HttpRequest& request ) override;
        HttpResponse Post( const HttpRequest& request ) override;

    private:
        HttpResponse Execute( const HttpRequest& request, bool isPost );

        std::shared_ptr<IHttpClient> m_Inner{ };
        int m_MaxRetries{ 3 };
        int m_RetryDelaySeconds{ 1 };
    };
}
