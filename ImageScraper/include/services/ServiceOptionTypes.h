#pragma once
#include <string>

#define REDDIT_LIMIT_MIN 25
#define REDDIT_LIMIT_MAX 100

namespace ImageScraper
{
    enum class ContentProvider
    {
        Reddit = 0,
        Twitter = 1
    };

    static const char* s_ContentProviderStrings[ 2 ]
    {
        "Reddit",
        "Twitter",
    };

    enum class RedditScope
    {
        Best = 0,
        Controversial = 1,
        Hot = 2,
        New = 3,
        Random = 4,
        Rising = 5,
        Top = 6
    };

    static const char* s_RedditScopeStrings[ 7 ]
    {
        "Best",
        "Controversial",
        "Hot",
        "New",
        "Random",
        "Rising",
        "Top"
    };

    struct UserInputOptions
    {
        ContentProvider m_Provider;

        // Reddit
        std::string m_SubredditName;
        std::string m_RedditScope;
        std::string m_RedditLimit;

        // Twitter
        std::string m_TwitterHandle;
    };
}