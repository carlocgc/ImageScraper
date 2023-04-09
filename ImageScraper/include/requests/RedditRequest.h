#pragma once
#include "Request.h"
#include "config/Config.h"

namespace ImageScraper
{
    class RedditRequest final : public Request
    {
    public:
        RequestResult Perform( const RequestOptions& options ) override;
    };
}
