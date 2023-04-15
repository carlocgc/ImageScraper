#pragma once
#include "requests/Request.h"
#include "config/Config.h"

namespace ImageScraper::Reddit
{
    class FetchSubredditPostsRequest final : public Request
    {
    public:
        RequestResult Perform( const RequestOptions& options ) override;
    };
}
