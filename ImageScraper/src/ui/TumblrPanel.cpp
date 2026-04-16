#include "ui/TumblrPanel.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::TumblrPanel::LoadSearchHistory( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = std::move( appConfig );
    if( !m_AppConfig ) return;

    m_AppConfig->GetValue<std::string>( "tumblr_last_user", m_TumblrUser );
}

void ImageScraper::TumblrPanel::SaveSearchHistory( )
{
    if( !m_AppConfig ) return;

    m_AppConfig->SetValue<std::string>( "tumblr_last_user", m_TumblrUser );
    if( !m_AppConfig->Serialise( ) )
    {
        WarningLog( "[%s] Failed to save Tumblr search history", __FUNCTION__ );
    }
}

void ImageScraper::TumblrPanel::Update( )
{
    if( ImGui::BeginChild( "TumblrUser", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_TumblrUser.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Tumblr User", buffer, INPUT_STRING_MAX, flags ) )
        {
            m_TumblrUser = buffer;
            SaveSearchHistory( );
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
    options.m_Provider  = ContentProvider::Tumblr;
    options.m_TumblrUser          = m_TumblrUser;
    options.m_TumblrMaxMediaItems = m_TumblrMaxMediaItems;
    return options;
}
