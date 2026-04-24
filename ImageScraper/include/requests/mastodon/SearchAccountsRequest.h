#pragma once

#include "requests/Request.h"
#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper::Mastodon
{
    class SearchAccountsRequest final : public Request
    {
    public:
        SearchAccountsRequest( );
        SearchAccountsRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_Path;
    };
}
