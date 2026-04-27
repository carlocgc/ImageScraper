#include "network/RedgifsResolver.h"
#include "log/Logger.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>

namespace
{
    constexpr const char* k_AuthUrl       = "https://api.redgifs.com/v2/auth/temporary";
    constexpr const char* k_GifUrlPrefix  = "https://api.redgifs.com/v2/gifs/";

    // Treat tokens as expired a little before they actually do, to avoid
    // racing the redgifs server clock.
    constexpr int k_TokenExpirySafetyBufferSeconds = 30;

    std::string ToLowerCopy( std::string s )
    {
        std::transform( s.begin( ), s.end( ), s.begin( ),
                        []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
        return s;
    }

    bool ContainsHost( const std::string& url, const char* host )
    {
        return ToLowerCopy( url ).find( host ) != std::string::npos;
    }
}

ImageScraper::RedgifsResolver::RedgifsResolver( std::shared_ptr<IHttpClient> client,
                                                std::string caBundle,
                                                std::string userAgent )
    : m_HttpClient{ std::move( client ) }
    , m_CaBundle{ std::move( caBundle ) }
    , m_UserAgent{ std::move( userAgent ) }
{
}

bool ImageScraper::RedgifsResolver::IsRedgifsUrl( const std::string& url )
{
    return ContainsHost( url, "redgifs.com" );
}

std::string ImageScraper::RedgifsResolver::ExtractSlug( const std::string& url )
{
    // Accept both /watch/<slug> and /ifr/<slug>. Strip query/fragment first.
    std::string trimmed = url;
    const std::size_t queryPos = trimmed.find_first_of( "?#" );
    if( queryPos != std::string::npos )
    {
        trimmed.erase( queryPos );
    }

    const std::string lowered = ToLowerCopy( trimmed );

    static const char* k_Markers[] = { "/watch/", "/ifr/" };
    std::size_t slugStart = std::string::npos;
    for( const char* marker : k_Markers )
    {
        const std::size_t pos = lowered.find( marker );
        if( pos != std::string::npos )
        {
            slugStart = pos + std::string{ marker }.size( );
            break;
        }
    }

    if( slugStart == std::string::npos || slugStart >= trimmed.size( ) )
    {
        return { };
    }

    std::string slug = trimmed.substr( slugStart );

    // Strip any trailing path segments.
    const std::size_t slashPos = slug.find( '/' );
    if( slashPos != std::string::npos )
    {
        slug.erase( slashPos );
    }

    return ToLowerCopy( slug );
}

bool ImageScraper::RedgifsResolver::EnsureToken( )
{
    const auto now = std::chrono::steady_clock::now( );
    if( !m_Token.empty( ) && now < m_TokenExpiresAt )
    {
        return true;
    }

    if( !m_HttpClient )
    {
        LogError( "[%s] RedgifsResolver has no HTTP client!", __FUNCTION__ );
        return false;
    }

    HttpRequest req{ };
    req.m_Url       = k_AuthUrl;
    req.m_CaBundle  = m_CaBundle;
    req.m_UserAgent = m_UserAgent;

    const HttpResponse response = m_HttpClient->Get( req );
    if( !response.m_Success )
    {
        LogError( "[%s] redgifs auth failed: HTTP %d %s", __FUNCTION__,
                  response.m_StatusCode, response.m_Error.c_str( ) );
        return false;
    }

    try
    {
        const auto parsed = nlohmann::json::parse( response.m_Body );
        if( !parsed.contains( "token" ) || !parsed[ "token" ].is_string( ) )
        {
            LogError( "[%s] redgifs auth response missing token field", __FUNCTION__ );
            return false;
        }

        m_Token = parsed[ "token" ].get<std::string>( );

        int expiresIn = 0;
        if( parsed.contains( "expiresIn" ) && parsed[ "expiresIn" ].is_number_integer( ) )
        {
            expiresIn = parsed[ "expiresIn" ].get<int>( );
        }
        else if( parsed.contains( "addr" ) ) // Newer payloads sometimes nest expiry; default conservatively.
        {
            expiresIn = 3600;
        }
        else
        {
            expiresIn = 3600;
        }

        const int safeExpiry = std::max( expiresIn - k_TokenExpirySafetyBufferSeconds, 60 );
        m_TokenExpiresAt = now + std::chrono::seconds{ safeExpiry };
    }
    catch( const nlohmann::json::exception& e )
    {
        LogError( "[%s] redgifs auth parse error: %s", __FUNCTION__, e.what( ) );
        return false;
    }

    return true;
}

std::optional<std::string> ImageScraper::RedgifsResolver::Resolve( const std::string& watchUrl )
{
    const std::string slug = ExtractSlug( watchUrl );
    if( slug.empty( ) )
    {
        LogDebug( "[%s] redgifs URL has no parseable slug: %s", __FUNCTION__, watchUrl.c_str( ) );
        return std::nullopt;
    }

    if( !EnsureToken( ) )
    {
        return std::nullopt;
    }

    HttpRequest req{ };
    req.m_Url       = std::string{ k_GifUrlPrefix } + slug;
    req.m_CaBundle  = m_CaBundle;
    req.m_UserAgent = m_UserAgent;
    req.m_Headers.push_back( "Authorization: Bearer " + m_Token );

    const HttpResponse response = m_HttpClient->Get( req );
    if( !response.m_Success )
    {
        LogError( "[%s] redgifs lookup failed for slug '%s': HTTP %d %s",
                  __FUNCTION__, slug.c_str( ), response.m_StatusCode, response.m_Error.c_str( ) );
        return std::nullopt;
    }

    try
    {
        const auto parsed = nlohmann::json::parse( response.m_Body );
        if( !parsed.contains( "gif" ) )
        {
            LogError( "[%s] redgifs response missing 'gif' for slug '%s'", __FUNCTION__, slug.c_str( ) );
            return std::nullopt;
        }

        const auto& gif = parsed[ "gif" ];
        if( !gif.contains( "urls" ) )
        {
            return std::nullopt;
        }

        const auto& urls = gif[ "urls" ];

        // Prefer HD; fall back to SD. Redgifs occasionally returns only one.
        for( const char* key : { "hd", "sd" } )
        {
            if( urls.contains( key ) && urls[ key ].is_string( ) )
            {
                std::string mp4 = urls[ key ].get<std::string>( );
                if( !mp4.empty( ) )
                {
                    return mp4;
                }
            }
        }
    }
    catch( const nlohmann::json::exception& e )
    {
        LogError( "[%s] redgifs lookup parse error for slug '%s': %s",
                  __FUNCTION__, slug.c_str( ), e.what( ) );
        return std::nullopt;
    }

    return std::nullopt;
}
