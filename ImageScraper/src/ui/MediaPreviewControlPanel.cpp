#define NOMINMAX
#include "ui/MediaPreviewControlPanel.h"
#include "imgui/imgui.h"

#include <algorithm>

ImageScraper::MediaPreviewControlPanel::MediaPreviewControlPanel(
    MediaPreviewPanel* previewPanel,
    DownloadHistoryPanel* historyPanel )
    : m_PreviewPanel{ previewPanel }
    , m_HistoryPanel{ historyPanel }
{
}

void ImageScraper::MediaPreviewControlPanel::Update( )
{
    // Base (minimum) button dimensions
    constexpr float k_PlayR_min   = 18.f;
    constexpr float k_NavR_min    = 13.f;
    constexpr float k_PlayDia_min = k_PlayR_min * 2.f;
    constexpr float k_NavDia_min  = k_NavR_min  * 2.f;
    constexpr float k_Spacing_min = 8.f;
    constexpr float k_MinContentW = k_NavDia_min + k_Spacing_min + k_PlayDia_min + k_Spacing_min + k_NavDia_min;
    constexpr float k_MinContentH = k_PlayDia_min;

    // Prevent the window from being resized smaller than needed to show the buttons at base size
    {
        const ImGuiStyle& style = ImGui::GetStyle( );
        const float minW = k_MinContentW + style.WindowPadding.x * 2.f;
        const float minH = k_MinContentH + style.WindowPadding.y * 2.f + ImGui::GetFrameHeight( );
        ImGui::SetNextWindowSizeConstraints( ImVec2( minW, minH ), ImVec2( FLT_MAX, FLT_MAX ) );
    }

    ImGui::SetNextWindowSize( ImVec2( 200, 80 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Media Controls", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    // Scale buttons proportionally to fill the available content region.
    // Scale is always >= 1 so buttons never shrink below their base size.
    const float availW  = ImGui::GetContentRegionAvail( ).x;
    const float availH  = ImGui::GetContentRegionAvail( ).y;
    const float scaleW  = availW / k_MinContentW;
    const float scaleH  = availH / k_MinContentH;
    const float scale   = std::max( 1.f, std::min( scaleW, scaleH ) );

    const float k_PlayR   = k_PlayR_min   * scale;
    const float k_NavR    = k_NavR_min    * scale;
    const float k_PlayDia = k_PlayR * 2.f;
    const float k_NavDia  = k_NavR  * 2.f;
    const float k_Spacing = k_Spacing_min * scale;
    const float k_TotalW  = k_NavDia + k_Spacing + k_PlayDia + k_Spacing + k_NavDia;

    const float startX  = ( availW - k_TotalW ) * 0.5f;
    const float baseY   = ( availH - k_PlayDia ) * 0.5f;
    const float navOffY = ( k_PlayDia - k_NavDia ) * 0.5f;

    const float cursorBaseX = ImGui::GetCursorPosX( );
    const float cursorBaseY = ImGui::GetCursorPosY( );

    const bool canGoBack    = m_HistoryPanel && m_HistoryPanel->HasNext( );
    const bool canGoForward = m_HistoryPanel && m_HistoryPanel->HasPrevious( );
    const bool canPlay      = m_PreviewPanel && m_PreviewPanel->CanPlayPause( );
    const bool playing      = m_PreviewPanel && m_PreviewPanel->IsPlaying( );

    ImDrawList* dl = ImGui::GetWindowDrawList( );

    // Draws a circle button background and returns {pressed, screenCenter, iconCol}.
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

        const char*  label = "<<";
        const ImVec2 sz    = ImGui::CalcTextSize( label );
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

        const char*  label = ">>";
        const ImVec2 sz    = ImGui::CalcTextSize( label );
        dl->AddText( ImVec2( center.x - sz.x * 0.5f, center.y - sz.y * 0.5f ), iconCol, label );

        if( pressed )
        {
            m_HistoryPanel->SelectPrevious( );
        }
    }

    ImGui::End( );
}
