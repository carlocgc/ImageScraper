#pragma once

#include "network/IHttpClient.h"
#include "network/IUrlResolver.h"
#include "network/RedgifsTokenCache.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace ImageScraper
{
    // Resolves redgifs.com watch / ifr URLs to direct CDN .mp4 URLs. Owns a
    // RedgifsTokenCache so a session of resolutions only authenticates once.
    // Thread-safe: a single instance is shared across services that may run
    // concurrently in the service task pool.
    class RedgifsUrlResolver final : public IUrlResolver
    {
    public:
        RedgifsUrlResolver( std::shared_ptr<IHttpClient> client,
                            std::string caBundle,
                            std::string userAgent );

        bool CanHandle( const std::string& url ) const override;
        std::optional<std::string> Resolve( const std::string& url ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient;
        std::string                  m_CaBundle;
        std::string                  m_UserAgent;
        RedgifsTokenCache            m_TokenCache;
        mutable std::mutex           m_Mutex;
    };
}
