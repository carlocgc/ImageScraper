#pragma once
#include <string>

namespace ImageScraper
{
    // ── Limits & capacities ───────────────────────────────────────────────────
    constexpr int CONTENT_PROVIDERS_COUNT       = 6;
    constexpr int INPUT_STRING_MAX              = 64;
    constexpr int REDDIT_LIMIT_MIN              = 1;
    constexpr int REDDIT_LIMIT_DEFAULT          = 5;
    constexpr int REDDIT_LIMIT_MAX              = 10000;
    constexpr int REDDIT_TARGET_TYPES_COUNT     = 2;
    constexpr int REDDIT_SCOPES_COUNT           = 8;
    constexpr int REDDIT_SCOPE_TIMEFRAMES_COUNT = 5;
    constexpr int TUMBLR_LIMIT_MIN              = 1;
    constexpr int TUMBLR_LIMIT_DEFAULT          = 5;
    constexpr int TUMBLR_LIMIT_MAX              = 10000;
    constexpr int FOURCHAN_THREAD_MIN           = 1;
    constexpr int FOURCHAN_THREAD_MAX           = 50;
    constexpr int FOURCHAN_MEDIA_MIN            = 1;
    constexpr int FOURCHAN_MEDIA_DEFAULT        = 5;
    constexpr int FOURCHAN_MEDIA_MAX            = 10000;
    constexpr int BLUESKY_LIMIT_MIN             = 1;
    constexpr int BLUESKY_LIMIT_DEFAULT         = 5;
    constexpr int BLUESKY_LIMIT_MAX             = 10000;
    constexpr int MASTODON_LIMIT_MIN            = 1;
    constexpr int MASTODON_LIMIT_DEFAULT        = 5;
    constexpr int MASTODON_LIMIT_MAX            = 10000;
    constexpr int REDGIFS_LIMIT_MIN             = 1;
    constexpr int REDGIFS_LIMIT_DEFAULT         = 5;
    constexpr int REDGIFS_LIMIT_MAX             = 10000;

    enum class ContentProvider : uint16_t
    {
        Reddit = 0,
        Tumblr = 1,
        FourChan = 2,
        Bluesky = 3,
        Mastodon = 4,
        Redgifs = 5,
        Count = CONTENT_PROVIDERS_COUNT
    };

    static const char* s_ContentProviderStrings[ CONTENT_PROVIDERS_COUNT ]
    {
        "Reddit",
        "Tumblr",
        "4chan",
        "Bluesky",
        "Mastodon",
        "Redgifs"
    };

    enum class RedditTargetType : uint16_t
    {
        Subreddit = 0,
        User = 1,
        Count = REDDIT_TARGET_TYPES_COUNT
    };

    static const char* s_RedditTargetTypeStrings[ REDDIT_TARGET_TYPES_COUNT ]
    {
        "Subreddit",
        "User"
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
        Sort = 7,
        Count = REDDIT_SCOPES_COUNT
    };

    static const char* s_RedditScopeStrings[ REDDIT_SCOPES_COUNT ]
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
        All = 4,
        Count = REDDIT_SCOPE_TIMEFRAMES_COUNT
    };

    static const char* s_RedditScopeTimeFrameStrings[ REDDIT_SCOPE_TIMEFRAMES_COUNT ]
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
        RedditTargetType m_RedditTargetType{ RedditTargetType::Subreddit };
        std::string m_RedditTargetName;
        std::string m_RedditScope;
        std::string m_RedditScopeTimeFrame;
        int m_RedditMaxMediaItems{ REDDIT_LIMIT_DEFAULT };

        // Tumblr
        std::string m_TumblrUser;
        int m_TumblrMaxMediaItems;

        // 4chan
        std::string m_FourChanBoard;
        int m_FourChanMaxThreads;
        int m_FourChanMaxMediaItems;

        // Bluesky
        std::string m_BlueskyActor;
        int m_BlueskyMaxMediaItems;

        // Mastodon
        std::string m_MastodonInstance;
        std::string m_MastodonAccount;
        int m_MastodonMaxMediaItems;

        // Redgifs
        std::string m_RedgifsUser;
        int m_RedgifsMaxMediaItems{ REDGIFS_LIMIT_DEFAULT };
    };
}
