#include "ui/MediaPreviewControlPanel.h"
#include "imgui/imgui.h"

ImageScraper::MediaPreviewControlPanel::MediaPreviewControlPanel(
    MediaPreviewPanel* previewPanel,
    DownloadHistoryPanel* historyPanel )
    : m_PreviewPanel{ previewPanel }
    , m_HistoryPanel{ historyPanel }
{
}

void ImageScraper::MediaPreviewControlPanel::Update( )
{
    ImGui::SetNextWindowSize( ImVec2( 300, 55 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Media Controls", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    constexpr float k_BtnW    = 40.f;
    constexpr float k_BtnH    = 30.f;
    constexpr float k_Spacing = 8.f;
    constexpr float k_TotalW  = k_BtnW * 3.f + k_Spacing * 2.f;

    const float startX = ( ImGui::GetContentRegionAvail( ).x - k_TotalW ) * 0.5f;
    ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + startX );

    const bool hasPrev = m_HistoryPanel && m_HistoryPanel->HasPrevious( );
    ImGui::BeginDisabled( !hasPrev );
    if( ImGui::Button( "|<", ImVec2( k_BtnW, k_BtnH ) ) )
    {
        m_HistoryPanel->SelectPrevious( );
    }
    ImGui::EndDisabled( );

    ImGui::SameLine( 0.f, k_Spacing );

    const bool canPlay = m_PreviewPanel && m_PreviewPanel->CanPlayPause( );
    ImGui::BeginDisabled( !canPlay );
    const char* playLabel = ( m_PreviewPanel && m_PreviewPanel->IsPlaying( ) ) ? "||" : " > ";
    if( ImGui::Button( playLabel, ImVec2( k_BtnW, k_BtnH ) ) )
    {
        m_PreviewPanel->TogglePlayPause( );
    }
    ImGui::EndDisabled( );

    ImGui::SameLine( 0.f, k_Spacing );

    const bool hasNext = m_HistoryPanel && m_HistoryPanel->HasNext( );
    ImGui::BeginDisabled( !hasNext );
    if( ImGui::Button( ">|", ImVec2( k_BtnW, k_BtnH ) ) )
    {
        m_HistoryPanel->SelectNext( );
    }
    ImGui::EndDisabled( );

    ImGui::End( );
}
