#pragma once

#include <deque>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace ImageScraper
{
    // One sliding-window admission rule: at most m_MaxRequests within any trailing m_WindowSeconds.
    struct RateLimitWindow
    {
        int m_MaxRequests{ 0 };
        int m_WindowSeconds{ 0 };
    };

    // An endpoint may carry multiple stacked windows (e.g. 1000/hour AND 5000/day for Tumblr).
    // Admission requires every window to have room.
    using RateLimitSpec = std::vector<RateLimitWindow>;

    // Per-service map: endpoint key -> spec. Empty key falls back to s_DefaultRateLimitKey.
    using RateLimitTable = std::unordered_map<std::string, RateLimitSpec>;

    inline constexpr const char* s_DefaultRateLimitKey = "__default__";
}
