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
    constexpr float k_MinContentW = k_NavDia_min + k_Spacing_min + k_PlayDia_min + k_Spacing_min +
                                    k_NavDia_min + k_Spacing_min + k_NavDia_min;
    constexpr float k_MinContentH = k_PlayDia_min;

    // Prevent the window from being resized smaller than needed to show the buttons at base size
    {
        const ImGuiStyle& style = ImGui::GetStyle( );
        const float minW = k_MinContentW + style.WindowPadding.x * 2.f;
        const float minH = k_MinContentH + style.WindowPadding.y * 2.f + ImGui::GetFrameHeight( );
        ImGui::SetNextWindowSizeConstraints( ImVec2( minW, minH ), ImVec2( FLT_MAX, FLT_MAX ) );
    }

    ImGui::SetNextWindowSize( ImVec2( 240, 80 ), ImGuiCond_FirstUseEver );

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
    const float k_TotalW  = k_NavDia + k_Spacing + k_PlayDia + k_Spacing + k_NavDia + k_Spacing + k_NavDia;

    const float startX  = ( availW - k_TotalW ) * 0.5f;
    const float baseY   = ( availH - k_PlayDia ) * 0.5f;
    const float navOffY = ( k_PlayDia - k_NavDia ) * 0.5f;

    const float cursorBaseX = ImGui::GetCursorPosX( );
    const float cursorBaseY = ImGui::GetCursorPosY( );

    const bool canGoBack    = m_HistoryPanel && m_HistoryPanel->HasNext( );
    const bool canGoForward = m_HistoryPanel && m_HistoryPanel->HasPrevious( );
    const bool canPlay      = m_PreviewPanel && m_PreviewPanel->CanPlayPause( );
    const bool canMute      = m_PreviewPanel && m_PreviewPanel->CanMute( );
    const bool playing      = m_PreviewPanel && m_PreviewPanel->IsPlaying( );
    const bool muted        = !m_PreviewPanel || m_PreviewPanel->IsMuted( );

    ImDrawList* dl = ImGui::GetWindowDrawList( );

    struct BtnResult
    {
        bool pressed;
        ImVec2 center;
        ImU32 iconCol;
    };

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

        const ImU32 bgCol = disabled    ? IM_COL32( 60, 60, 60, 120 )
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

    // --- Back (|<<) - toward oldest ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn( "##back", 0.f, navOffY, k_NavR, !canGoBack );

        const float tw     = k_NavR * 0.36f;
        const float th     = k_NavR * 0.40f;
        const float ov     = k_NavR * 0.07f;
        const float bw     = k_NavR * 0.10f;
        const float bg     = k_NavR * 0.04f;
        const float totalW = tw + ( tw - ov ) + bg + bw;
        const float rx     = center.x + totalW * 0.5f;

        dl->AddTriangleFilled(
            ImVec2( rx, center.y - th ),
            ImVec2( rx, center.y + th ),
            ImVec2( rx - tw, center.y ),
            iconCol );

        dl->AddTriangleFilled(
            ImVec2( rx - tw + ov, center.y - th ),
            ImVec2( rx - tw + ov, center.y + th ),
            ImVec2( rx - 2.f * tw + ov, center.y ),
            iconCol );

        const float barRight = rx - 2.f * tw + ov - bg;
        dl->AddRectFilled(
            ImVec2( barRight - bw, center.y - th ),
            ImVec2( barRight, center.y + th ),
            iconCol );

        if( pressed )
        {
            m_HistoryPanel->SelectNext( );
        }

        if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( "Previous item" );
        }
    }

    // --- Play / pause ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn( "##play", k_NavDia + k_Spacing, 0.f, k_PlayR, !canPlay );

        if( playing )
        {
            const float bw  = k_PlayR * 0.18f;
            const float bh  = k_PlayR * 0.45f;
            const float gap = k_PlayR * 0.12f;
            dl->AddRectFilled(
                ImVec2( center.x - gap - bw, center.y - bh ),
                ImVec2( center.x - gap, center.y + bh ),
                iconCol );
            dl->AddRectFilled(
                ImVec2( center.x + gap, center.y - bh ),
                ImVec2( center.x + gap + bw, center.y + bh ),
                iconCol );
        }
        else
        {
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

        if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( playing ? "Pause preview" : "Play preview" );
        }
    }

    // --- Forward (>>|) - toward latest ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn(
            "##forward",
            k_NavDia + k_Spacing + k_PlayDia + k_Spacing,
            navOffY,
            k_NavR,
            !canGoForward );

        const float tw     = k_NavR * 0.36f;
        const float th     = k_NavR * 0.40f;
        const float ov     = k_NavR * 0.07f;
        const float bw     = k_NavR * 0.10f;
        const float bg     = k_NavR * 0.04f;
        const float totalW = tw + ( tw - ov ) + bg + bw;
        const float lx     = center.x - totalW * 0.5f;

        dl->AddTriangleFilled(
            ImVec2( lx, center.y - th ),
            ImVec2( lx, center.y + th ),
            ImVec2( lx + tw, center.y ),
            iconCol );

        dl->AddTriangleFilled(
            ImVec2( lx + tw - ov, center.y - th ),
            ImVec2( lx + tw - ov, center.y + th ),
            ImVec2( lx + 2.f * tw - ov, center.y ),
            iconCol );

        const float barLeft = lx + 2.f * tw - ov + bg;
        dl->AddRectFilled(
            ImVec2( barLeft, center.y - th ),
            ImVec2( barLeft + bw, center.y + th ),
            iconCol );

        if( pressed )
        {
            m_HistoryPanel->SelectPrevious( );
        }

        if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( "Next item" );
        }
    }

    // --- Mute / unmute ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn(
            "##mute",
            k_NavDia + k_Spacing + k_PlayDia + k_Spacing + k_NavDia + k_Spacing,
            navOffY,
            k_NavR,
            !canMute );

        const float bodyW  = k_NavR * 0.22f;
        const float bodyH  = k_NavR * 0.34f;
        const float coneW  = k_NavR * 0.30f;
        const float waveR1 = k_NavR * 0.42f;
        const float waveR2 = k_NavR * 0.62f;

        dl->AddRectFilled(
            ImVec2( center.x + coneW * 0.55f, center.y - bodyH ),
            ImVec2( center.x + bodyW + coneW * 0.55f, center.y + bodyH ),
            iconCol );
        dl->AddTriangleFilled(
            ImVec2( center.x + coneW * 0.55f, center.y - bodyH * 1.1f ),
            ImVec2( center.x + coneW * 0.55f, center.y + bodyH * 1.1f ),
            ImVec2( center.x - coneW * 0.45f, center.y ),
            iconCol );

        if( muted )
        {
            const float slash = k_NavR * 0.62f;
            dl->AddLine(
                ImVec2( center.x + slash * 0.35f, center.y + slash * 0.35f ),
                ImVec2( center.x - slash * 0.55f, center.y - slash * 0.55f ),
                iconCol,
                std::max( 1.5f, scale ) );
        }
        else
        {
            dl->AddBezierCubic(
                ImVec2( center.x - coneW * 0.35f, center.y - waveR1 * 0.45f ),
                ImVec2( center.x - waveR1 * 0.75f, center.y - waveR1 * 0.40f ),
                ImVec2( center.x - waveR1 * 0.75f, center.y + waveR1 * 0.40f ),
                ImVec2( center.x - coneW * 0.35f, center.y + waveR1 * 0.45f ),
                iconCol,
                std::max( 1.5f, scale ) );
            dl->AddBezierCubic(
                ImVec2( center.x - coneW * 0.55f, center.y - waveR2 * 0.55f ),
                ImVec2( center.x - waveR2 * 0.95f, center.y - waveR2 * 0.45f ),
                ImVec2( center.x - waveR2 * 0.95f, center.y + waveR2 * 0.45f ),
                ImVec2( center.x - coneW * 0.55f, center.y + waveR2 * 0.55f ),
                iconCol,
                std::max( 1.5f, scale ) );
        }

        if( pressed )
        {
            m_PreviewPanel->ToggleMute( );
        }

        if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( muted ? "Unmute preview" : "Mute preview" );
        }
    }

    ImGui::End( );
}
