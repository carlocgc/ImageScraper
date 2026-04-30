#pragma once

#include "services/ServiceOptionTypes.h"

#include <string>

namespace ImageScraper
{
    inline constexpr int INVALID_CONTENT_PROVIDER = -1;
    class IServiceSink
    {
    public:
        virtual ~IServiceSink( ) = default;

        // Returns true if the user has requested cancellation
        virtual bool IsCancelled( ) = 0;

        // Called by a service when its run finishes (success or failure) - unlocks the UI
        virtual void OnRunComplete( ) = 0;

        // Per-file download progress in [0, 1]
        virtual void OnCurrentDownloadProgress( float progress ) = 0;

        // Overall batch progress - current file index out of total
        virtual void OnTotalDownloadProgress( int current, int total ) = 0;

        // Returns the ContentProvider currently performing OAuth sign-in, or INVALID_CONTENT_PROVIDER
        virtual int GetSigningInProvider( ) = 0;

        // Called when OAuth sign-in for the given provider completes (success or failure)
        virtual void OnSignInComplete( ContentProvider provider ) = 0;

        // Called after each file is successfully written to disk
        virtual void OnFileDownloaded( const std::string& filepath, const std::string& sourceUrl ) = 0;

        // Called when a service request begins waiting on a rate limit (only fires for waits past a threshold).
        // Default-empty so existing implementors do not have to override unless they want UI feedback.
        virtual void OnRateLimitWait( const std::string& /*serviceName*/, const std::string& /*rateLimitKey*/, int /*waitSeconds*/ ) { }

        // Called when the corresponding wait is over (admission granted or cancelled).
        virtual void OnRateLimitResume( const std::string& /*serviceName*/, const std::string& /*rateLimitKey*/ ) { }
    };
}
