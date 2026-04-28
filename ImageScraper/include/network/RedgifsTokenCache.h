#pragma once

#include "network/IHttpClient.h"

#include <chrono>
#include <memory>
#include <string>

namespace ImageScraper
{
    // Caches a redgifs anonymous /v2/auth/temporary token for the lifetime
    // of one download batch. Construction is free - the auth call only
    // happens on the first EnsureToken() call. Not thread-safe.
    class RedgifsTokenCache
    {
    public:
        RedgifsTokenCache( std::shared_ptr<IHttpClient> client,
                           std::string caBundle,
                           std::string userAgent );

        // Lazily fetches a token if one is not cached or has expired.
        // Returns true if a usable token is available afterwards.
        bool EnsureToken( );

        const std::string& Token( ) const { return m_Token; }

    private:
        std::shared_ptr<IHttpClient>          m_HttpClient;
        std::string                           m_CaBundle;
        std::string                           m_UserAgent;
        std::string                           m_Token{ };
        std::chrono::steady_clock::time_point m_TokenExpiresAt{ };
    };
}
