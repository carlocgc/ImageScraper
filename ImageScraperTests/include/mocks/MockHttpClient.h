#pragma once

#include "network/IHttpClient.h"

#include <string>

namespace ImageScraper
{
    class MockHttpClient : public IHttpClient
    {
    public:
        HttpResponse m_Response{ };
        HttpRequest  m_LastRequest{ };
        std::string  m_LastRateLimitKey{ };
        int          m_CallCount{ 0 };

        HttpResponse Get( const HttpRequest& request, const std::string& rateLimitKey = "" ) override
        {
            m_LastRequest = request;
            m_LastRateLimitKey = rateLimitKey;
            ++m_CallCount;
            return m_Response;
        }

        HttpResponse Post( const HttpRequest& request, const std::string& rateLimitKey = "" ) override
        {
            m_LastRequest = request;
            m_LastRateLimitKey = rateLimitKey;
            ++m_CallCount;
            return m_Response;
        }
    };
}
