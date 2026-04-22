#pragma once

#include "requests/Request.h"
#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper::Bluesky
{
    class GetAuthorFeedRequest final : public Request
    {
    public:
        GetAuthorFeedRequest( );
        GetAuthorFeedRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_BaseUrl;
    };
}
