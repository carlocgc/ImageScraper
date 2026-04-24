#include "ui/RedditPanel.h"
#include "ui/DownloadOptionControls.h"
#include "log/Logger.h"
#include "utils/RedditUtils.h"

#include <algorithm>
#include <cctype>

namespace
{
    constexpr const char* s_ConfigKey_RedditSubredditHistory = "reddit_subreddit_history";
    constexpr const char* s_ConfigKey_RedditUserHistory      = "reddit_user_history";
    constexpr const char* s_ConfigKey_RedditTargetType       = "reddit_target_type";
    constexpr const char* s_ConfigKey_RedditMaxDownloads     = "reddit_max_downloads";
}

void ImageScraper::RedditPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SubredditHistory.Load( appConfig, s_ConfigKey_RedditSubredditHistory );
    m_UserHistory.Load( appConfig, s_ConfigKey_RedditUserHistory );
    m_SubredditName = m_SubredditHistory.GetMostRecent( );
    m_UserName = m_UserHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int savedTargetType = static_cast<int>( RedditTargetType::Subreddit );
        if( m_AppConfig->GetValue<int>( s_ConfigKey_RedditTargetType, savedTargetType )
            && savedTargetType >= 0
            && savedTargetType < REDDIT_TARGET_TYPES_COUNT )
        {
            m_RedditTargetType = static_cast<RedditTargetType>( savedTargetType );
        }

        int saved = REDDIT_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( s_ConfigKey_RedditMaxDownloads, saved ) )
        {
            m_RedditMaxMediaItems = std::clamp( saved, REDDIT_LIMIT_MIN, REDDIT_LIMIT_MAX );
        }
    }
}

void ImageScraper::RedditPanel::OnSearchCommitted( )
{
    if( m_RedditTargetType == RedditTargetType::User )
    {
        m_UserName = RedditUtils::NormalizeUserName( m_UserName );
        m_UserHistory.Push( m_UserName );
        return;
    }

    m_SubredditName = RedditUtils::NormalizeSubredditName( m_SubredditName );
    m_SubredditHistory.Push( m_SubredditName );
}

bool ImageScraper::RedditPanel::IsReadyToRun( ) const
{
    if( m_RedditTargetType == RedditTargetType::User )
    {
        return !RedditUtils::NormalizeUserName( m_UserName ).empty( );
    }

    return !RedditUtils::NormalizeSubredditName( m_SubredditName ).empty( );
}

void ImageScraper::RedditPanel::Update( )
{
    const RedditTargetType previousTargetType = m_RedditTargetType;
    int redditTargetType = static_cast<int>( m_RedditTargetType );
    if( DownloadOptionControls::DrawCombo(
        {
            "RedditTargetType",
            "##reddit_target_type",
            "Target",
            "Subreddit: search a community such as Gifs.\nUser: search a profile such as spez."
        },
        redditTargetType,
        s_RedditTargetTypeStrings,
        IM_ARRAYSIZE( s_RedditTargetTypeStrings ) ) )
    {
        m_RedditTargetType = static_cast<RedditTargetType>( redditTargetType );

        if( m_RedditTargetType != previousTargetType && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( s_ConfigKey_RedditTargetType, redditTargetType );
            m_AppConfig->Serialise( );
        }
    }

    const bool isUserMode = m_RedditTargetType == RedditTargetType::User;
    std::string& targetName = isUserMode ? m_UserName : m_SubredditName;
    const SearchHistory& targetHistory = isUserMode ? m_UserHistory : m_SubredditHistory;

    DownloadOptionControls::DrawSearchInput(
        {
            isUserMode ? "RedditUserName" : "SubredditName",
            isUserMode ? "##reddit_user" : "##subreddit",
            isUserMode ? "##reddit_user_hist_btn" : "##subreddit_hist_btn",
            isUserMode ? "##reddit_user_hist" : "##subreddit_hist",
            isUserMode ? "User" : "Subreddit",
            isUserMode
                ? "Examples:\nspez - Reddit user\nu/spez - normalized to spez"
                : "Examples:\nGifs - subreddit name\nr/Gifs - normalized to Gifs",
            ImGuiInputTextFlags_CharsNoBlank
        },
        targetName,
        targetHistory );

    if( !isUserMode )
    {
        int redditScope = static_cast<int>( m_RedditScope );
        if( DownloadOptionControls::DrawCombo(
            {
                "SubredditScope",
                "##subreddit_scope",
                "Scope",
                "Examples:\nHot - active posts\nTop - ranked posts\nNew - newest posts"
            },
            redditScope,
            s_RedditScopeStrings,
            IM_ARRAYSIZE( s_RedditScopeStrings ) ) )
        {
            m_RedditScope = static_cast<RedditScope>( redditScope );
        }
    }

    const RedditScope scope = m_RedditScope;

    if( !isUserMode &&
        ( scope == RedditScope::Top ||
          scope == RedditScope::Controversial ||
          scope == RedditScope::Sort ) )
    {
        int redditScopeTimeFrame = static_cast<int>( m_RedditScopeTimeFrame );
        if( DownloadOptionControls::DrawCombo(
            {
                "SubredditScopeTimeFrame",
                "##subreddit_scope_time_frame",
                "Time",
                "Examples:\nDay - today\nWeek - this week\nAll - all time"
            },
            redditScopeTimeFrame,
            s_RedditScopeTimeFrameStrings,
            IM_ARRAYSIZE( s_RedditScopeTimeFrameStrings ) ) )
        {
            m_RedditScopeTimeFrame = static_cast<RedditScopeTimeFrame>( redditScopeTimeFrame );
        }
    }

    const int prev = m_RedditMaxMediaItems;
    if( DownloadOptionControls::DrawClampedInputInt(
        {
            "RedditMaxMediaItems",
            "##reddit_max_media_items",
            "Limit",
            DownloadOptionControls::s_MaxMediaDownloadsTooltip
        },
        m_RedditMaxMediaItems,
        REDDIT_LIMIT_MIN,
        REDDIT_LIMIT_MAX ) )
    {
        if( m_RedditMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( s_ConfigKey_RedditMaxDownloads, m_RedditMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }
}

ImageScraper::UserInputOptions ImageScraper::RedditPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::Reddit;
    options.m_RedditTargetType = m_RedditTargetType;
    options.m_RedditTargetName = m_RedditTargetType == RedditTargetType::User
        ? RedditUtils::NormalizeUserName( m_UserName )
        : RedditUtils::NormalizeSubredditName( m_SubredditName );

    auto toLower = [ ]( unsigned char ch ) { return static_cast<char>( std::tolower( ch ) ); };

    if( m_RedditTargetType == RedditTargetType::Subreddit )
    {
        std::string scope = s_RedditScopeStrings[ static_cast<int>( m_RedditScope ) ];
        std::transform( scope.begin( ), scope.end( ), scope.begin( ), toLower );
        options.m_RedditScope = scope;

        if( m_RedditScope == RedditScope::Top ||
            m_RedditScope == RedditScope::Controversial ||
            m_RedditScope == RedditScope::Sort )
        {
            std::string scopeTimeFrame = s_RedditScopeTimeFrameStrings[ static_cast<int>( m_RedditScopeTimeFrame ) ];
            std::transform( scopeTimeFrame.begin( ), scopeTimeFrame.end( ), scopeTimeFrame.begin( ), toLower );
            options.m_RedditScopeTimeFrame = scopeTimeFrame;
        }
    }

    options.m_RedditMaxMediaItems = m_RedditMaxMediaItems;
    return options;
}
