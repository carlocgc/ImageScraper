#define NOMINMAX
#include "ui/MediaPreviewControlPanel.h"
#include "imgui/imgui.h"

#include <algorithm>

namespace
{
    constexpr const char* kIconPrevious = u8"\uE892";
    constexpr const char* kIconNext     = u8"\uE893";
    constexpr const char* kIconPlay     = u8"\uE768";
    constexpr const char* kIconPause    = u8"\uE769";
    constexpr const char* kIconMuted    = u8"\uE74F";
    constexpr const char* kIconVolume0  = u8"\uE992";

    constexpr const char* kFallbackPrevious = "|<";
    constexpr const char* kFallbackNext     = ">|";
    constexpr const char* kFallbackPlay     = ">";
    constexpr const char* kFallbackPause    = "||";
    constexpr const char* kFallbackMuted    = "x";
    constexpr const char* kFallbackVolume0  = "o";
}

ImageScraper::MediaPreviewControlPanel::MediaPreviewControlPanel(
    MediaPreviewPanel* previewPanel,
    DownloadHistoryPanel* historyPanel,
    ImFont* iconFont )
    : m_PreviewPanel{ previewPanel }
    , m_HistoryPanel{ historyPanel }
    , m_IconFont{ iconFont }
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

    auto DrawButtonIcon = [ & ]( ImVec2 center, float radius, const char* iconGlyph, const char* fallbackGlyph, ImU32 iconCol )
    {
        ImFont* font = m_IconFont ? m_IconFont : ImGui::GetFont( );
        const float fontSize = m_IconFont ? radius * 1.395f : radius * 1.05f;
        const char* glyph = m_IconFont ? iconGlyph : fallbackGlyph;
        const ImVec2 textSize = font->CalcTextSizeA( fontSize, FLT_MAX, 0.0f, glyph );
        const float verticalNudge = m_IconFont ? 0.5f : 0.0f;
        const ImVec2 textPos = ImVec2(
            center.x - textSize.x * 0.5f,
            center.y - textSize.y * 0.5f + verticalNudge );

        dl->AddText( font, fontSize, textPos, iconCol, glyph );
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
        DrawButtonIcon( center, k_NavR, kIconPrevious, kFallbackPrevious, iconCol );

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
        DrawButtonIcon( center, k_PlayR, playing ? kIconPause : kIconPlay, playing ? kFallbackPause : kFallbackPlay, iconCol );

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
        DrawButtonIcon( center, k_NavR, kIconNext, kFallbackNext, iconCol );

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
        DrawButtonIcon( center, k_NavR, muted ? kIconMuted : kIconVolume0, muted ? kFallbackMuted : kFallbackVolume0, iconCol );

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
