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
    ImGui::SetNextWindowSize( ImVec2( 200, 80 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Media Controls", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    constexpr float k_PlayR   = 18.f;
    constexpr float k_NavR    = 13.f;
    constexpr float k_PlayDia = k_PlayR * 2.f;
    constexpr float k_NavDia  = k_NavR * 2.f;
    constexpr float k_Spacing = 8.f;
    constexpr float k_TotalW  = k_NavDia + k_Spacing + k_PlayDia + k_Spacing + k_NavDia;

    const float availW  = ImGui::GetContentRegionAvail( ).x;
    const float availH  = ImGui::GetContentRegionAvail( ).y;
    const float startX  = ( availW - k_TotalW ) * 0.5f;
    const float baseY   = ( availH - k_PlayDia ) * 0.5f;
    const float navOffY = ( k_PlayDia - k_NavDia ) * 0.5f;

    const float cursorBaseX = ImGui::GetCursorPosX( );
    const float cursorBaseY = ImGui::GetCursorPosY( );

    // canGoBack    = can navigate to older items (lower ring-buffer index)
    // canGoForward = can navigate to newer/latest items (higher ring-buffer index)
    const bool canGoBack    = m_HistoryPanel && m_HistoryPanel->HasNext( );
    const bool canGoForward = m_HistoryPanel && m_HistoryPanel->HasPrevious( );
    const bool canPlay      = m_PreviewPanel && m_PreviewPanel->CanPlayPause( );
    const bool playing      = m_PreviewPanel && m_PreviewPanel->IsPlaying( );

    ImDrawList* dl = ImGui::GetWindowDrawList( );

    // Draws a circular button background; returns {pressed, screenCenter, iconCol}.
    struct BtnResult { bool pressed; ImVec2 center; ImU32 iconCol; };

    auto CircleBtn = [ & ]( const char* id, float localX, float localY, float radius, bool disabled ) -> BtnResult
    {
        ImGui::SetCursorPos( ImVec2( cursorBaseX + startX + localX, cursorBaseY + baseY + localY ) );
        const ImVec2 screenTL = ImGui::GetCursorScreenPos( );
        const ImVec2 center   = ImVec2( screenTL.x + radius, screenTL.y + radius );

        ImGui::BeginDisabled( disabled );
        const bool pressed = ImGui::InvisibleButton( id, ImVec2( radius * 2.f, radius * 2.f ) );
        const bool hovered = ImGui::IsItemHovered( );
        const bool active  = ImGui::IsItemActive( );
        ImGui::EndDisabled( );

        const ImU32 bgCol = disabled    ? IM_COL32(  60,  60,  60, 120 )
                          : active      ? ImGui::GetColorU32( ImGuiCol_ButtonActive )
                          : hovered     ? ImGui::GetColorU32( ImGuiCol_ButtonHovered )
                          :               ImGui::GetColorU32( ImGuiCol_Button );

        dl->AddCircleFilled( center, radius, bgCol );
        dl->AddCircle( center, radius, IM_COL32( 120, 120, 120, 100 ), 0, 1.5f );

        const ImU32 iconCol = disabled
            ? IM_COL32( 180, 180, 180, 60 )
            : ImGui::GetColorU32( ImGuiCol_Text );

        return { pressed, center, iconCol };
    };

    // --- Back (<<) - toward oldest ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn( "##back", 0.f, navOffY, k_NavR, !canGoBack );

        const char*    label = "<<";
        const ImVec2   sz    = ImGui::CalcTextSize( label );
        dl->AddText( ImVec2( center.x - sz.x * 0.5f, center.y - sz.y * 0.5f ), iconCol, label );

        if( pressed )
        {
            m_HistoryPanel->SelectNext( );
        }
    }

    // --- Play / Pause ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn( "##play", k_NavDia + k_Spacing, 0.f, k_PlayR, !canPlay );

        if( playing )
        {
            // Pause: two vertical filled bars
            const float bw  = k_PlayR * 0.18f;
            const float bh  = k_PlayR * 0.45f;
            const float gap = k_PlayR * 0.12f;
            dl->AddRectFilled(
                ImVec2( center.x - gap - bw, center.y - bh ),
                ImVec2( center.x - gap,       center.y + bh ), iconCol );
            dl->AddRectFilled(
                ImVec2( center.x + gap,       center.y - bh ),
                ImVec2( center.x + gap + bw,  center.y + bh ), iconCol );
        }
        else
        {
            // Play: right-pointing filled triangle
            const float th = k_PlayR * 0.48f;
            const float tw = k_PlayR * 0.52f;
            dl->AddTriangleFilled(
                ImVec2( center.x - tw * 0.35f, center.y - th ),
                ImVec2( center.x - tw * 0.35f, center.y + th ),
                ImVec2( center.x + tw * 0.65f, center.y ),
                iconCol );
        }

        if( pressed )
        {
            m_PreviewPanel->TogglePlayPause( );
        }
    }

    // --- Forward (>>) - toward latest ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn( "##forward", k_NavDia + k_Spacing + k_PlayDia + k_Spacing, navOffY, k_NavR, !canGoForward );

        const char*    label = ">>";
        const ImVec2   sz    = ImGui::CalcTextSize( label );
        dl->AddText( ImVec2( center.x - sz.x * 0.5f, center.y - sz.y * 0.5f ), iconCol, label );

        if( pressed )
        {
            m_HistoryPanel->SelectPrevious( );
        }
    }

    ImGui::End( );
}
