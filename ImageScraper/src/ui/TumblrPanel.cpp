#include "ui/TumblrPanel.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::TumblrPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "tumblr_user_history" );
    m_TumblrUser = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = TUMBLR_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( "tumblr_max_downloads", saved ) )
        {
            m_TumblrMaxMediaItems = std::clamp( saved, TUMBLR_LIMIT_MIN, TUMBLR_LIMIT_MAX );
        }
    }
}

void ImageScraper::TumblrPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_TumblrUser );
}

void ImageScraper::TumblrPanel::Update( )
{
    if( ImGui::BeginChild( "TumblrUser", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_TumblrUser.c_str( ) );
        const float arrowW  = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        ImGui::SetNextItemWidth( ImGui::CalcItemWidth( ) - arrowW - spacing );
        if( ImGui::InputText( "##tumblr_user", buffer, INPUT_STRING_MAX, ImGuiInputTextFlags_CharsNoBlank ) )
        {
            m_TumblrUser = buffer;
        }

        const ImVec2 inputMin = ImGui::GetItemRectMin( );
        const ImVec2 inputMax = ImGui::GetItemRectMax( );

        ImGui::SameLine( 0.f, spacing );
        if( ImGui::ArrowButton( "##tumblr_hist_btn", ImGuiDir_Down ) )
        {
            ImGui::OpenPopup( "##tumblr_hist" );
        }

        ImGui::SameLine( 0.f, spacing );
        ImGui::TextUnformatted( "Tumblr User" );

        const float popupW = ( inputMax.x - inputMin.x ) + spacing + arrowW;
        ImGui::SetNextWindowPos( ImVec2( inputMin.x, inputMax.y ), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2( popupW, 0.f ), ImGuiCond_Always );
        if( ImGui::BeginPopup( "##tumblr_hist", ImGuiWindowFlags_NoFocusOnAppearing ) )
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
                        m_TumblrUser = item;
                    }
                }
            }
            ImGui::EndPopup( );
        }
    }

    ImGui::EndChild( );

    if( ImGui::BeginChild( "TumblrMaxMediaItems", ImVec2( 500, 25 ), false ) )
    {
        const int prev = m_TumblrMaxMediaItems;
        ImGui::InputInt( "Max Downloads", &m_TumblrMaxMediaItems );
        m_TumblrMaxMediaItems = std::clamp( m_TumblrMaxMediaItems, TUMBLR_LIMIT_MIN, TUMBLR_LIMIT_MAX );
        if( m_TumblrMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "tumblr_max_downloads", m_TumblrMaxMediaItems );
            m_AppConfig->Serialise( );
        }
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
