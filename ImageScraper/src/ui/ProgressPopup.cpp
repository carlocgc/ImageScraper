#include "ui/ProgressPopup.h"

bool ImageScraper::DrawProgressPopupContents( const ProgressPopupContent& content )
{
    ImGui::TextWrapped( "%s", content.m_Message.c_str( ) );
    ImGui::Spacing( );
    ImGui::ProgressBar( content.m_ProgressFraction, ImVec2( content.m_ProgressBarWidth, 0.0f ) );

    if( !content.m_PrimaryDetail.empty( ) )
    {
        ImGui::TextDisabled( "%s", content.m_PrimaryDetail.c_str( ) );
    }

    if( !content.m_SecondaryDetail.empty( ) )
    {
        ImGui::TextDisabled( "%s", content.m_SecondaryDetail.c_str( ) );
    }

    if( !content.m_CurrentPath.empty( ) )
    {
        ImGui::Spacing( );
        ImGui::PushTextWrapPos( ImGui::GetCursorPosX( ) + content.m_ProgressBarWidth );
        ImGui::TextDisabled( "%s", content.m_CurrentPath.c_str( ) );
        ImGui::PopTextWrapPos( );
    }

    bool actionRequested = false;
    if( content.m_ActionLabel != nullptr && content.m_ActionLabel[0] != '\0' )
    {
        ImGui::Spacing( );
        actionRequested = ImGui::Button( content.m_ActionLabel, content.m_ActionButtonSize );
    }
    return actionRequested;
}
