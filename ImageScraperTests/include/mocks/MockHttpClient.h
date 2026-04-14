#pragma once

#include "network/IHttpClient.h"

namespace ImageScraper
{
    class MockHttpClient : public IHttpClient
    {
    public:
        HttpResponse m_Response{ };
        HttpRequest  m_LastRequest{ };
        int          m_CallCount{ 0 };

        HttpResponse Get( const HttpRequest& request ) override
        {
            m_LastRequest = request;
            ++m_CallCount;
            return m_Response;
        }

        HttpResponse Post( const HttpRequest& request ) override
        {
            m_LastRequest = request;
            ++m_CallCount;
            return m_Response;
        }
    };
}
