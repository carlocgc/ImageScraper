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

ImageScraper::HttpResponse ImageScraper::RetryHttpClient::Get( const HttpRequest& request, const std::string& rateLimitKey )
{
    return Execute( request, rateLimitKey, false );
}

ImageScraper::HttpResponse ImageScraper::RetryHttpClient::Post( const HttpRequest& request, const std::string& rateLimitKey )
{
    return Execute( request, rateLimitKey, true );
}

ImageScraper::HttpResponse ImageScraper::RetryHttpClient::Execute( const HttpRequest& request, const std::string& rateLimitKey, bool isPost )
{
    HttpResponse response{ };
    const char* method = isPost ? "POST" : "GET";
    const char* effectiveKey = rateLimitKey.empty( ) ? "<default>" : rateLimitKey.c_str( );

    for( int attempt = 0; attempt <= m_MaxRetries; ++attempt )
    {
        response = isPost ? m_Inner->Post( request, rateLimitKey ) : m_Inner->Get( request, rateLimitKey );

        if( response.m_Success )
        {
            return response;
        }

        if( response.m_StatusCode == s_RateLimitCode )
        {
            WarningLog( "[%s] %s %s hit 429 for key '%s'. Waiting %is before retry (attempt %i/%i).", __FUNCTION__, method, request.m_Url.c_str( ), effectiveKey, s_RateLimitDelaySeconds, attempt + 1, m_MaxRetries );
            std::this_thread::sleep_for( std::chrono::seconds( s_RateLimitDelaySeconds ) );
            continue;
        }

        // Don't retry on client errors (4xx) other than 429
        if( response.m_StatusCode >= 400 && response.m_StatusCode < 500 )
        {
            LogDebug( "[%s] %s %s failed with client error %i for key '%s', not retrying at HTTP layer.", __FUNCTION__, method, request.m_Url.c_str( ), response.m_StatusCode, effectiveKey );
            return response;
        }

        if( attempt < m_MaxRetries )
        {
            const int delay = m_RetryDelaySeconds * ( 1 << attempt ); // exponential backoff
            WarningLog( "[%s] %s %s failed with status %i for key '%s'. Retrying in %is (attempt %i/%i).", __FUNCTION__, method, request.m_Url.c_str( ), response.m_StatusCode, effectiveKey, delay, attempt + 1, m_MaxRetries );
            std::this_thread::sleep_for( std::chrono::seconds( delay ) );
        }
    }

    return response;
}
