#include "network/RetryHttpClient.h"
#include "log/Logger.h"

#include <thread>
#include <chrono>

static constexpr int s_RateLimitCode = 429;
static constexpr int s_RateLimitDelaySeconds = 5;

ImageScraper::RetryHttpClient::RetryHttpClient( std::shared_ptr<IHttpClient> inner, int maxRetries, int retryDelaySeconds )
    : m_Inner( std::move( inner ) )
    , m_MaxRetries( maxRetries )
    , m_RetryDelaySeconds( retryDelaySeconds )
{
}

ImageScraper::HttpResponse ImageScraper::RetryHttpClient::Get( const HttpRequest& request )
{
    return Execute( request, false );
}

ImageScraper::HttpResponse ImageScraper::RetryHttpClient::Post( const HttpRequest& request )
{
    return Execute( request, true );
}

ImageScraper::HttpResponse ImageScraper::RetryHttpClient::Execute( const HttpRequest& request, bool isPost )
{
    HttpResponse response{ };

    for( int attempt = 0; attempt <= m_MaxRetries; ++attempt )
    {
        response = isPost ? m_Inner->Post( request ) : m_Inner->Get( request );

        if( response.m_Success )
        {
            return response;
        }

        if( response.m_StatusCode == s_RateLimitCode )
        {
            WarningLog( "[%s] Rate limited (429), waiting %is before retry (attempt %i/%i)...", __FUNCTION__, s_RateLimitDelaySeconds, attempt + 1, m_MaxRetries );
            std::this_thread::sleep_for( std::chrono::seconds( s_RateLimitDelaySeconds ) );
            continue;
        }

        // Don't retry on client errors (4xx) other than 429
        if( response.m_StatusCode >= 400 && response.m_StatusCode < 500 )
        {
            DebugLog( "[%s] Client error %i, not retrying.", __FUNCTION__, response.m_StatusCode );
            return response;
        }

        if( attempt < m_MaxRetries )
        {
            const int delay = m_RetryDelaySeconds * ( 1 << attempt ); // exponential backoff
            WarningLog( "[%s] Request failed (status %i), retrying in %is (attempt %i/%i)...", __FUNCTION__, response.m_StatusCode, delay, attempt + 1, m_MaxRetries );
            std::this_thread::sleep_for( std::chrono::seconds( delay ) );
        }
    }

    return response;
}
