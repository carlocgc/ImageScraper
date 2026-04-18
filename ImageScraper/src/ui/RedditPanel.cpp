#include "ui/RedditPanel.h"
#include "log/Logger.h"

#include <algorithm>
#include <cctype>

void ImageScraper::RedditPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "reddit_subreddit_history" );
    m_SubredditName = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = REDDIT_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( "reddit_max_downloads", saved ) )
        {
            m_RedditMaxMediaItems = std::clamp( saved, REDDIT_LIMIT_MIN, REDDIT_LIMIT_MAX );
        }
    }
}

void ImageScraper::RedditPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_SubredditName );
}

void ImageScraper::RedditPanel::Update( )
{
    if( ImGui::BeginChild( "SubredditName", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_SubredditName.c_str( ) );
        const float arrowW  = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        ImGui::SetNextItemWidth( ImGui::CalcItemWidth( ) - arrowW - spacing );
        if( ImGui::InputText( "##subreddit", buffer, INPUT_STRING_MAX, ImGuiInputTextFlags_CharsNoBlank ) )
        {
            m_SubredditName = buffer;
        }

        const ImVec2 inputMin = ImGui::GetItemRectMin( );
        const ImVec2 inputMax = ImGui::GetItemRectMax( );

        ImGui::SameLine( 0.f, spacing );
        if( ImGui::ArrowButton( "##subreddit_hist_btn", ImGuiDir_Down ) )
        {
            ImGui::OpenPopup( "##subreddit_hist" );
        }

        ImGui::SameLine( 0.f, spacing );
        ImGui::TextUnformatted( "Subreddit (e.g. Gifs)" );

        const float popupW = ( inputMax.x - inputMin.x ) + spacing + arrowW;
        ImGui::SetNextWindowPos( ImVec2( inputMin.x, inputMax.y ), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2( popupW, 0.f ), ImGuiCond_Always );
        if( ImGui::BeginPopup( "##subreddit_hist", ImGuiWindowFlags_NoFocusOnAppearing ) )
        {
            if( m_SearchHistory.IsEmpty( ) )
            {
                ImGui::TextDisabled( "No history yet" );
            }
            else
            {
                for( const auto& item : m_SearchHistory.GetItems( ) )
                {
                    if( ImGui::Selectable( item.c_str( ) ) )
                    {
                        m_SubredditName = item;
                    }
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
        const int prev = m_RedditMaxMediaItems;
        ImGui::InputInt( "Max Downloads", &m_RedditMaxMediaItems );
        m_RedditMaxMediaItems = std::clamp( m_RedditMaxMediaItems, REDDIT_LIMIT_MIN, REDDIT_LIMIT_MAX );
        if( m_RedditMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "reddit_max_downloads", m_RedditMaxMediaItems );
            m_AppConfig->Serialise( );
        }
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
