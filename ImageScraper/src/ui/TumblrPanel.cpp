#include "ui/TumblrPanel.h"

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
        }
    }

    ImGui::EndChild( );
}

ImageScraper::UserInputOptions ImageScraper::TumblrPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider  = ContentProvider::Tumblr;
    options.m_TumblrUser = m_TumblrUser;
    return options;
}
