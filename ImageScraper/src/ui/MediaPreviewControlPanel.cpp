#define NOMINMAX
#include "ui/MediaPreviewControlPanel.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#pragma comment( lib, "gdiplus.lib" )

namespace
{
    constexpr const char* kFallbackPrevious   = "|<";
    constexpr const char* kFallbackNext       = ">|";
    constexpr const char* kFallbackPlay       = ">";
    constexpr const char* kFallbackPause      = "||";
    constexpr const char* kFallbackMuted      = "x";
    constexpr const char* kFallbackVolume0    = "o";
    constexpr const char* kFallbackFullscreen = "[ ]";

    class GdiplusSession
    {
    public:
        GdiplusSession( )
        {
            Gdiplus::GdiplusStartupInput startupInput;
            const Gdiplus::Status status = Gdiplus::GdiplusStartup( &m_Token, &startupInput, nullptr );
            m_IsValid = ( status == Gdiplus::Ok );
        }

        ~GdiplusSession( )
        {
            if( m_IsValid )
            {
                Gdiplus::GdiplusShutdown( m_Token );
            }
        }

        bool IsValid( ) const { return m_IsValid; }

    private:
        ULONG_PTR m_Token{ 0 };
        bool      m_IsValid{ false };
    };

    const GdiplusSession& GetGdiplusSession( )
    {
        static const GdiplusSession session;
        return session;
    }

    ImageScraper::MediaPreviewControlPanel::IconTexture LoadIconTexture(
        const std::filesystem::path& path )
    {
        if( !GetGdiplusSession( ).IsValid( ) || !std::filesystem::exists( path ) )
        {
            return { };
        }

        const std::wstring widePath = path.wstring( );
        Gdiplus::Bitmap sourceBitmap( widePath.c_str( ) );
        if( sourceBitmap.GetLastStatus( ) != Gdiplus::Ok )
        {
            return { };
        }

        const int width  = static_cast<int>( sourceBitmap.GetWidth( ) );
        const int height = static_cast<int>( sourceBitmap.GetHeight( ) );
        if( width <= 0 || height <= 0 )
        {
            return { };
        }

        Gdiplus::Bitmap rgbaBitmap( width, height, PixelFormat32bppARGB );
        Gdiplus::Graphics graphics( &rgbaBitmap );
        if( graphics.GetLastStatus( ) != Gdiplus::Ok ||
            graphics.DrawImage( &sourceBitmap, 0, 0, width, height ) != Gdiplus::Ok )
        {
            return { };
        }

        Gdiplus::Rect lockRect( 0, 0, width, height );
        Gdiplus::BitmapData bitmapData{ };
        if( rgbaBitmap.LockBits( &lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData ) != Gdiplus::Ok )
        {
            return { };
        }

        std::vector<unsigned char> pixels( static_cast<size_t>( width ) * static_cast<size_t>( height ) * 4 );

        for( int y = 0; y < height; ++y )
        {
            const auto* srcRow = static_cast<const unsigned char*>( bitmapData.Scan0 ) + ( static_cast<size_t>( y ) * static_cast<size_t>( bitmapData.Stride ) );
            auto* dstRow = pixels.data( ) + ( static_cast<size_t>( y ) * static_cast<size_t>( width ) * 4 );

            for( int x = 0; x < width; ++x )
            {
                const unsigned char blue  = srcRow[ x * 4 + 0 ];
                const unsigned char green = srcRow[ x * 4 + 1 ];
                const unsigned char red   = srcRow[ x * 4 + 2 ];
                const unsigned char alpha = srcRow[ x * 4 + 3 ];

                dstRow[ x * 4 + 0 ] = red;
                dstRow[ x * 4 + 1 ] = green;
                dstRow[ x * 4 + 2 ] = blue;
                dstRow[ x * 4 + 3 ] = alpha;
            }
        }

        rgbaBitmap.UnlockBits( &bitmapData );

        GLuint texture = 0;
        glGenTextures( 1, &texture );
        glBindTexture( GL_TEXTURE_2D, texture );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glTexImage2D( GL_TEXTURE_2D,
                      0,
                      GL_RGBA,
                      width,
                      height,
                      0,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      pixels.data( ) );
        glBindTexture( GL_TEXTURE_2D, 0 );

        return { texture, width, height };
    }
}

ImageScraper::MediaPreviewControlPanel::~MediaPreviewControlPanel( )
{
    ReleaseIcons( );
}

