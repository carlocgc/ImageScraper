#include "ui/FourChanPanel.h"

void ImageScraper::FourChanPanel::Update( )
{
    if( ImGui::BeginChild( "FourChanBoard", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_FourChanBoard.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Board (e.g. v, sci )", buffer, INPUT_STRING_MAX, flags ) )
        {
            m_FourChanBoard = buffer;
        }
    }

    ImGui::EndChild( );
}

ImageScraper::UserInputOptions ImageScraper::FourChanPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider            = ContentProvider::FourChan;
    options.m_FourChanBoard       = m_FourChanBoard;
    options.m_FourChanMaxThreads  = m_FourChanMaxThreads;
    options.m_FourChanMaxMediaItems = m_FourChanMaxMediaItems;
    return options;
}
