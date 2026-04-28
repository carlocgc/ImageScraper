#pragma once

#include "requests/Request.h"
#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper::Redgifs
{
    // GETs https://api.redgifs.com/v2/gifs/<slug>. Caller must put the slug in
    // RequestOptions::m_UrlExt and a Bearer token in m_AccessToken.
    class GetGifRequest final : public Request
    {
    public:
        GetGifRequest( );
        GetGifRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_BaseUrl;
    };
}
