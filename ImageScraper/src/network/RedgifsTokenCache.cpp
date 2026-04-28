#include "network/RedgifsTokenCache.h"

#include "log/Logger.h"
#include "requests/redgifs/GetTemporaryAuthRequest.h"
#include "utils/RedgifsUtils.h"

#include <algorithm>

namespace
{
    // Treat tokens as expired a little before they actually do, to avoid
    // racing the redgifs server clock.
    constexpr int k_TokenExpirySafetyBufferSeconds = 30;
}

ImageScraper::RedgifsTokenCache::RedgifsTokenCache( std::shared_ptr<IHttpClient> client,
                                                    std::string caBundle,
                                                    std::string userAgent )
    : m_HttpClient{ std::move( client ) }
    , m_CaBundle{ std::move( caBundle ) }
    , m_UserAgent{ std::move( userAgent ) }
{
}

bool ImageScraper::RedgifsTokenCache::EnsureToken( )
{
    const auto now = std::chrono::steady_clock::now( );
    if( !m_Token.empty( ) && now < m_TokenExpiresAt )
    {
        return true;
    }

    if( !m_HttpClient )
    {
        LogError( "[%s] RedgifsTokenCache has no HTTP client!", __FUNCTION__ );
        return false;
    }

    RequestOptions options{ };
    options.m_CaBundle = m_CaBundle;
    options.m_UserAgent = m_UserAgent;

    Redgifs::GetTemporaryAuthRequest request{ m_HttpClient };
    const RequestResult result = request.Perform( options );
    if( !result.m_Success )
    {
        LogError( "[%s] redgifs auth failed: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return false;
    }

    std::string token{ };
    int ttlSeconds = 0;
    if( !RedgifsUtils::ExtractTokenFromAuthResponse( result.m_Response, token, ttlSeconds ) )
    {
        return false;
    }

    m_Token = std::move( token );
    const int safeExpiry = ( std::max )( ttlSeconds - k_TokenExpirySafetyBufferSeconds, 60 );
    m_TokenExpiresAt = now + std::chrono::seconds{ safeExpiry };

    return true;
}
