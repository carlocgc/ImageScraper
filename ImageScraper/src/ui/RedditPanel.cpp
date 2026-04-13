#include "ui/RedditPanel.h"

#include <algorithm>
#include <cctype>

void ImageScraper::RedditPanel::Update( )
{
    if( ImGui::BeginChild( "SubredditName", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_SubredditName.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Subreddit (e.g. Gifs)", buffer, INPUT_STRING_MAX, flags ) )
        {
            m_SubredditName = buffer;
        }
    }

    ImGui::EndChild( );

    if( ImGui::BeginChild( "SubredditScope", ImVec2( 500, 25 ), false ) )
    {
        int redditScope = static_cast<int>( m_RedditScope );
        ImGui::Combo( "Scope", &redditScope, s_RedditScopeStrings, IM_ARRAYSIZE( s_RedditScopeStrings ) );
        m_RedditScope = static_cast<RedditScope>( redditScope );
    }

    ImGui::EndChild( );

    const RedditScope scope = static_cast<RedditScope>( m_RedditScope );

    if( scope == RedditScope::Top ||
        scope == RedditScope::Controversial ||
        scope == RedditScope::Sort )
    {
        if( ImGui::BeginChild( "SubredditScopeTimeFrame", ImVec2( 500, 25 ), false ) )
        {
            int redditScopeTimeFrame = static_cast<int>( m_RedditScopeTimeFrame );
            ImGui::Combo( "Time Frame", &redditScopeTimeFrame, s_RedditScopeTimeFrameStrings, IM_ARRAYSIZE( s_RedditScopeTimeFrameStrings ) );
            m_RedditScopeTimeFrame = static_cast<RedditScopeTimeFrame>( redditScopeTimeFrame );
        }

        ImGui::EndChild( );
    }

    if( ImGui::BeginChild( "RedditMaxMediaItems", ImVec2( 500, 25 ), false ) )
    {
        ImGui::InputInt( "Max Downloads", &m_RedditMaxMediaItems );
        m_RedditMaxMediaItems = std::clamp( m_RedditMaxMediaItems, REDDIT_LIMIT_MIN, REDDIT_LIMIT_MAX );
    }

    ImGui::EndChild( );
}

ImageScraper::UserInputOptions ImageScraper::RedditPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider      = ContentProvider::Reddit;
    options.m_SubredditName = m_SubredditName;

    auto toLower = [ ]( unsigned char ch ) { return static_cast<char>( std::tolower( ch ) ); };

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

    options.m_RedditMaxMediaItems = m_RedditMaxMediaItems;
    return options;
}
