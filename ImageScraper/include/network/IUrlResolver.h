#pragma once

#include <optional>
#include <string>

namespace ImageScraper
{
    // A pluggable URL pre-processor used by Service::DownloadMediaUrls. When a
    // scraped URL is not directly downloadable (e.g. redgifs watch pages
    // resolve to CDN .mp4 URLs), an IUrlResolver translates it before the
    // download step. Implementations should be safe to call concurrently
    // because one resolver instance is shared across services that may run
    // on different threads in the service task pool.
    class IUrlResolver
    {
    public:
        virtual ~IUrlResolver( ) = default;

        // Cheap predicate. Must not perform I/O. Used by the base service
        // to decide whether to dispatch to Resolve.
        virtual bool CanHandle( const std::string& url ) const = 0;

        // Translate the given URL to a directly downloadable one. Returns
        // std::nullopt on any failure (auth, lookup, parse). The base
        // service will skip URLs that fail to resolve.
        virtual std::optional<std::string> Resolve( const std::string& url ) = 0;
    };
}
