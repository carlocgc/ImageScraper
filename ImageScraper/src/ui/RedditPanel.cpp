#include "ui/RedditPanel.h"
#include "log/Logger.h"

#include <algorithm>
#include <cctype>

void ImageScraper::RedditPanel::LoadSearchHistory( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = std::move( appConfig );
    if( !m_AppConfig ) return;

    Json arr;
    if( m_AppConfig->GetValue<Json>( "reddit_subreddit_history", arr ) && arr.is_array( ) )
    {
        for( const auto& item : arr )
        {
            if( item.is_string( ) )
                m_SearchHistory.push_back( item.get<std::string>( ) );
        }
        if( static_cast<int>( m_SearchHistory.size( ) ) > k_MaxHistory )
            m_SearchHistory.resize( k_MaxHistory );
    }

    if( !m_SearchHistory.empty( ) )
        m_SubredditName = m_SearchHistory.front( );
}

void ImageScraper::RedditPanel::SaveSearchHistory( )
{
    if( !m_AppConfig ) return;

    Json arr = Json::array( );
    for( const auto& item : m_SearchHistory )
        arr.push_back( item );

    m_AppConfig->SetValue<Json>( "reddit_subreddit_history", arr );
    if( !m_AppConfig->Serialise( ) )
    {
        WarningLog( "[%s] Failed to save Reddit search history", __FUNCTION__ );
    }
}

void ImageScraper::RedditPanel::OnSearchCommitted( )
{
    PushToHistory( m_SubredditName );
}

void ImageScraper::RedditPanel::PushToHistory( const std::string& value )
{
    if( value.empty( ) ) return;

    auto it = std::find( m_SearchHistory.begin( ), m_SearchHistory.end( ), value );
    if( it != m_SearchHistory.end( ) )
        m_SearchHistory.erase( it );

    m_SearchHistory.insert( m_SearchHistory.begin( ), value );

    if( static_cast<int>( m_SearchHistory.size( ) ) > k_MaxHistory )
        m_SearchHistory.resize( k_MaxHistory );

    SaveSearchHistory( );
}

void ImageScraper::RedditPanel::Update( )
{
    if( ImGui::BeginChild( "SubredditName", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_SubredditName.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        const float arrowW  = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        ImGui::SetNextItemWidth( ImGui::CalcItemWidth( ) - arrowW - spacing );
        if( ImGui::InputText( "##subreddit", buffer, INPUT_STRING_MAX, flags ) )
            m_SubredditName = buffer;

        ImGui::SameLine( 0.f, spacing );
        if( ImGui::ArrowButton( "##subreddit_hist_btn", ImGuiDir_Down ) )
            ImGui::OpenPopup( "##subreddit_hist" );

        ImGui::SameLine( 0.f, spacing );
        ImGui::TextUnformatted( "Subreddit (e.g. Gifs)" );

        if( ImGui::BeginPopup( "##subreddit_hist" ) )
        {
            if( m_SearchHistory.empty( ) )
            {
                ImGui::TextDisabled( "No history yet" );
            }
            else
            {
                for( const auto& item : m_SearchHistory )
                {
                    if( ImGui::Selectable( item.c_str( ) ) )
                        m_SubredditName = item;
                }
            }
            ImGui::EndPopup( );
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
