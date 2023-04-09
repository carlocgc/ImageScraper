#pragma once
#include "config/Config.h"

#include "Service.h"
#include "ui/FrontEnd.h"

#include <string>
#include <future>

namespace ImageScraper
{
    class RedditService final : public Service
    {
    public:
        ~RedditService( ) override { };
        void DownloadHotReddit( const Config& config, const std::string& subreddit );
        bool HandleUrl( const Config& config, const FrontEnd& frontEnd, const std::string& url ) override;
    };
}
