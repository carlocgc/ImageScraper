#include "ui/TumblrPanel.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::TumblrPanel::LoadSearchHistory( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = std::move( appConfig );
    if( !m_AppConfig ) return;

    Json arr;
    if( m_AppConfig->GetValue<Json>( "tumblr_user_history", arr ) && arr.is_array( ) )
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
        m_TumblrUser = m_SearchHistory.front( );
}

void ImageScraper::TumblrPanel::SaveSearchHistory( )
{
    if( !m_AppConfig ) return;

    Json arr = Json::array( );
    for( const auto& item : m_SearchHistory )
        arr.push_back( item );

    m_AppConfig->SetValue<Json>( "tumblr_user_history", arr );
    if( !m_AppConfig->Serialise( ) )
    {
        WarningLog( "[%s] Failed to save Tumblr search history", __FUNCTION__ );
    }
}

void ImageScraper::TumblrPanel::OnSearchCommitted( )
{
    PushToHistory( m_TumblrUser );
}

void ImageScraper::TumblrPanel::PushToHistory( const std::string& value )
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

void ImageScraper::TumblrPanel::Update( )
{
    if( ImGui::BeginChild( "TumblrUser", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_TumblrUser.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        const float arrowW  = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        ImGui::SetNextItemWidth( ImGui::CalcItemWidth( ) - arrowW - spacing );
        if( ImGui::InputText( "##tumblr_user", buffer, INPUT_STRING_MAX, flags ) )
            m_TumblrUser = buffer;

        ImGui::SameLine( 0.f, spacing );
        if( ImGui::ArrowButton( "##tumblr_hist_btn", ImGuiDir_Down ) )
            ImGui::OpenPopup( "##tumblr_hist" );

        ImGui::SameLine( 0.f, spacing );
        ImGui::TextUnformatted( "Tumblr User" );

        if( ImGui::BeginPopup( "##tumblr_hist" ) )
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
                        m_TumblrUser = item;
                }
            }
            ImGui::EndPopup( );
        }
    }

    ImGui::EndChild( );

    if( ImGui::BeginChild( "TumblrMaxMediaItems", ImVec2( 500, 25 ), false ) )
    {
        ImGui::InputInt( "Max Downloads", &m_TumblrMaxMediaItems );
        m_TumblrMaxMediaItems = std::clamp( m_TumblrMaxMediaItems, TUMBLR_LIMIT_MIN, TUMBLR_LIMIT_MAX );
    }

    ImGui::EndChild( );
}

ImageScraper::UserInputOptions ImageScraper::TumblrPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider            = ContentProvider::Tumblr;
    options.m_TumblrUser          = m_TumblrUser;
    options.m_TumblrMaxMediaItems = m_TumblrMaxMediaItems;
    return options;
}
