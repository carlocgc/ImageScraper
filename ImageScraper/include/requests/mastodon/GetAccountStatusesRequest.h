#pragma once

#include "requests/Request.h"
#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper::Mastodon
{
    class GetAccountStatusesRequest final : public Request
    {
    public:
        GetAccountStatusesRequest( );
        GetAccountStatusesRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_PathPrefix;
        static const std::string s_PathSuffix;
    };
}
