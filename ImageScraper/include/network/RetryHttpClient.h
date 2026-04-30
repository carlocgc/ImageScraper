#pragma once

#include "network/IHttpClient.h"

#include <memory>
#include <string>

namespace ImageScraper
{
    class RetryHttpClient : public IHttpClient
    {
    public:
        RetryHttpClient( std::shared_ptr<IHttpClient> inner, int maxRetries = 3, int retryDelaySeconds = 1 );

        HttpResponse Get( const HttpRequest& request, const std::string& rateLimitKey = "" ) override;
        HttpResponse Post( const HttpRequest& request, const std::string& rateLimitKey = "" ) override;

    private:
        HttpResponse Execute( const HttpRequest& request, const std::string& rateLimitKey, bool isPost );

        std::shared_ptr<IHttpClient> m_Inner{ };
        int m_MaxRetries{ 3 };
        int m_RetryDelaySeconds{ 1 };
    };
}
