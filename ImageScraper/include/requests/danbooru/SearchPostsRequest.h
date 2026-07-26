#pragma once

#include "network/IHttpClient.h"
#include "requests/Request.h"

#include <memory>
#include <string>

namespace ImageScraper::Danbooru
{
    class SearchPostsRequest : public Request
    {
    public:
        SearchPostsRequest( );
        SearchPostsRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_BaseUrl;
    };
}
