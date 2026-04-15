#pragma once
#include "requests/Request.h"
#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper::Reddit
{
    class FetchSubredditPostsRequest final : public Request
    {
    public:
        FetchSubredditPostsRequest( );
        FetchSubredditPostsRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_BaseUrl;
        static const std::string s_AuthBaseUrl;
    };
}