ImageScraper::MediaPreviewControlPanel::MediaPreviewControlPanel(
    MediaPreviewPanel* previewPanel,
    DownloadHistoryPanel* historyPanel,
    ImFont* iconFont,
    const std::filesystem::path& assetRoot )
    : m_PreviewPanel{ previewPanel }
    , m_HistoryPanel{ historyPanel }
    , m_IconFont{ iconFont }
    , m_AssetRoot{ assetRoot }
{
}

void ImageScraper::MediaPreviewControlPanel::EnsureIconsLoaded( )
{
    if( m_IconsAttempted )
    {
        return;
    }

    m_IconsAttempted = true;

    m_PreviousIcon   = LoadIconTexture( m_AssetRoot / "previous.png" );
    m_NextIcon       = LoadIconTexture( m_AssetRoot / "next.png" );
    m_PlayIcon       = LoadIconTexture( m_AssetRoot / "play.png" );
    m_PauseIcon      = LoadIconTexture( m_AssetRoot / "pause.png" );
    m_MutedIcon      = LoadIconTexture( m_AssetRoot / "muted.png" );
    m_VolumeIcon     = LoadIconTexture( m_AssetRoot / "volume.png" );
    m_FullscreenIcon = LoadIconTexture( m_AssetRoot / "fullscreen.png" );
}

void ImageScraper::MediaPreviewControlPanel::ReleaseIcons( )
{
    auto ReleaseIcon = []( IconTexture& icon )
    {
        if( icon.m_Texture != 0 )
        {
            glDeleteTextures( 1, &icon.m_Texture );
        }
        icon = { };
    };

    ReleaseIcon( m_PreviousIcon );
    ReleaseIcon( m_NextIcon );
    ReleaseIcon( m_PlayIcon );
    ReleaseIcon( m_PauseIcon );
    ReleaseIcon( m_MutedIcon );
    ReleaseIcon( m_VolumeIcon );
    ReleaseIcon( m_FullscreenIcon );
}

