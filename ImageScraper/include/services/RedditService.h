#pragma once


#include <string>
#include "services/Service.h"
#include "ui/FrontEnd.h"
#include "config/Config.h"

namespace ImageScraper
{
    class RedditService : public Service
    {
    public:
        ~RedditService( ) = default;

        bool HandleUrl( const Config& config, FrontEnd& frontEnd, const std::string& url ) override;

    private:
        void DownloadHotReddit( const Config& config, FrontEnd& frontEnd, const std::string& subreddit );
    };
}
