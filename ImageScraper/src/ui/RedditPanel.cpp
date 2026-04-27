#include "ui/RedditPanel.h"
#include "ui/DownloadOptionControls.h"
#include "log/Logger.h"
#include "utils/RedditUtils.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <cctype>

namespace
{
    constexpr const char* s_ConfigKey_RedditSubredditHistory = "reddit_subreddit_history";
    constexpr const char* s_ConfigKey_RedditUserHistory      = "reddit_user_history";
    constexpr const char* s_ConfigKey_RedditTargetType       = "reddit_target_type";
    constexpr const char* s_ConfigKey_RedditMaxDownloads     = "reddit_max_downloads";
    constexpr const char* s_ConfigKey_RedditScope            = "reddit_scope";
    constexpr const char* s_ConfigKey_RedditScopeTimeFrame   = "reddit_scope_time_frame";
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

        int scope = static_cast<int>( RedditScope::Hot );
        if( m_AppConfig->GetValue<int>( s_ConfigKey_RedditScope, scope ) )
        {
            m_RedditScope = static_cast<RedditScope>( scope );
        }

        int scopeTimeFrame = static_cast<int>( RedditScopeTimeFrame::All );
        if( m_AppConfig->GetValue<int>( s_ConfigKey_RedditScopeTimeFrame, scopeTimeFrame ) )
        {
            m_RedditScopeTimeFrame = static_cast<RedditScopeTimeFrame>( scopeTimeFrame );
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
        const RedditScope previousRedditScope = m_RedditScope;
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

            if( m_RedditScope != previousRedditScope && m_AppConfig )
            {
                m_AppConfig->SetValue<int>( s_ConfigKey_RedditScope, redditScope );
                m_AppConfig->Serialise( );
            }
        }
    }

    const RedditScope scope = m_RedditScope;

    if( !isUserMode &&
        ( scope == RedditScope::Top ||
          scope == RedditScope::Controversial ||
          scope == RedditScope::Sort ) )
    {
        const RedditScopeTimeFrame previousRedditScopeTimeFrame = m_RedditScopeTimeFrame;
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

            if( m_RedditScopeTimeFrame != previousRedditScopeTimeFrame && m_AppConfig )
            {
                m_AppConfig->SetValue<int>( s_ConfigKey_RedditScopeTimeFrame, redditScopeTimeFrame );
                m_AppConfig->Serialise( );
            }
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

    if( m_RedditTargetType == RedditTargetType::Subreddit )
    {
        options.m_RedditScope = StringUtils::ToLower( s_RedditScopeStrings[ static_cast<int>( m_RedditScope ) ] );

        if( m_RedditScope == RedditScope::Top ||
            m_RedditScope == RedditScope::Controversial ||
            m_RedditScope == RedditScope::Sort )
        {
            options.m_RedditScopeTimeFrame = StringUtils::ToLower( s_RedditScopeTimeFrameStrings[ static_cast<int>( m_RedditScopeTimeFrame ) ] );
        }
    }

    options.m_RedditMaxMediaItems = m_RedditMaxMediaItems;
    return options;
}
