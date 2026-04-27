#pragma once

#include "network/IHttpClient.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace ImageScraper
{
    // Resolves redgifs.com watch URLs (which are HTML pages, not media) into
    // direct CDN .mp4 URLs via the redgifs public API. Holds a temporary auth
    // token so a batch of resolutions only authenticates once.
    //
    // Construct one resolver per download batch; the token is not thread-safe.
    class RedgifsResolver
    {
    public:
        RedgifsResolver( std::shared_ptr<IHttpClient> client,
                         std::string caBundle,
                         std::string userAgent );

        // True for any URL pointing at a redgifs watch page, e.g.:
        //   https://redgifs.com/watch/<slug>
        //   https://www.redgifs.com/watch/<slug>
        //   https://v3.redgifs.com/watch/<slug>
        // The /ifr/ embed form is also accepted.
        static bool IsRedgifsUrl( const std::string& url );

        // Pulls the lowercased gif slug from a redgifs URL. Returns empty
        // string if no slug can be parsed. Pure - no HTTP.
        static std::string ExtractSlug( const std::string& url );

        // Resolves a redgifs watch URL to a direct CDN .mp4 URL (HD when
        // available, falling back to SD). Returns nullopt if the URL is
        // unrecognised, auth fails, the gif is gone, or the API response
        // does not include a usable URL.
        std::optional<std::string> Resolve( const std::string& watchUrl );

    private:
        bool EnsureToken( );

        std::shared_ptr<IHttpClient>                m_HttpClient;
        std::string                                 m_CaBundle;
        std::string                                 m_UserAgent;
        std::string                                 m_Token{ };
        std::chrono::steady_clock::time_point       m_TokenExpiresAt{ };
    };
}
