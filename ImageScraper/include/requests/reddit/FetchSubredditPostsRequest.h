#pragma once
#include "requests/Request.h"
#include "config/Config.h"

namespace ImageScraper::Reddit
{
    class FetchSubredditPostsRequest final : public Request
    {
    public:
        RequestResult Perform( const RequestOptions& options ) override;
    private:
        static const std::string s_BaseUrl;
        static const std::string s_AuthBaseUrl;
    };
}
