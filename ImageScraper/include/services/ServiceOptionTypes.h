#pragma once
#include <string>

#define REDDIT_LIMIT_MIN 25
#define REDDIT_LIMIT_MAX 100
#define SUBREDDIT_NAME_MAX_LENGTH 64

#define INSTAGRAM_USER_MAX_LENGTH 64

namespace ImageScraper
{
    enum class ContentProvider : uint16_t
    {
        Reddit = 0,
        Instagram = 1
    };

    static const char* s_ContentProviderStrings[ 2 ]
    {
        "Reddit",
        "Instagram",
    };

    enum class RedditScope : uint16_t
    {
        Best = 0,
        Controversial = 1,
        Hot = 2,
        New = 3,
        Random = 4,
        Rising = 5,
        Top = 6,
        Sort = 7
    };

    static const char* s_RedditScopeStrings[ 8 ]
    {
        "Best",
        "Controversial",
        "Hot",
        "New",
        "Random",
        "Rising",
        "Top",
        "Sort"
    };

    enum class RedditScopeTimeFrame : uint16_t
    {
        Hour = 0,
        Day = 1,
        Week = 2,
        Month = 3,
        All = 4
    };

    static const char* s_RedditScopeTimeFrameStrings[ 5 ]
    {
        "Hour",
        "Day",
        "Week",
        "Month",
        "All"
    };

    struct UserInputOptions
    {
        ContentProvider m_Provider;

        // Reddit
        std::string m_SubredditName;
        std::string m_RedditScope;
        std::string m_RedditScopeTimeFrame;
        std::string m_RedditLimit;

        // Instagram
        std::string m_InstagramUser;
    };
}