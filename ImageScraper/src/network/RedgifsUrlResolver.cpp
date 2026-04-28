#include "network/RedgifsUrlResolver.h"

#include "log/Logger.h"
#include "requests/RequestTypes.h"
#include "requests/redgifs/GetGifRequest.h"
#include "utils/RedgifsUtils.h"

ImageScraper::RedgifsUrlResolver::RedgifsUrlResolver( std::shared_ptr<IHttpClient> client,
                                                      std::string caBundle,
                                                      std::string userAgent )
    : m_HttpClient{ std::move( client ) }
    , m_CaBundle{ std::move( caBundle ) }
    , m_UserAgent{ std::move( userAgent ) }
    , m_TokenCache{ m_HttpClient, m_CaBundle, m_UserAgent }
{
}

bool ImageScraper::RedgifsUrlResolver::CanHandle( const std::string& url ) const
{
    return RedgifsUtils::IsRedgifsUrl( url );
}

std::optional<std::string> ImageScraper::RedgifsUrlResolver::Resolve( const std::string& url )
{
    std::lock_guard<std::mutex> lock{ m_Mutex };

    const std::string slug = RedgifsUtils::ExtractSlug( url );
    if( slug.empty( ) )
    {
        LogError( "[%s] Could not parse slug from redgifs URL: %s", __FUNCTION__, url.c_str( ) );
        return std::nullopt;
    }

    if( !m_TokenCache.EnsureToken( ) )
    {
        LogError( "[%s] Could not acquire redgifs token for: %s", __FUNCTION__, url.c_str( ) );
        return std::nullopt;
    }

    RequestOptions options{ };
    options.m_CaBundle = m_CaBundle;
    options.m_UserAgent = m_UserAgent;
    options.m_AccessToken = m_TokenCache.Token( );
    options.m_UrlExt = slug;

    Redgifs::GetGifRequest request{ m_HttpClient };
    const RequestResult result = request.Perform( options );
    if( !result.m_Success )
    {
        LogError( "[%s] redgifs lookup failed for slug '%s': %s", __FUNCTION__, slug.c_str( ), result.m_Error.m_ErrorString.c_str( ) );
        return std::nullopt;
    }

    const std::string resolved = RedgifsUtils::ExtractMediaUrlFromGifResponse( result.m_Response );
    if( resolved.empty( ) )
    {
        LogError( "[%s] redgifs response had no usable URL for slug '%s'", __FUNCTION__, slug.c_str( ) );
        return std::nullopt;
    }

    return resolved;
}
