#pragma once
#include "config/Config.h"
#include <string>


class RedditService
{
public:
    void DownloadHotReddit( const Config& config, const std::string& subreddit );

};

