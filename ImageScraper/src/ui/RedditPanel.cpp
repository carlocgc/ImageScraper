#include "ui/RedditPanel.h"
#include "log/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

static bool HasAnyFile( const std::filesystem::path& dir )
{
    if( !std::filesystem::exists( dir ) )
    {
        return false;
    }
    std::error_code ec;
    for( const auto& entry : std::filesystem::recursive_directory_iterator( dir, ec ) )
    {
        if( entry.is_regular_file( ) )
        {
            return true;
        }
    }
    return false;
}

void ImageScraper::RedditPanel::LoadSearchHistory( std::shared_ptr<JsonFile> appConfig )
{
    m_SearchHistory.Load( std::move( appConfig ), "reddit_subreddit_history" );
    m_SubredditName = m_SearchHistory.GetMostRecent( );
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
        ImGui::InputInt( "Max Downloads", &m_RedditMaxMediaItems );
        m_RedditMaxMediaItems = std::clamp( m_RedditMaxMediaItems, REDDIT_LIMIT_MIN, REDDIT_LIMIT_MAX );
    }

    ImGui::EndChild( );

    const std::filesystem::path redditDir =
        std::filesystem::path( m_OutputDir ) / "Downloads" / "Reddit";
    const bool hasContent = HasAnyFile( redditDir );

    ImGui::BeginDisabled( !hasContent || m_SigningIn );

    if( ImGui::Button( "Delete All##reddit", ImVec2( 100, 40 ) ) )
    {
        ImGui::OpenPopup( "Confirm Delete All##reddit" );
    }

    ImGui::EndDisabled( );

    ImGui::SetNextWindowSize( ImVec2( 380, 0 ), ImGuiCond_Always );
    if( ImGui::BeginPopupModal( "Confirm Delete All##reddit", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::Spacing( );
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.6f, 0.1f, 1.0f ) );
        ImGui::TextWrapped( "This will permanently delete all downloaded Reddit content from disk." );
        ImGui::PopStyleColor( );
        ImGui::Spacing( );

        ImGui::PushStyleColor( ImGuiCol_Button,        ImVec4( 0.7f, 0.3f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.9f, 0.4f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,  ImVec4( 0.5f, 0.2f, 0.0f, 1.0f ) );

        if( ImGui::Button( "Delete All", ImVec2( 110, 0 ) ) )
        {
            std::error_code ec;
            std::filesystem::remove_all( redditDir, ec );
            if( ec )
            {
                LogError( "[%s] Failed to delete Reddit downloads: %s",
                          __FUNCTION__, ec.message( ).c_str( ) );
            }

            if( m_OnDeleteAll )
            {
                m_OnDeleteAll( redditDir.string( ) );
            }

            ImGui::CloseCurrentPopup( );
        }

        ImGui::PopStyleColor( 3 );

        ImGui::SameLine( );
        if( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) )
        {
            ImGui::CloseCurrentPopup( );
        }

        ImGui::EndPopup( );
    }
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
