#pragma once
#include "requests/Request.h"

namespace ImageScraper::Tumblr
{
    class RetrievePublishedPostsRequest : public Request
    {
    public:
        RequestResult Perform( const RequestOptions& options ) override;
    private:
        static const std::string s_BaseUrl;
    };
}
