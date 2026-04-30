#include "network/RateLimitedHttpClient.h"
#include "log/Logger.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace
{
    constexpr int s_PollIntervalMs = 100;
    constexpr int s_CallbackThresholdSec = 2;
    constexpr int s_CancelledStatusCode = -1;
    const std::string s_EmptyKey{ };
}

ImageScraper::RateLimitedHttpClient::RateLimitedHttpClient(
    std::shared_ptr<IHttpClient> inner,
    RateLimitTable table,
    std::function<bool( )> isCancelled,
    std::shared_ptr<IServiceSink> sink,
    std::string serviceName )
    : m_Inner( std::move( inner ) )
    , m_Table( std::move( table ) )
    , m_IsCancelled( std::move( isCancelled ) )
    , m_Sink( std::move( sink ) )
    , m_ServiceName( std::move( serviceName ) )
{
}

ImageScraper::HttpResponse ImageScraper::RateLimitedHttpClient::Get( const HttpRequest& request, const std::string& rateLimitKey )
{
    return Execute( request, rateLimitKey, false );
}

ImageScraper::HttpResponse ImageScraper::RateLimitedHttpClient::Post( const HttpRequest& request, const std::string& rateLimitKey )
{
    return Execute( request, rateLimitKey, true );
}

ImageScraper::HttpResponse ImageScraper::RateLimitedHttpClient::Execute( const HttpRequest& request, const std::string& rateLimitKey, bool isPost )
{
    const std::string& effectiveKey = ResolveKey( rateLimitKey );
    const RateLimitSpec* spec = FindSpec( effectiveKey );

    // No spec at all (no entry under the key, no __default__) - pass through unrestricted.
    if( spec == nullptr || spec->empty( ) )
    {
        return isPost ? m_Inner->Post( request, rateLimitKey ) : m_Inner->Get( request, rateLimitKey );
    }

    bool callbackFired = false;

    while( true )
    {
        if( m_IsCancelled && m_IsCancelled( ) )
        {
            if( callbackFired && m_Sink )
            {
                m_Sink->OnRateLimitResume( m_ServiceName, effectiveKey );
            }
            return MakeCancelledResponse( );
        }

        int waitSeconds = 0;
        {
            std::lock_guard<std::mutex> lock( m_Mutex );
            Bucket& bucket = GetOrCreateBucket( effectiveKey, *spec );
            const auto now = std::chrono::steady_clock::now( );
            waitSeconds = ComputeWaitSeconds( bucket, *spec, now );

            if( waitSeconds <= 0 )
            {
                RecordRequest( bucket, now );
            }
        }

        if( waitSeconds <= 0 )
        {
            if( callbackFired && m_Sink )
            {
                m_Sink->OnRateLimitResume( m_ServiceName, effectiveKey );
            }
            return isPost ? m_Inner->Post( request, rateLimitKey ) : m_Inner->Get( request, rateLimitKey );
        }

        WarningLog( "[%s] Rate limit reached for %s/%s, waiting %is...", __FUNCTION__, m_ServiceName.c_str( ), effectiveKey.c_str( ), waitSeconds );

        if( !callbackFired && waitSeconds >= s_CallbackThresholdSec && m_Sink )
        {
            m_Sink->OnRateLimitWait( m_ServiceName, effectiveKey, waitSeconds );
            callbackFired = true;
        }

        if( !PollSleep( waitSeconds ) )
        {
            if( callbackFired && m_Sink )
            {
                m_Sink->OnRateLimitResume( m_ServiceName, effectiveKey );
            }
            return MakeCancelledResponse( );
        }
        // Loop and re-check; another thread (or the same one) may have consumed slots in the meantime.
    }
}

const std::string& ImageScraper::RateLimitedHttpClient::ResolveKey( const std::string& rateLimitKey ) const
{
    if( !rateLimitKey.empty( ) )
    {
        return rateLimitKey;
    }
    static const std::string defaultKey{ s_DefaultRateLimitKey };
    return defaultKey;
}

const ImageScraper::RateLimitSpec* ImageScraper::RateLimitedHttpClient::FindSpec( const std::string& key ) const
{
    auto it = m_Table.find( key );
    if( it != m_Table.end( ) )
    {
        return &it->second;
    }

    // Fall back to default bucket when the explicit key isn't registered.
    auto defaultIt = m_Table.find( s_DefaultRateLimitKey );
    if( defaultIt != m_Table.end( ) )
    {
        return &defaultIt->second;
    }

    return nullptr;
}

ImageScraper::RateLimitedHttpClient::Bucket& ImageScraper::RateLimitedHttpClient::GetOrCreateBucket( const std::string& key, const RateLimitSpec& spec )
{
    auto it = m_Buckets.find( key );
    if( it == m_Buckets.end( ) )
    {
        Bucket fresh{ };
        fresh.m_Windows.resize( spec.size( ) );
        it = m_Buckets.emplace( key, std::move( fresh ) ).first;
    }
    return it->second;
}

int ImageScraper::RateLimitedHttpClient::ComputeWaitSeconds( Bucket& bucket, const RateLimitSpec& spec, std::chrono::steady_clock::time_point now ) const
{
    int worstWait = 0;

    for( size_t i = 0; i < spec.size( ); ++i )
    {
        const RateLimitWindow& window = spec[ i ];
        std::deque<std::chrono::steady_clock::time_point>& timestamps = bucket.m_Windows[ i ];

        const auto windowStart = now - std::chrono::seconds( window.m_WindowSeconds );
        while( !timestamps.empty( ) && timestamps.front( ) < windowStart )
        {
            timestamps.pop_front( );
        }

        if( static_cast<int>( timestamps.size( ) ) >= window.m_MaxRequests )
        {
            const auto waitDuration = ( timestamps.front( ) + std::chrono::seconds( window.m_WindowSeconds ) ) - now;
            // Round up to the next whole second so we don't wake fractionally early.
            const auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>( waitDuration ).count( );
            const int waitSec = static_cast<int>( ( waitMs + 999 ) / 1000 );
            if( waitSec > worstWait )
            {
                worstWait = waitSec;
            }
        }
    }

    return worstWait;
}

void ImageScraper::RateLimitedHttpClient::RecordRequest( Bucket& bucket, std::chrono::steady_clock::time_point now ) const
{
    for( auto& timestamps : bucket.m_Windows )
    {
        timestamps.push_back( now );
    }
}

bool ImageScraper::RateLimitedHttpClient::PollSleep( int totalSeconds )
{
    const auto deadline = std::chrono::steady_clock::now( ) + std::chrono::seconds( totalSeconds );
    while( std::chrono::steady_clock::now( ) < deadline )
    {
        if( m_IsCancelled && m_IsCancelled( ) )
        {
            return false;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( s_PollIntervalMs ) );
    }
    return true;
}

ImageScraper::HttpResponse ImageScraper::RateLimitedHttpClient::MakeCancelledResponse( ) const
{
    HttpResponse response{ };
    response.m_StatusCode = s_CancelledStatusCode;
    response.m_Success = false;
    response.m_Error = "cancelled";
    return response;
}
