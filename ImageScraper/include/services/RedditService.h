#pragma once
#include "config/Config.h"
#include <string>
#include <future>
#include "Service.h"

class RedditService final : public Service
{
public:
    ~RedditService( ) override { };
    void DownloadHotReddit( const Config& config, const std::string& subreddit );
    bool HandleUrl( const Config& config, const std::string& url ) override;
};