void ImageScraper::MediaPreviewControlPanel::Update( )
{
    EnsureIconsLoaded( );

    constexpr const char* kBlockedTooltip = "Controls unavailable while a download is running";
    constexpr const char* kPrivacyTooltip = "Controls unavailable while privacy mode is on";

    // Base (minimum) button dimensions
    constexpr float k_PlayR_min   = 18.f;
    constexpr float k_NavR_min    = 13.f;
    constexpr float k_PlayDia_min = k_PlayR_min * 2.f;
    constexpr float k_NavDia_min  = k_NavR_min  * 2.f;
    constexpr float k_Spacing_min = 8.f;
    constexpr float k_MinContentW = k_NavDia_min + k_Spacing_min + k_PlayDia_min + k_Spacing_min +
                                    k_NavDia_min + k_Spacing_min + k_NavDia_min + k_Spacing_min +
                                    k_NavDia_min;
    constexpr float k_MinContentH = k_PlayDia_min;

    // Scrub bar reservation along the top edge of the content region.
    constexpr float k_ScrubHitH    = 14.f;  // generous click target
    constexpr float k_ScrubBarRest = 6.f;
    constexpr float k_ScrubBarHot  = 10.f;
    constexpr float k_ScrubGap     = 6.f;   // gap between scrub strip and buttons

    // Prevent the window from being resized smaller than needed to show the buttons at base size
    {
        const ImGuiStyle& style = ImGui::GetStyle( );
        const float minW = k_MinContentW + style.WindowPadding.x * 2.f;
        const float minH = k_MinContentH + k_ScrubHitH + k_ScrubGap +
                           style.WindowPadding.y * 2.f + ImGui::GetFrameHeight( );
        ImGui::SetNextWindowSizeConstraints( ImVec2( minW, minH ), ImVec2( FLT_MAX, FLT_MAX ) );
    }

    ImGui::SetNextWindowSize( ImVec2( 240, 80 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Media Controls", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    const float fullAvailW = ImGui::GetContentRegionAvail( ).x;
    const float fullAvailH = ImGui::GetContentRegionAvail( ).y;

    // --- Scrub bar (top edge of content region) ---
    {
        const bool blocked = m_Blocked;
        const bool privacy = m_PreviewPanel && m_PreviewPanel->IsPrivacyMode( );
        const bool canScrub = m_PreviewPanel && m_PreviewPanel->CanScrub( ) && !blocked && !privacy;
        const float progress = m_PreviewPanel ? m_PreviewPanel->GetProgress( ) : -1.0f;

        const ImVec2 cursorTL = ImGui::GetCursorScreenPos( );
        const float barAreaY = cursorTL.y;
        const float barAreaX = cursorTL.x;

        ImGui::PushID( "##scrubBar" );
        ImGui::BeginDisabled( !canScrub );
        ImGui::InvisibleButton( "scrubHit", ImVec2( fullAvailW, k_ScrubHitH ) );
        const bool hovered = ImGui::IsItemHovered( );
        const bool active  = ImGui::IsItemActive( );

        if( canScrub )
        {
            if( ImGui::IsItemActivated( ) )
            {
                m_PreviewPanel->BeginScrub( );
            }
            if( active )
            {
                const float mouseX = ImGui::GetIO( ).MousePos.x;
                const float t = std::clamp( ( mouseX - barAreaX ) / std::max( fullAvailW, 1.f ), 0.f, 1.f );
                m_PreviewPanel->UpdateScrub( t );
            }
            if( ImGui::IsItemDeactivated( ) )
            {
                const float mouseX = ImGui::GetIO( ).MousePos.x;
                const float t = std::clamp( ( mouseX - barAreaX ) / std::max( fullAvailW, 1.f ), 0.f, 1.f );
                m_PreviewPanel->EndScrub( t );
            }
        }
        ImGui::EndDisabled( );

        // Re-fetch progress after a possible UpdateScrub above so the bar tracks the cursor live.
        const float drawProgress = ( m_PreviewPanel && canScrub ) ? m_PreviewPanel->GetProgress( ) : progress;

        const float barH    = ( hovered || active ) ? k_ScrubBarHot : k_ScrubBarRest;
        const float barY0   = barAreaY + ( k_ScrubHitH - barH ) * 0.5f;
        const float barY1   = barY0 + barH;
        ImDrawList* sdl     = ImGui::GetWindowDrawList( );

        const ImU32 bgCol = canScrub ? IM_COL32( 0, 0, 0, 120 ) : IM_COL32( 60, 60, 60, 90 );
        sdl->AddRectFilled( ImVec2( barAreaX, barY0 ),
                            ImVec2( barAreaX + fullAvailW, barY1 ), bgCol );

        if( drawProgress >= 0.0f )
        {
            const ImU32 fillCol = canScrub ? IM_COL32( 100, 180, 255, 220 )
                                           : IM_COL32( 130, 130, 130, 160 );
            const float fillW = fullAvailW * std::clamp( drawProgress, 0.0f, 1.0f );
            sdl->AddRectFilled( ImVec2( barAreaX, barY0 ),
                                ImVec2( barAreaX + fillW, barY1 ), fillCol );

            if( canScrub && ( hovered || active ) )
            {
                const float handleR = barH * 0.9f;
                sdl->AddCircleFilled( ImVec2( barAreaX + fillW, ( barY0 + barY1 ) * 0.5f ),
                                      handleR, IM_COL32( 230, 240, 255, 255 ) );
            }
        }

        if( hovered && canScrub && m_PreviewPanel )
        {
            const std::string label = m_PreviewPanel->GetProgressLabel( );
            if( !label.empty( ) )
            {
                ImGui::SetTooltip( "%s", label.c_str( ) );
            }
        }
        ImGui::PopID( );

        // Advance cursor past the scrub strip so buttons render below it.
        ImGui::Dummy( ImVec2( 0.0f, k_ScrubGap ) );
    }

    // Scale buttons proportionally to fill the remaining content region.
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
    const float k_TotalW  = k_NavDia + k_Spacing + k_PlayDia + k_Spacing + k_NavDia + k_Spacing +
                            k_NavDia + k_Spacing + k_NavDia;

    const float startX  = ( availW - k_TotalW ) * 0.5f;
    const float baseY   = ( availH - k_PlayDia ) * 0.5f;
    const float navOffY = ( k_PlayDia - k_NavDia ) * 0.5f;

    const float cursorBaseX = ImGui::GetCursorPosX( );
    const float cursorBaseY = ImGui::GetCursorPosY( );

    const bool canGoBack    = m_HistoryPanel && m_HistoryPanel->HasPrevious( );
    const bool canGoForward = m_HistoryPanel && m_HistoryPanel->HasNext( );
    const bool canPlay      = m_PreviewPanel && m_PreviewPanel->CanPlayPause( );
    const bool canMute      = m_PreviewPanel && m_PreviewPanel->CanMute( );
    const bool playing      = m_PreviewPanel && m_PreviewPanel->IsPlaying( );
    const float volume      = m_PreviewPanel ? m_PreviewPanel->GetVolume( ) : 1.0f;
    const bool muted        = !m_PreviewPanel || m_PreviewPanel->IsMuted( ) || volume <= 0.0f;
    const bool blocked      = m_Blocked;
    const bool privacy      = m_PreviewPanel && m_PreviewPanel->IsPrivacyMode( );

    ImDrawList* dl = ImGui::GetWindowDrawList( );

    struct BtnResult
    {
        bool pressed;
        ImVec2 center;
        ImU32 iconCol;
    };

    auto DrawButtonIcon = [ & ]( ImVec2 center,
                                 float radius,
                                 const IconTexture& iconTexture,
                                 const char* fallbackGlyph,
                                 ImU32 iconCol )
    {
        if( iconTexture.m_Texture != 0 && iconTexture.m_Width > 0 && iconTexture.m_Height > 0 )
        {
            float drawW = radius * 1.2f;
            float drawH = drawW * ( static_cast<float>( iconTexture.m_Height ) / static_cast<float>( iconTexture.m_Width ) );
            const float maxIconH = radius * 1.2f;
            if( drawH > maxIconH )
            {
                const float scaleDown = maxIconH / drawH;
                drawW *= scaleDown;
                drawH *= scaleDown;
            }

            const ImVec2 min( center.x - drawW * 0.5f, center.y - drawH * 0.5f );
            const ImVec2 max( center.x + drawW * 0.5f, center.y + drawH * 0.5f );
            dl->AddImage( static_cast<ImTextureID>( static_cast<intptr_t>( iconTexture.m_Texture ) ), min, max,
                          ImVec2( 0.0f, 0.0f ), ImVec2( 1.0f, 1.0f ), iconCol );
            return;
        }

        ImFont* font = m_IconFont ? m_IconFont : ImGui::GetFont( );
        const float fontSize = m_IconFont ? radius * 1.395f : radius * 1.05f;
        const ImVec2 textSize = font->CalcTextSizeA( fontSize, FLT_MAX, 0.0f, fallbackGlyph );
        const float verticalNudge = m_IconFont ? 0.5f : 0.0f;
        const ImVec2 textPos(
            center.x - textSize.x * 0.5f,
            center.y - textSize.y * 0.5f + verticalNudge );

        dl->AddText( font, fontSize, textPos, iconCol, fallbackGlyph );
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

    // --- Back (|<<) ---
    {
        const bool disabled = blocked || privacy || !canGoBack;
        auto [ pressed, center, iconCol ] = CircleBtn( "##back", 0.f, navOffY, k_NavR, disabled );
        DrawButtonIcon( center, k_NavR, m_PreviousIcon, kFallbackPrevious, iconCol );

        if( pressed )
        {
            m_HistoryPanel->SelectPrevious( );
        }

        if( blocked && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kBlockedTooltip );
        }
        else if( privacy && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kPrivacyTooltip );
        }
        else if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( "Previous item" );
        }
    }

    // --- Play / pause ---
    {
        const bool disabled = blocked || privacy || !canPlay;
        auto [ pressed, center, iconCol ] = CircleBtn( "##play", k_NavDia + k_Spacing, 0.f, k_PlayR, disabled );
        DrawButtonIcon( center, k_PlayR, playing ? m_PauseIcon : m_PlayIcon, playing ? kFallbackPause : kFallbackPlay, iconCol );

        if( pressed )
        {
            m_PreviewPanel->TogglePlayPause( );
        }

        if( blocked && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kBlockedTooltip );
        }
        else if( privacy && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kPrivacyTooltip );
        }
        else if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( playing ? "Pause preview" : "Play preview" );
        }
    }

    // --- Forward (>>|) ---
    {
        const bool disabled = blocked || privacy || !canGoForward;
        auto [ pressed, center, iconCol ] = CircleBtn(
            "##forward",
            k_NavDia + k_Spacing + k_PlayDia + k_Spacing,
            navOffY,
            k_NavR,
            disabled );
        DrawButtonIcon( center, k_NavR, m_NextIcon, kFallbackNext, iconCol );

        if( pressed )
        {
            m_HistoryPanel->SelectNext( );
        }

        if( blocked && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kBlockedTooltip );
        }
        else if( privacy && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kPrivacyTooltip );
        }
        else if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( "Next item" );
        }
    }

    // --- Mute / unmute (with hover-popup volume slider) ---
    ImVec2 muteCenter{ 0.f, 0.f };
    bool   muteHovered = false;
    bool   muteDisabled = false;
    {
        muteDisabled = blocked || privacy || !canMute;
        auto [ pressed, center, iconCol ] = CircleBtn(
            "##mute",
            k_NavDia + k_Spacing + k_PlayDia + k_Spacing + k_NavDia + k_Spacing,
            navOffY,
            k_NavR,
            muteDisabled );
        DrawButtonIcon( center, k_NavR, muted ? m_MutedIcon : m_VolumeIcon, muted ? kFallbackMuted : kFallbackVolume0, iconCol );
        muteCenter  = center;
        muteHovered = ImGui::IsItemHovered( );

        if( pressed )
        {
            m_PreviewPanel->ToggleMute( );
        }

        if( blocked && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kBlockedTooltip );
        }
        else if( privacy && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( kPrivacyTooltip );
        }
        // Note: no plain hover tooltip here; the slider popup serves as the affordance.
    }

    // --- Fullscreen (stub) ---
    {
        auto [ pressed, center, iconCol ] = CircleBtn(
            "##fullscreen",
            k_NavDia + k_Spacing + k_PlayDia + k_Spacing + k_NavDia + k_Spacing + k_NavDia + k_Spacing,
            navOffY,
            k_NavR,
            false );
        DrawButtonIcon( center, k_NavR, m_FullscreenIcon, kFallbackFullscreen, iconCol );

        if( pressed )
        {
            // Placeholder only. Fullscreen behavior is intentionally not implemented yet.
        }

        if( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( "Fullscreen (not implemented yet)" );
        }
    }

    ImGui::End( );

    // --- Volume slider popup ---
    // Rendered as a separate floating window outside the controls Begin/End so it can
    // overflow past the controls window's bounds.
    if( m_PreviewPanel )
    {
        const bool canShow = canMute && !muteDisabled;
        if( !canShow )
        {
            m_VolumePopupOpen = false;
            m_VolumeHoverLeftAt = 0.0;
        }
        else if( muteHovered )
        {
            m_VolumePopupOpen = true;
        }

        if( m_VolumePopupOpen )
        {
            const float anchorY = muteCenter.y - k_NavR - 6.0f;
            ImGui::SetNextWindowPos( ImVec2( muteCenter.x, anchorY ), ImGuiCond_Always, ImVec2( 0.5f, 1.0f ) );

            constexpr ImGuiWindowFlags k_PopupFlags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_AlwaysAutoResize;

            bool sliderActive    = false;
            bool windowHovered   = false;

            if( ImGui::Begin( "##volumeOverlay", nullptr, k_PopupFlags ) )
            {
                float v = m_PreviewPanel->GetVolume( );
                if( ImGui::VSliderFloat( "##vol", ImVec2( 22.0f, 90.0f ), &v, 0.0f, 1.0f, "" ) )
                {
                    m_PreviewPanel->SetVolume( v, /*persist=*/false );
                }
                sliderActive = ImGui::IsItemActive( );
                if( ImGui::IsItemDeactivatedAfterEdit( ) )
                {
                    m_PreviewPanel->SetVolume( v, /*persist=*/true );
                }

                const int pct = static_cast<int>( std::round( m_PreviewPanel->GetVolume( ) * 100.0f ) );
                char pctText[ 8 ];
                std::snprintf( pctText, sizeof( pctText ), "%d%%", pct );
                const float textW = ImGui::CalcTextSize( pctText ).x;
                const float availPctW = ImGui::GetContentRegionAvail( ).x;
                ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + std::max( 0.0f, ( availPctW - textW ) * 0.5f ) );
                ImGui::TextUnformatted( pctText );

                windowHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem );
            }
            ImGui::End( );

            const bool keepOpen = muteHovered || windowHovered || sliderActive;
            const double now = ImGui::GetTime( );
            if( keepOpen )
            {
                m_VolumeHoverLeftAt = 0.0;
            }
            else
            {
                if( m_VolumeHoverLeftAt == 0.0 )
                {
                    m_VolumeHoverLeftAt = now;
                }
                else if( now - m_VolumeHoverLeftAt > 0.15 )
                {
                    m_VolumePopupOpen = false;
                    m_VolumeHoverLeftAt = 0.0;
                }
            }
        }
    }
}
