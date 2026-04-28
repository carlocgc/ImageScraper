#pragma once

#include "requests/Request.h"
#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper::Redgifs
{
    // GETs https://api.redgifs.com/v2/users/<username>/search with the
    // username in RequestOptions::m_UrlExt, a Bearer token in m_AccessToken,
    // and pagination via m_QueryParams (count, page).
    class SearchUserGifsRequest final : public Request
    {
    public:
        SearchUserGifsRequest( );
        SearchUserGifsRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_BaseUrlPrefix;
        static const std::string s_BaseUrlSuffix;
    };
}
