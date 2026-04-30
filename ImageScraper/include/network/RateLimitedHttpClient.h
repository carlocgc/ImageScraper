#pragma once

#include "network/IHttpClient.h"
#include "network/RateLimitTypes.h"
#include "services/IServiceSink.h"

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ImageScraper
{
    // Outer-most decorator on the HTTP stack. Sleeps the calling thread until each window for the
    // selected endpoint key has room (sliding-window admission control). 429s and transient errors
    // are NOT this class's concern - those stay in RetryHttpClient.
    class RateLimitedHttpClient : public IHttpClient
    {
    public:
        RateLimitedHttpClient(
            std::shared_ptr<IHttpClient> inner,
            RateLimitTable table,
            std::function<bool( )> isCancelled,
            std::shared_ptr<IServiceSink> sink,
            std::string serviceName );

        HttpResponse Get( const HttpRequest& request, const std::string& rateLimitKey = "" ) override;
        HttpResponse Post( const HttpRequest& request, const std::string& rateLimitKey = "" ) override;

    private:
        struct Bucket
        {
            // One deque per window in the spec; deques hold timestamps of admitted requests.
            std::vector<std::deque<std::chrono::steady_clock::time_point>> m_Windows;
        };

        HttpResponse Execute( const HttpRequest& request, const std::string& rateLimitKey, bool isPost );
        const std::string& ResolveKey( const std::string& rateLimitKey ) const;
        const RateLimitSpec* FindSpec( const std::string& key ) const;
        Bucket& GetOrCreateBucket( const std::string& key, const RateLimitSpec& spec );
        int ComputeWaitSeconds( Bucket& bucket, const RateLimitSpec& spec, std::chrono::steady_clock::time_point now ) const;
        void RecordRequest( Bucket& bucket, std::chrono::steady_clock::time_point now ) const;
        bool PollSleep( int totalSeconds );
        HttpResponse MakeCancelledResponse( ) const;

        std::shared_ptr<IHttpClient> m_Inner;
        RateLimitTable m_Table;
        std::function<bool( )> m_IsCancelled;
        std::shared_ptr<IServiceSink> m_Sink;
        std::string m_ServiceName;

        std::unordered_map<std::string, Bucket> m_Buckets;
        std::mutex m_Mutex;
    };
}
