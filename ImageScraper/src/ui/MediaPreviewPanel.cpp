#define NOMINMAX
#include "ui/MediaPreviewPanel.h"
#include "utils/DownloadUtils.h"
#include "utils/StringUtils.h"
#include "log/Logger.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace
{
    constexpr size_t k_MaxRetainedReadyGifs = 3;
    constexpr uint64_t k_MaxRetainedGifBytes = 512ull * 1024ull * 1024ull;
    constexpr uint64_t k_MaxSingleGifBytes = 256ull * 1024ull * 1024ull;

    // Segoe MDL2 Assets glyphs for the privacy toggle
    constexpr const char* kIconEyeOpen   = "\xEE\xA2\x90"; // U+E890 View
    constexpr const char* kIconEyeHidden = "\xEE\xB4\x9A"; // U+ED1A Hide
    constexpr const char* kIconEyeOpenFallback   = "o";
    constexpr const char* kIconEyeHiddenFallback = "/";

    std::string FormatDimensionsLabel( int width, int height )
    {
        if( width <= 0 || height <= 0 )
        {
            return { };
        }

        std::ostringstream stream;
        stream << width << " x " << height;
        return stream.str( );
    }
}

ImageScraper::MediaPreviewPanel::~MediaPreviewPanel( )
{
    m_CancelAudioPrepare = true;
    m_CancelDecode = true;
    CancelGifWarmDecode( true );

    if( m_AudioPrepareFuture.valid( ) )
    {
        m_AudioPrepareFuture.wait( );
    }
    if( m_DecodeFuture.valid( ) )
    {
        m_DecodeFuture.wait( );
    }

    StopAudioPlayback( );
    m_AudioPlayer.reset( );
    m_VideoPlayer.reset( );
    FreeTextures( );
}

void ImageScraper::MediaPreviewPanel::Update( )
{
    {
        std::unique_ptr<DecodedMedia> decoded;
        {
            std::lock_guard<std::mutex> lock( m_DecodedMutex );
            decoded = std::move( m_PendingDecoded );
        }
        if( decoded )
        {
            UploadDecoded( std::move( *decoded ) );
        }
    }

    ApplyPreparedGifWarmDecode( );
    ApplyPreparedAudio( );
    KickDecodeIfNeeded( );
    StartNextGifWarmDecode( );

    if( m_MediaState == MediaState::GifPlaying )
    {
        AdvanceGifFrame( );
    }
    else if( m_MediaState == MediaState::VideoPlaying )
    {
        AdvanceVideoFrame( );
    }

    ImVec2 contentScreenMin{ 0.0f, 0.0f };
    ImVec2 contentScreenMax{ 0.0f, 0.0f };
    ImVec2 imageScreenMin{ 0.0f, 0.0f };
    ImVec2 imageScreenMax{ 0.0f, 0.0f };
    bool   hasImageRect = false;
    bool   windowOpen = false;
    ImGui::SetNextWindowSize( ImVec2( 480, 480 ), ImGuiCond_FirstUseEver );

    if( ImGui::Begin( "Media Preview", nullptr ) )
    {
        windowOpen = true;

        const ImVec2 winPos = ImGui::GetWindowPos( );
        contentScreenMin = ImVec2(
            winPos.x + ImGui::GetWindowContentRegionMin( ).x,
            winPos.y + ImGui::GetWindowContentRegionMin( ).y );
        contentScreenMax = ImVec2(
            winPos.x + ImGui::GetWindowContentRegionMax( ).x,
            winPos.y + ImGui::GetWindowContentRegionMax( ).y );

        if( m_PrivacyMode )
        {
            ImGui::GetWindowDrawList( )->AddRectFilled(
                contentScreenMin,
                contentScreenMax,
                IM_COL32_BLACK );
        }
        else if( m_Textures.empty( ) )
        {
            ImGui::TextDisabled( m_IsDecoding ? "Loading preview..." : "No media loaded yet" );
        }
        else
        {
            const ImVec2 avail = ImGui::GetContentRegionAvail( );
            float drawW = static_cast<float>( m_Width );
            float drawH = static_cast<float>( m_Height );

            if( drawW > avail.x )
            {
                drawH = drawH * ( avail.x / drawW );
                drawW = avail.x;
            }
            if( drawH > avail.y )
            {
                drawW = drawW * ( avail.y / drawH );
                drawH = avail.y;
            }

            const float offsetX = ( avail.x - drawW ) * 0.5f;
            const float offsetY = ( avail.y - drawH ) * 0.5f;
            ImGui::SetCursorPos(
                ImVec2(
                    ImGui::GetCursorPosX( ) + offsetX,
                    ImGui::GetCursorPosY( ) + offsetY ) );

            imageScreenMin = ImGui::GetCursorScreenPos( );
            imageScreenMax = ImVec2( imageScreenMin.x + drawW, imageScreenMin.y + drawH );
            hasImageRect = true;

            ImGui::Image( static_cast<ImTextureID>( static_cast<intptr_t>( m_Textures.front( ) ) ), ImVec2( drawW, drawH ) );

            if( ImGui::IsItemHovered( ) )
            {
                ImGui::SetTooltip( "%s", m_CurrentFilePath.c_str( ) );
            }
        }

        {
            constexpr float k_BtnR = 16.0f;
            constexpr float k_Pad  = 6.0f;

            const ImVec2 center( contentScreenMax.x - k_Pad - k_BtnR,
                                 contentScreenMin.y + k_Pad + k_BtnR );
            const ImVec2 btnPos( center.x - k_BtnR, center.y - k_BtnR );
            const ImVec2 btnSize( k_BtnR * 2.0f, k_BtnR * 2.0f );

            ImGui::SetCursorScreenPos( btnPos );
            const bool pressed = ImGui::InvisibleButton( "##privacy_toggle", btnSize );
            const bool hovered = ImGui::IsItemHovered( );

            if( hovered )
            {
                ImGui::SetTooltip( "%s", m_PrivacyMode ? "Show preview" : "Hide preview (privacy mode)" );
            }

            if( pressed )
            {
                m_PrivacyMode = !m_PrivacyMode;
                OnPrivacyModeChanged( );
                if( m_AppConfig )
                {
                    m_AppConfig->SetValue<bool>( "preview_privacy_mode", m_PrivacyMode );
                    m_AppConfig->Serialise( );
                }
            }

            ImDrawList* buttonDrawList = ImGui::GetWindowDrawList( );
            const ImU32 backgroundColor = m_PrivacyMode
                ? ( hovered ? IM_COL32( 110, 110, 110, 220 ) : IM_COL32( 80, 80, 80, 200 ) )
                : ( hovered ? IM_COL32( 0, 0, 0, 200 ) : IM_COL32( 0, 0, 0, 140 ) );
            const ImU32 iconColor = IM_COL32( 240, 240, 240, 230 );

            buttonDrawList->AddCircleFilled( center, k_BtnR, backgroundColor, 24 );

            const char* glyph = m_PrivacyMode ? kIconEyeHidden : kIconEyeOpen;
            const char* fallback = m_PrivacyMode ? kIconEyeHiddenFallback : kIconEyeOpenFallback;

            const bool useIconFont = ( m_IconFont != nullptr ) && m_IconFont->IsLoaded( );
            if( useIconFont )
            {
                ImGui::PushFont( m_IconFont );
            }

            const char* drawText = useIconFont ? glyph : fallback;
            const ImVec2 size = ImGui::CalcTextSize( drawText );
            // Glyph from Segoe MDL2 at 36pt is taller than our 32px button; nudge
            // it up slightly so it visually centres.
            const ImVec2 textPos(
                center.x - size.x * 0.5f,
                center.y - size.y * 0.5f - 1.0f );
            buttonDrawList->AddText( textPos, iconColor, drawText );

            if( useIconFont )
            {
                ImGui::PopFont( );
            }
        }
    }
    ImGui::End( );

    if( windowOpen && !m_PrivacyMode )
    {
        constexpr float k_Pad = 6.0f;
        constexpr float k_BadgePad = 3.0f;
        ImDrawList* foreground = ImGui::GetForegroundDrawList( );
        const ImU32 textColor = ImGui::GetColorU32( ImGuiCol_Text );
        const ImU32 badgeColor = IM_COL32( 0, 0, 0, 140 );

        auto DrawBadge = [&]( ImVec2 position, const char* text )
        {
            const ImVec2 textSize = ImGui::CalcTextSize( text );
            foreground->AddRectFilled(
                ImVec2( position.x - k_BadgePad, position.y - k_BadgePad ),
                ImVec2( position.x + textSize.x + k_BadgePad, position.y + textSize.y + k_BadgePad ),
                badgeColor,
                3.0f );
            foreground->AddText( position, textColor, text );
            return ImVec2( textSize.x + ( k_BadgePad * 2.0f ), textSize.y + ( k_BadgePad * 2.0f ) );
        };

        const float badgeY = contentScreenMin.y + k_Pad;
        if( !m_CurrentFilePath.empty( ) )
        {
            const ImVec2 nameBadgeSize = DrawBadge( ImVec2( contentScreenMin.x + k_Pad, badgeY ), m_CurrentFileName.c_str( ) );

            std::vector<std::string> metadataBadges;
            const std::string dimensionsLabel = FormatDimensionsLabel( m_Width, m_Height );
            if( !m_CurrentProviderName.empty( ) )
            {
                metadataBadges.push_back( m_CurrentProviderName );
            }
            if( !m_CurrentSubfolderLabel.empty( ) )
            {
                metadataBadges.push_back( m_CurrentSubfolderLabel );
            }
            if( !dimensionsLabel.empty( ) )
            {
                metadataBadges.push_back( dimensionsLabel );
            }
            if( !m_CurrentFileSizeLabel.empty( ) )
            {
                metadataBadges.push_back( m_CurrentFileSizeLabel );
            }

            float badgeRowY = badgeY + nameBadgeSize.y + 4.0f;
            const float badgeX = contentScreenMin.x + k_Pad;

            for( const std::string& badgeText : metadataBadges )
            {
                const ImVec2 badgeSize = DrawBadge( ImVec2( badgeX, badgeRowY ), badgeText.c_str( ) );
                badgeRowY += badgeSize.y + 4.0f;
            }
        }

        if( hasImageRect && IsCurrentGifWarming( ) )
        {
            const float barWidth = ( imageScreenMax.x - imageScreenMin.x ) * 0.3f;
            const float barHeight = 8.0f;
            const float segmentWidth = barWidth * 0.35f;
            const float travelWidth = std::max( barWidth - segmentWidth, 0.0f );
            const float travel = static_cast<float>( std::fabs( std::sin( ImGui::GetTime( ) * 1.5 ) ) );

            const ImVec2 barMin(
                imageScreenMin.x + ( imageScreenMax.x - imageScreenMin.x - barWidth ) * 0.5f,
                imageScreenMin.y + ( imageScreenMax.y - imageScreenMin.y - barHeight ) * 0.5f );
            const ImVec2 barMax( barMin.x + barWidth, barMin.y + barHeight );
            const float segmentX = barMin.x + travelWidth * travel;

            foreground->AddRectFilled( barMin, barMax, IM_COL32( 0, 0, 0, 140 ), 4.0f );
            foreground->AddRectFilled(
                ImVec2( segmentX, barMin.y ),
                ImVec2( segmentX + segmentWidth, barMax.y ),
                IM_COL32( 100, 180, 255, 220 ),
                4.0f );
        }
    }
}

void ImageScraper::MediaPreviewPanel::OnFileDownloaded( const std::string& filepath )
{
    std::lock_guard<std::mutex> lock( m_PathMutex );
    m_LatestPath = filepath;
    m_HasLatestPath = true;
}

void ImageScraper::MediaPreviewPanel::RequestPreview( const std::string& filepath )
{
    if( filepath.empty( ) )
    {
        ClearPreview( );
        return;
    }

    m_CancelDecode = true;
    m_CancelAudioPrepare = true;

    if( m_MediaState == MediaState::VideoPlaying )
    {
        PauseVideoPlayback( );
    }
    else
    {
        StopAudioPlayback( );
    }

    {
        std::lock_guard<std::mutex> lock( m_PathMutex );
        m_LatestPath = filepath;
        m_HasLatestPath = true;
    }

    if( IsGif( filepath ) )
    {
        TouchGifEntry( filepath );
    }

    m_ForceLoad = true;
    m_PlayOnUpload = false;
}

void ImageScraper::MediaPreviewPanel::ClearPreview( )
{
    m_CancelDecode = true;
    m_CancelAudioPrepare = true;
    CancelGifWarmDecode( true );
    m_PendingGifWarmQueue.clear( );

    if( m_DecodeFuture.valid( ) )
    {
        m_DecodeFuture.wait( );
    }
    if( m_AudioPrepareFuture.valid( ) )
    {
        m_AudioPrepareFuture.wait( );
    }

    {
        std::lock_guard<std::mutex> lock( m_DecodedMutex );
        m_PendingDecoded.reset( );
    }
    {
        std::lock_guard<std::mutex> lock( m_PendingAudioMutex );
        m_PendingAudioPlayer.reset( );
    }
    {
        std::lock_guard<std::mutex> lock( m_PendingGifWarmMutex );
        m_PendingGifWarmDecode.reset( );
    }
    {
        std::lock_guard<std::mutex> lock( m_PathMutex );
        m_HasLatestPath = false;
    }

    StopAudioPlayback( );
    FreeTextures( );
    m_VideoPlayer.reset( );
    m_AudioPlayer.reset( );
    m_MediaState = MediaState::None;
    m_CurrentFilePath.clear( );
    ClearMetadataCache( );
    m_LoadingFilePath.clear( );
    m_VideoFrameBuffer.clear( );
    m_VideoFrameIndex = 0;
    m_HasAudio = false;
    m_PlayOnUpload = false;
    m_IsPlaybackClockRunning = false;
    m_PlaybackTimeSeconds = 0.0;
    m_GifPlaybackTimeMs = 0.0;
}

void ImageScraper::MediaPreviewPanel::ReleaseFileIfCurrent( const std::string& filepath )
{
    if( m_CurrentFilePath != filepath && m_LoadingFilePath != filepath )
    {
        return;
    }

    ClearPreview( );
}

void ImageScraper::MediaPreviewPanel::TogglePlayPause( )
{
    if( IsCurrentGifReady( ) )
    {
        if( m_MediaState == MediaState::StaticFrame )
        {
            m_MediaState = MediaState::GifPlaying;
            return;
        }
        if( m_MediaState == MediaState::GifPlaying )
        {
            m_MediaState = MediaState::StaticFrame;
            return;
        }
    }

    if( m_MediaState == MediaState::VideoPlaying )
    {
        PauseVideoPlayback( );
    }
    else if( m_MediaState == MediaState::VideoPaused )
    {
        StartVideoPlayback( );
    }
}

void ImageScraper::MediaPreviewPanel::ToggleMute( )
{
    if( !CanMute( ) )
    {
        return;
    }

    m_IsMuted = !m_IsMuted;

    if( m_AudioPlayer && m_AudioPlayer->IsPlaying( ) )
    {
        m_AudioPlayer->SetVolume( m_IsMuted ? 0.0f : m_Volume );
    }
}

float ImageScraper::MediaPreviewPanel::GetVolume( ) const
{
    return m_Volume;
}

void ImageScraper::MediaPreviewPanel::SetVolume( float volume, bool persist )
{
    volume = std::clamp( volume, 0.0f, 1.0f );
    if( volume == m_Volume && !persist )
    {
        return;
    }

    m_Volume = volume;

    if( m_AudioPlayer && m_AudioPlayer->IsPlaying( ) )
    {
        m_AudioPlayer->SetVolume( m_IsMuted ? 0.0f : m_Volume );
    }

    if( persist && m_AppConfig )
    {
        m_AppConfig->SetValue<float>( "preview_volume", m_Volume );
        m_AppConfig->Serialise( );
    }
}

bool ImageScraper::MediaPreviewPanel::IsPlaying( ) const
{
    return m_MediaState == MediaState::GifPlaying || m_MediaState == MediaState::VideoPlaying;
}

bool ImageScraper::MediaPreviewPanel::IsMuted( ) const
{
    return m_IsMuted;
}

bool ImageScraper::MediaPreviewPanel::CanPlayPause( ) const
{
    if( m_PrivacyMode )
    {
        return false;
    }

    if( HasCurrentGifPlayback( ) )
    {
        return true;
    }

    return m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused;
}

bool ImageScraper::MediaPreviewPanel::CanMute( ) const
{
    return m_HasAudio &&
           ( m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused );
}

void ImageScraper::MediaPreviewPanel::KickDecodeIfNeeded( )
{
    if( m_IsDecoding ||
        ( ( m_MediaState == MediaState::GifPlaying || m_MediaState == MediaState::VideoPlaying ) && !m_ForceLoad ) )
    {
        return;
    }
    m_ForceLoad = false;

    std::string filepath;
    {
        std::lock_guard<std::mutex> lock( m_PathMutex );
        if( !m_HasLatestPath )
        {
            return;
        }
        filepath = m_LatestPath;
        m_HasLatestPath = false;
    }

    m_LoadingFilePath = filepath;
    m_CancelDecode = false;
    m_IsDecoding = true;

    m_DecodeFuture = std::async( std::launch::async, [this, filepath]( )
    {
        auto decoded = DecodeFile( filepath );

        if( !m_CancelDecode )
        {
            std::lock_guard<std::mutex> lock( m_DecodedMutex );
            m_PendingDecoded = std::move( decoded );
        }

        m_IsDecoding = false;
    } );
}

void ImageScraper::MediaPreviewPanel::KickAudioPrepareIfNeeded( )
{
    if( !m_HasAudio || m_CurrentFilePath.empty( ) || IsGif( m_CurrentFilePath ) || m_IsPreparingAudio || m_AudioPlayer )
    {
        return;
    }

    m_CancelAudioPrepare = false;
    m_IsPreparingAudio = true;

    const std::string filepath = m_CurrentFilePath;

    m_AudioPrepareFuture = std::async( std::launch::async, [this, filepath]( )
    {
        auto audioPlayer = std::make_unique<MediaAudioPlayer>( );
        if( audioPlayer->Open( filepath ) && !m_CancelAudioPrepare )
        {
            std::lock_guard<std::mutex> lock( m_PendingAudioMutex );
            m_PendingAudioPlayer = std::move( audioPlayer );
        }

        m_IsPreparingAudio = false;
    } );
}

void ImageScraper::MediaPreviewPanel::ApplyPreparedAudio( )
{
    std::unique_ptr<MediaAudioPlayer> audioPlayer;
    {
        std::lock_guard<std::mutex> lock( m_PendingAudioMutex );
        audioPlayer = std::move( m_PendingAudioPlayer );
    }

    if( !audioPlayer )
    {
        return;
    }

    if( audioPlayer->GetFilePath( ) != m_CurrentFilePath )
    {
        return;
    }

    m_AudioPlayer = std::move( audioPlayer );

    if( m_MediaState == MediaState::VideoPlaying )
    {
        StartAudioPlaybackFromCurrentTime( );
    }
}

void ImageScraper::MediaPreviewPanel::ApplyPreparedGifWarmDecode( )
{
    std::unique_ptr<PendingGifWarmDecode> pending;
    {
        std::lock_guard<std::mutex> lock( m_PendingGifWarmMutex );
        pending = std::move( m_PendingGifWarmDecode );
    }

    if( !pending )
    {
        return;
    }

    GifRetainedEntry* entry = FindGifEntry( pending->m_FilePath );
    if( !entry )
    {
        m_ActiveGifWarmFilePath.clear( );
        return;
    }

    if( pending->m_Result.m_Status == GifPlaybackCache::DecodeStatus::Ready )
    {
        const uint64_t gifBytes = pending->m_Result.m_Runtime.GetTotalFrameBytes( );
        if( gifBytes > k_MaxSingleGifBytes )
        {
            WarningLog(
                "[%s] Skipping retained GIF warm-up for %s because decoded size %llu bytes exceeds limit %llu bytes",
                __FUNCTION__,
                pending->m_FilePath.c_str( ),
                static_cast<unsigned long long>( gifBytes ),
                static_cast<unsigned long long>( k_MaxSingleGifBytes ) );
            entry->m_Runtime.reset( );
            entry->m_State = GifWarmState::Failed;
            m_ActiveGifWarmFilePath.clear( );
            return;
        }

        entry->m_Runtime = std::make_unique<GifPlaybackCache::Runtime>( std::move( pending->m_Result.m_Runtime ) );
        entry->m_State = GifWarmState::Ready;
        InfoLog(
            "[%s] Warmed GIF %s into %llu bytes across %zu frames",
            __FUNCTION__,
            pending->m_FilePath.c_str( ),
            static_cast<unsigned long long>( gifBytes ),
            entry->m_Runtime->m_Frames.size( ) );
        TrimGifRetention( );

        if( m_CurrentFilePath == pending->m_FilePath )
        {
            SyncCurrentGifSelectionState( );
        }
    }
    else if( pending->m_Result.m_Status == GifPlaybackCache::DecodeStatus::Failed )
    {
        entry->m_Runtime.reset( );
        entry->m_State = GifWarmState::Failed;
    }
    else
    {
        if( entry->m_State == GifWarmState::Warming )
        {
            entry->m_State = GifWarmState::None;
        }
    }

    m_ActiveGifWarmFilePath.clear( );
}

void ImageScraper::MediaPreviewPanel::QueueGifWarmDecode( const std::string& filepath )
{
    if( filepath.empty( ) )
    {
        return;
    }

    GifRetainedEntry* entry = FindGifEntry( filepath );
    if( !entry )
    {
        return;
    }

    if( entry->m_State == GifWarmState::Ready || entry->m_State == GifWarmState::Warming || entry->m_State == GifWarmState::Queued )
    {
        return;
    }

    entry->m_State = GifWarmState::Queued;
    RemovePendingGifWarmDecode( filepath );
    m_PendingGifWarmQueue.push_back( filepath );
}

void ImageScraper::MediaPreviewPanel::StartNextGifWarmDecode( )
{
    if( m_PrivacyMode || m_IsGifWarmDecoding || m_PendingGifWarmQueue.empty( ) )
    {
        return;
    }

    const std::string filepath = m_PendingGifWarmQueue.back( );
    m_PendingGifWarmQueue.pop_back( );

    GifRetainedEntry* entry = FindGifEntry( filepath );
    if( !entry )
    {
        WarningLog( "[%s] Pending GIF warm-up queue lost entry for: %s", __FUNCTION__, filepath.c_str( ) );
        return;
    }
    if( entry->m_State == GifWarmState::Ready )
    {
        return;
    }

    entry->m_State = GifWarmState::Warming;
    m_ActiveGifWarmFilePath = filepath;
    m_CancelGifWarmDecodeRequested = false;
    m_IsGifWarmDecoding = true;

    m_GifWarmDecodeFuture = std::async( std::launch::async, [this, filepath]( )
    {
        GifPlaybackCache::DecodeResult result = GifPlaybackCache::DecodeGifFile(
            filepath,
            [this]( )
            {
                return m_CancelGifWarmDecodeRequested.load( );
            } );

        std::lock_guard<std::mutex> lock( m_PendingGifWarmMutex );
        m_PendingGifWarmDecode = std::make_unique<PendingGifWarmDecode>( );
        m_PendingGifWarmDecode->m_FilePath = filepath;
        m_PendingGifWarmDecode->m_Result = std::move( result );
        m_IsGifWarmDecoding = false;
    } );
}

void ImageScraper::MediaPreviewPanel::CancelGifWarmDecode( bool waitForCompletion )
{
    m_CancelGifWarmDecodeRequested = true;

    if( waitForCompletion && m_GifWarmDecodeFuture.valid( ) )
    {
        m_GifWarmDecodeFuture.wait( );
    }
}

void ImageScraper::MediaPreviewPanel::EnsureGifEntry( const std::string& filepath )
{
    GifRetainedEntry* entry = FindGifEntry( filepath );
    if( !entry )
    {
        m_GifEntries.push_back( GifRetainedEntry{ } );
        entry = &m_GifEntries.back( );
        entry->m_FilePath = filepath;
    }
}

void ImageScraper::MediaPreviewPanel::TouchGifEntry( const std::string& filepath )
{
    EnsureGifEntry( filepath );
    GifRetainedEntry* entry = FindGifEntry( filepath );
    entry->m_LastTouched = ++m_GifTouchCounter;
}

void ImageScraper::MediaPreviewPanel::TrimGifRetention( )
{
    auto NeedsEviction = [this]( )
    {
        size_t readyCount = 0;
        for( const GifRetainedEntry& entry : m_GifEntries )
        {
            if( entry.m_State == GifWarmState::Ready && entry.m_Runtime )
            {
                ++readyCount;
            }
        }

        return readyCount > k_MaxRetainedReadyGifs || GetTotalReadyGifBytes( ) > k_MaxRetainedGifBytes;
    };

    while( NeedsEviction( ) )
    {
        auto victim = m_GifEntries.end( );
        for( auto it = m_GifEntries.begin( ); it != m_GifEntries.end( ); ++it )
        {
            if( it->m_State != GifWarmState::Ready || !it->m_Runtime )
            {
                continue;
            }
            if( it->m_FilePath == m_CurrentFilePath )
            {
                continue;
            }
            if( victim == m_GifEntries.end( ) || it->m_LastTouched < victim->m_LastTouched )
            {
                victim = it;
            }
        }

        if( victim == m_GifEntries.end( ) )
        {
            break;
        }

        InfoLog(
            "[%s] Evicting warmed GIF %s to enforce retention limits",
            __FUNCTION__,
            victim->m_FilePath.c_str( ) );
        m_GifEntries.erase( victim );
    }
}

uint64_t ImageScraper::MediaPreviewPanel::GetTotalReadyGifBytes( ) const
{
    uint64_t totalBytes = 0;
    for( const GifRetainedEntry& entry : m_GifEntries )
    {
        if( entry.m_State == GifWarmState::Ready && entry.m_Runtime )
        {
            totalBytes += entry.m_Runtime->GetTotalFrameBytes( );
        }
    }

    return totalBytes;
}

void ImageScraper::MediaPreviewPanel::RemovePendingGifWarmDecode( const std::string& filepath )
{
    m_PendingGifWarmQueue.erase(
        std::remove( m_PendingGifWarmQueue.begin( ), m_PendingGifWarmQueue.end( ), filepath ),
        m_PendingGifWarmQueue.end( ) );
}

void ImageScraper::MediaPreviewPanel::SyncCurrentGifSelectionState( )
{
    GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
    if( !runtime )
    {
        if( IsGif( m_CurrentFilePath ) )
        {
            m_MediaState = MediaState::StaticFrame;
        }
        return;
    }

    m_GifPlaybackTimeMs = 0.0;
    m_CurrentFrame = 0;
    UploadCurrentGifFrame( 0 );
    m_MediaState = MediaState::StaticFrame;
}

void ImageScraper::MediaPreviewPanel::AdvanceGifFrame( )
{
    GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
    if( !runtime || runtime->m_TotalDurationMs <= 0 || runtime->m_Frames.empty( ) || m_IsScrubbing )
    {
        return;
    }

    m_GifPlaybackTimeMs += ImGui::GetIO( ).DeltaTime * 1000.0;
    while( m_GifPlaybackTimeMs >= static_cast<double>( runtime->m_TotalDurationMs ) )
    {
        m_GifPlaybackTimeMs -= static_cast<double>( runtime->m_TotalDurationMs );
    }

    const size_t frameIndex = runtime->GetFrameIndexForTimeMs( static_cast<int64_t>( m_GifPlaybackTimeMs ) );
    UploadCurrentGifFrame( frameIndex );
}

void ImageScraper::MediaPreviewPanel::UploadCurrentGifFrame( size_t frameIndex )
{
    GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
    if( !runtime || runtime->m_Frames.empty( ) || m_Textures.empty( ) )
    {
        return;
    }

    frameIndex = std::min( frameIndex, runtime->m_Frames.size( ) - 1 );
    const GifPlaybackCache::Frame& frame = runtime->m_Frames[ frameIndex ];

    glBindTexture( GL_TEXTURE_2D, m_Textures.front( ) );
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        runtime->m_Width,
        runtime->m_Height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        frame.m_RgbaPixels.data( ) );
    glBindTexture( GL_TEXTURE_2D, 0 );

    m_CurrentFrame = static_cast<int>( frameIndex );
}

void ImageScraper::MediaPreviewPanel::SetCurrentGifTimeMs( int64_t timeMs )
{
    GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
    if( !runtime || runtime->m_TotalDurationMs <= 0 )
    {
        return;
    }

    timeMs = std::clamp<int64_t>( timeMs, 0, runtime->m_TotalDurationMs - 1 );
    m_GifPlaybackTimeMs = static_cast<double>( timeMs );
    const size_t frameIndex = runtime->GetFrameIndexForTimeMs( timeMs );
    UploadCurrentGifFrame( frameIndex );
}

bool ImageScraper::MediaPreviewPanel::IsCurrentGifReady( ) const
{
    const GifRetainedEntry* entry = FindGifEntry( m_CurrentFilePath );
    return entry && entry->m_State == GifWarmState::Ready && entry->m_Runtime && entry->m_Runtime->HasMultipleFrames( );
}

bool ImageScraper::MediaPreviewPanel::IsCurrentGifWarming( ) const
{
    if( m_PrivacyMode || !IsGif( m_CurrentFilePath ) )
    {
        return false;
    }

    const GifRetainedEntry* entry = FindGifEntry( m_CurrentFilePath );
    if( !entry )
    {
        return false;
    }

    return entry->m_State == GifWarmState::Queued || entry->m_State == GifWarmState::Warming;
}

bool ImageScraper::MediaPreviewPanel::HasCurrentGifPlayback( ) const
{
    return !m_PrivacyMode &&
           IsCurrentGifReady( ) &&
           ( m_MediaState == MediaState::StaticFrame || m_MediaState == MediaState::GifPlaying );
}

ImageScraper::MediaPreviewPanel::GifRetainedEntry* ImageScraper::MediaPreviewPanel::FindGifEntry( const std::string& filepath )
{
    const MediaPreviewPanel* self = this;
    return const_cast<GifRetainedEntry*>( self->FindGifEntry( filepath ) );
}

const ImageScraper::MediaPreviewPanel::GifRetainedEntry* ImageScraper::MediaPreviewPanel::FindGifEntry( const std::string& filepath ) const
{
    const auto it = std::find_if(
        m_GifEntries.begin( ),
        m_GifEntries.end( ),
        [&]( const GifRetainedEntry& entry )
        {
            return entry.m_FilePath == filepath;
        } );

    return it == m_GifEntries.end( ) ? nullptr : &*it;
}

ImageScraper::GifPlaybackCache::Runtime* ImageScraper::MediaPreviewPanel::GetCurrentGifRuntime( )
{
    const MediaPreviewPanel* self = this;
    return const_cast<GifPlaybackCache::Runtime*>( self->GetCurrentGifRuntime( ) );
}

const ImageScraper::GifPlaybackCache::Runtime* ImageScraper::MediaPreviewPanel::GetCurrentGifRuntime( ) const
{
    const GifRetainedEntry* entry = FindGifEntry( m_CurrentFilePath );
    if( !entry || entry->m_State != GifWarmState::Ready || !entry->m_Runtime )
    {
        return nullptr;
    }

    return entry->m_Runtime.get( );
}

void ImageScraper::MediaPreviewPanel::OnPrivacyModeChanged( )
{
    m_PlayOnUpload = false;
    ResetPlaybackToStartPaused( );

    if( m_PrivacyMode )
    {
        CancelGifWarmDecode( false );
        m_PendingGifWarmQueue.clear( );
        if( GifRetainedEntry* entry = FindGifEntry( m_CurrentFilePath ) )
        {
            if( entry->m_State == GifWarmState::Queued || entry->m_State == GifWarmState::Warming )
            {
                entry->m_State = GifWarmState::None;
            }
        }
        return;
    }

    if( IsGif( m_CurrentFilePath ) && !IsCurrentGifReady( ) )
    {
        QueueGifWarmDecode( m_CurrentFilePath );
    }
}

void ImageScraper::MediaPreviewPanel::ResetPlaybackToStartPaused( )
{
    StopAudioPlayback( );
    m_IsPlaybackClockRunning = false;
    m_PlaybackTimeSeconds = 0.0;
    m_CurrentFrame = 0;
    m_GifPlaybackTimeMs = 0.0;

    if( IsCurrentGifReady( ) )
    {
        UploadCurrentGifFrame( 0 );
    }

    if( m_MediaState == MediaState::GifPlaying )
    {
        m_MediaState = MediaState::StaticFrame;
    }

    if( m_MediaState != MediaState::VideoPlaying && m_MediaState != MediaState::VideoPaused )
    {
        return;
    }

    m_MediaState = MediaState::VideoPaused;
    m_VideoFrameIndex = 0;

    if( !m_VideoPlayer || !m_VideoPlayer->IsOpen( ) || m_Textures.empty( ) )
    {
        return;
    }

    m_VideoPlayer->SeekToStart( );
    if( m_VideoPlayer->DecodeNextFrame( m_VideoFrameBuffer ) )
    {
        UploadCurrentVideoFrame( );
    }
}

void ImageScraper::MediaPreviewPanel::StartVideoPlayback( )
{
    if( m_MediaState != MediaState::VideoPaused )
    {
        return;
    }

    m_PlaybackStartedAt = std::chrono::steady_clock::now( );
    m_IsPlaybackClockRunning = true;
    m_MediaState = MediaState::VideoPlaying;

    KickAudioPrepareIfNeeded( );
    StartAudioPlaybackFromCurrentTime( );
}

void ImageScraper::MediaPreviewPanel::PauseVideoPlayback( )
{
    if( m_MediaState != MediaState::VideoPlaying )
    {
        return;
    }

    m_PlaybackTimeSeconds = GetPlaybackTimeSeconds( );
    m_IsPlaybackClockRunning = false;
    m_MediaState = MediaState::VideoPaused;
    StopAudioPlayback( );
}

void ImageScraper::MediaPreviewPanel::RestartVideoPlayback( )
{
    if( !m_VideoPlayer || !m_VideoPlayer->IsOpen( ) || m_Textures.empty( ) )
    {
        return;
    }

    m_VideoPlayer->SeekToStart( );
    if( !m_VideoPlayer->DecodeNextFrame( m_VideoFrameBuffer ) )
    {
        PauseVideoPlayback( );
        return;
    }

    UploadCurrentVideoFrame( );
    m_VideoFrameIndex = 0;
    m_PlaybackTimeSeconds = 0.0;

    if( m_MediaState == MediaState::VideoPlaying )
    {
        m_PlaybackStartedAt = std::chrono::steady_clock::now( );
        StartAudioPlaybackFromCurrentTime( );
    }
}

void ImageScraper::MediaPreviewPanel::StartAudioPlaybackFromCurrentTime( )
{
    if( m_MediaState != MediaState::VideoPlaying || !m_AudioPlayer )
    {
        return;
    }

    m_AudioPlayer->StartFrom( GetPlaybackTimeSeconds( ), m_IsMuted ? 0.0f : m_Volume );
}

void ImageScraper::MediaPreviewPanel::StopAudioPlayback( )
{
    if( m_AudioPlayer )
    {
        m_AudioPlayer->Stop( );
    }
}

void ImageScraper::MediaPreviewPanel::UploadCurrentVideoFrame( ) const
{
    if( m_Textures.empty( ) )
    {
        return;
    }

    glBindTexture( GL_TEXTURE_2D, m_Textures.front( ) );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, m_VideoFrameBuffer.data( ) );
    glBindTexture( GL_TEXTURE_2D, 0 );
}

double ImageScraper::MediaPreviewPanel::GetPlaybackTimeSeconds( ) const
{
    double playbackTime = m_PlaybackTimeSeconds;
    if( m_IsPlaybackClockRunning )
    {
        playbackTime += std::chrono::duration<double>(
            std::chrono::steady_clock::now( ) - m_PlaybackStartedAt ).count( );
    }

    return std::max( playbackTime, 0.0 );
}

bool ImageScraper::MediaPreviewPanel::CanScrub( ) const
{
    if( m_PrivacyMode )
    {
        return false;
    }

    if( HasCurrentGifPlayback( ) )
    {
        return true;
    }

    if( ( m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused ) &&
        m_VideoPlayer && m_VideoPlayer->IsOpen( ) && m_VideoPlayer->GetDuration( ) > 0.0 )
    {
        return true;
    }

    return false;
}

float ImageScraper::MediaPreviewPanel::GetProgress( ) const
{
    if( HasCurrentGifPlayback( ) )
    {
        const GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
        if( runtime && runtime->m_TotalDurationMs > 0 )
        {
            const double clampedTime = std::clamp( m_GifPlaybackTimeMs, 0.0, static_cast<double>( runtime->m_TotalDurationMs - 1 ) );
            return static_cast<float>( clampedTime / static_cast<double>( runtime->m_TotalDurationMs ) );
        }
    }

    if( ( m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused ) && m_VideoPlayer )
    {
        const double duration = m_VideoPlayer->GetDuration( );
        if( duration > 0.0 )
        {
            const double currentTime = std::clamp( GetPlaybackTimeSeconds( ), 0.0, duration );
            return static_cast<float>( currentTime / duration );
        }
    }

    return -1.0f;
}

std::string ImageScraper::MediaPreviewPanel::GetProgressLabel( ) const
{
    auto FormatSeconds = []( double seconds )
    {
        if( seconds < 0.0 )
        {
            seconds = 0.0;
        }

        const int total = static_cast<int>( seconds );
        const int minutes = total / 60;
        const int wholeSeconds = total % 60;
        std::ostringstream stream;
        stream << std::setw( 2 ) << std::setfill( '0' ) << minutes << ":"
               << std::setw( 2 ) << std::setfill( '0' ) << wholeSeconds;
        return stream.str( );
    };

    if( HasCurrentGifPlayback( ) )
    {
        const GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
        if( runtime )
        {
            std::ostringstream stream;
            stream << "frame " << ( m_CurrentFrame + 1 ) << " / " << runtime->m_Frames.size( );
            return stream.str( );
        }
    }

    if( ( m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused ) && m_VideoPlayer )
    {
        const double duration = m_VideoPlayer->GetDuration( );
        if( duration > 0.0 )
        {
            const double currentTime = std::clamp( GetPlaybackTimeSeconds( ), 0.0, duration );
            return FormatSeconds( currentTime ) + " / " + FormatSeconds( duration );
        }
    }

    return { };
}

void ImageScraper::MediaPreviewPanel::BeginScrub( )
{
    if( !CanScrub( ) || m_IsScrubbing )
    {
        return;
    }

    m_WasPlayingBeforeScrub = ( m_MediaState == MediaState::GifPlaying || m_MediaState == MediaState::VideoPlaying );

    if( m_MediaState == MediaState::GifPlaying )
    {
        m_MediaState = MediaState::StaticFrame;
    }
    else if( m_MediaState == MediaState::VideoPlaying )
    {
        PauseVideoPlayback( );
    }

    m_IsScrubbing = true;
    m_LastScrubSeekAt = std::chrono::steady_clock::time_point{ };
}

void ImageScraper::MediaPreviewPanel::UpdateScrub( float normalized )
{
    if( !m_IsScrubbing )
    {
        return;
    }

    normalized = std::clamp( normalized, 0.0f, 1.0f );

    if( HasCurrentGifPlayback( ) )
    {
        GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
        if( runtime )
        {
            SetCurrentGifTimeMs( runtime->GetTimeMsForNormalized( normalized ) );
        }
        return;
    }

    if( ( m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused ) &&
        m_VideoPlayer && m_VideoPlayer->IsOpen( ) )
    {
        const double duration = m_VideoPlayer->GetDuration( );
        if( duration <= 0.0 )
        {
            return;
        }

        const double targetSeconds = static_cast<double>( normalized ) * duration;
        m_PlaybackTimeSeconds = targetSeconds;

        const auto now = std::chrono::steady_clock::now( );
        constexpr auto k_MinSeekInterval = std::chrono::milliseconds( 80 );
        if( m_LastScrubSeekAt != std::chrono::steady_clock::time_point{ } &&
            now - m_LastScrubSeekAt < k_MinSeekInterval )
        {
            return;
        }
        m_LastScrubSeekAt = now;

        if( !m_VideoPlayer->SeekToKeyframe( targetSeconds ) )
        {
            return;
        }
        if( m_VideoPlayer->DecodeNextFrame( m_VideoFrameBuffer ) )
        {
            UploadCurrentVideoFrame( );
        }

        const double fps = m_VideoPlayer->GetFPS( );
        if( fps > 0.0 )
        {
            m_VideoFrameIndex = static_cast<int>( targetSeconds * fps );
        }
    }
}

void ImageScraper::MediaPreviewPanel::EndScrub( float normalized )
{
    if( !m_IsScrubbing )
    {
        return;
    }

    normalized = std::clamp( normalized, 0.0f, 1.0f );

    if( HasCurrentGifPlayback( ) )
    {
        GifPlaybackCache::Runtime* runtime = GetCurrentGifRuntime( );
        if( runtime )
        {
            SetCurrentGifTimeMs( runtime->GetTimeMsForNormalized( normalized ) );
        }

        m_IsScrubbing = false;
        if( m_WasPlayingBeforeScrub )
        {
            m_MediaState = MediaState::GifPlaying;
        }
        return;
    }

    if( ( m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused ) &&
        m_VideoPlayer && m_VideoPlayer->IsOpen( ) )
    {
        const double duration = m_VideoPlayer->GetDuration( );
        if( duration > 0.0 )
        {
            const double targetSeconds = static_cast<double>( normalized ) * duration;

            if( m_VideoPlayer->SeekToTimeExact( targetSeconds, m_VideoFrameBuffer ) )
            {
                UploadCurrentVideoFrame( );
            }
            m_PlaybackTimeSeconds = targetSeconds;

            const double fps = m_VideoPlayer->GetFPS( );
            if( fps > 0.0 )
            {
                m_VideoFrameIndex = static_cast<int>( targetSeconds * fps );
            }
        }

        m_IsScrubbing = false;
        if( m_WasPlayingBeforeScrub && m_MediaState == MediaState::VideoPaused )
        {
            StartVideoPlayback( );
        }
        return;
    }

    m_IsScrubbing = false;
}

void ImageScraper::MediaPreviewPanel::AdvanceVideoFrame( )
{
    if( !m_VideoPlayer || !m_VideoPlayer->IsOpen( ) || m_Textures.empty( ) )
    {
        return;
    }

    double playbackTime = GetPlaybackTimeSeconds( );
    const double duration = m_VideoPlayer->GetDuration( );
    if( duration > 0.0 && playbackTime >= duration )
    {
        RestartVideoPlayback( );
        playbackTime = GetPlaybackTimeSeconds( );
    }

    const double fps = m_VideoPlayer->GetFPS( );
    if( fps <= 0.0 )
    {
        return;
    }

    while( playbackTime >= static_cast<double>( m_VideoFrameIndex + 1 ) / fps )
    {
        if( !m_VideoPlayer->DecodeNextFrame( m_VideoFrameBuffer ) )
        {
            RestartVideoPlayback( );
            return;
        }

        ++m_VideoFrameIndex;
        UploadCurrentVideoFrame( );
    }
}

void ImageScraper::MediaPreviewPanel::UploadDecoded( DecodedMedia&& decoded )
{
    StopAudioPlayback( );
    FreeTextures( );
    m_VideoPlayer.reset( );
    m_AudioPlayer.reset( );
    m_HasAudio = false;
    m_IsPlaybackClockRunning = false;
    m_PlaybackTimeSeconds = 0.0;
    m_GifPlaybackTimeMs = 0.0;
    m_CurrentFrame = 0;

    if( decoded.m_PixelData.empty( ) )
    {
        m_MediaState = MediaState::None;
        m_PlayOnUpload = false;
        return;
    }

    m_CurrentFilePath = decoded.m_FilePath;
    RefreshMetadataCache( );
    m_Width = decoded.m_Width;
    m_Height = decoded.m_Height;

    GLuint textureId = 0;
    glGenTextures( 1, &textureId );
    glBindTexture( GL_TEXTURE_2D, textureId );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        decoded.m_Width,
        decoded.m_Height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        decoded.m_PixelData.data( ) );
    glBindTexture( GL_TEXTURE_2D, 0 );
    m_Textures.push_back( textureId );

    if( decoded.m_IsVideo )
    {
        m_HasAudio = decoded.m_HasAudio;
        m_VideoFrameBuffer = std::move( decoded.m_PixelData );
        m_VideoPlayer = std::move( decoded.m_VideoPlayer );
        m_VideoFrameIndex = 0;
        m_MediaState = MediaState::VideoPaused;

        KickAudioPrepareIfNeeded( );
        const bool shouldPlayOnUpload = m_PlayOnUpload && !m_PrivacyMode;
        m_PlayOnUpload = false;
        if( shouldPlayOnUpload )
        {
            StartVideoPlayback( );
        }
        return;
    }

    m_PlayOnUpload = false;
    m_MediaState = MediaState::StaticFrame;

    if( IsGif( decoded.m_FilePath ) )
    {
        EnsureGifEntry( decoded.m_FilePath );
        if( !m_PrivacyMode && !IsCurrentGifReady( ) )
        {
            QueueGifWarmDecode( decoded.m_FilePath );
        }
        SyncCurrentGifSelectionState( );
    }
}

void ImageScraper::MediaPreviewPanel::FreeTextures( )
{
    if( !m_Textures.empty( ) )
    {
        glDeleteTextures( static_cast<GLsizei>( m_Textures.size( ) ), m_Textures.data( ) );
        m_Textures.clear( );
    }

    m_Width = 0;
    m_Height = 0;
    m_CurrentFrame = 0;
    m_GifPlaybackTimeMs = 0.0;
}

std::unique_ptr<ImageScraper::MediaPreviewPanel::DecodedMedia>
ImageScraper::MediaPreviewPanel::DecodeFile( const std::string& filepath )
{
    if( IsVideo( filepath ) )
    {
        return DecodeVideoFile( filepath );
    }

    return DecodeStillImageFile( filepath );
}

std::unique_ptr<ImageScraper::MediaPreviewPanel::DecodedMedia>
ImageScraper::MediaPreviewPanel::DecodeStillImageFile( const std::string& filepath )
{
    auto decoded = std::make_unique<DecodedMedia>( );
    decoded->m_FilePath = filepath;

    if( !VideoPlayer::DecodeFirstFrameFile( filepath, decoded->m_PixelData, decoded->m_Width, decoded->m_Height ) )
    {
        return nullptr;
    }

    return decoded;
}

std::unique_ptr<ImageScraper::MediaPreviewPanel::DecodedMedia>
ImageScraper::MediaPreviewPanel::DecodeVideoFile( const std::string& filepath )
{
    auto decoded = std::make_unique<DecodedMedia>( );
    decoded->m_FilePath = filepath;
    decoded->m_IsVideo = true;
    decoded->m_VideoPlayer = std::make_unique<VideoPlayer>( );

    if( !decoded->m_VideoPlayer->Open( filepath ) )
    {
        return nullptr;
    }

    decoded->m_HasAudio = decoded->m_VideoPlayer->HasAudio( );
    decoded->m_Width = decoded->m_VideoPlayer->GetWidth( );
    decoded->m_Height = decoded->m_VideoPlayer->GetHeight( );

    if( !decoded->m_VideoPlayer->DecodeNextFrame( decoded->m_PixelData ) )
    {
        return nullptr;
    }

    const size_t pixelBytes = static_cast<size_t>( decoded->m_Width ) * static_cast<size_t>( decoded->m_Height ) * 4;
    if( decoded->m_PixelData.size( ) > pixelBytes )
    {
        decoded->m_PixelData.resize( pixelBytes );
    }

    return decoded;
}

bool ImageScraper::MediaPreviewPanel::IsGif( const std::string& filepath )
{
    const auto dot = filepath.rfind( '.' );
    if( dot == std::string::npos )
    {
        return false;
    }

    const std::string ext = StringUtils::ToLower( filepath.substr( dot + 1 ) );
    return ext == "gif";
}

bool ImageScraper::MediaPreviewPanel::IsVideo( const std::string& filepath )
{
    const auto dot = filepath.rfind( '.' );
    if( dot == std::string::npos )
    {
        return false;
    }

    const std::string ext = StringUtils::ToLower( filepath.substr( dot + 1 ) );
    return ext == "mp4" || ext == "webm" || ext == "mov" || ext == "mkv" || ext == "avi";
}

std::string ImageScraper::MediaPreviewPanel::FormatFileSize( const std::string& filepath )
{
    std::error_code error;
    const auto bytes = std::filesystem::file_size( filepath, error );
    if( error )
    {
        return { };
    }

    std::ostringstream stream;
    if( bytes < 1024 )
    {
        stream << bytes << " B";
    }
    else if( bytes < 1024 * 1024 )
    {
        stream << std::fixed << std::setprecision( 1 ) << ( bytes / 1024.0 ) << " KB";
    }
    else
    {
        stream << std::fixed << std::setprecision( 1 ) << ( bytes / ( 1024.0 * 1024.0 ) ) << " MB";
    }

    return stream.str( );
}

void ImageScraper::MediaPreviewPanel::SetDownloadRoot( const std::filesystem::path& downloadRoot )
{
    m_DownloadRoot = downloadRoot;
    if( !m_CurrentFilePath.empty( ) )
    {
        RefreshMetadataCache( );
    }
}

void ImageScraper::MediaPreviewPanel::RefreshMetadataCache( )
{
    if( m_CurrentFilePath.empty( ) )
    {
        ClearMetadataCache( );
        return;
    }

    m_CurrentFileName = std::filesystem::path( m_CurrentFilePath ).filename( ).string( );
    m_CurrentProviderName = DownloadHelpers::GetProviderName( m_CurrentFilePath, m_DownloadRoot );
    m_CurrentSubfolderLabel = DownloadHelpers::GetSubfolderLabel( m_CurrentFilePath, m_DownloadRoot );
    m_CurrentFileSizeLabel = FormatFileSize( m_CurrentFilePath );
}

void ImageScraper::MediaPreviewPanel::ClearMetadataCache( )
{
    m_CurrentFileName.clear( );
    m_CurrentProviderName.clear( );
    m_CurrentSubfolderLabel.clear( );
    m_CurrentFileSizeLabel.clear( );
}

void ImageScraper::MediaPreviewPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;

    if( !m_AppConfig )
    {
        return;
    }

    bool privacy = false;
    if( m_AppConfig->GetValue<bool>( "preview_privacy_mode", privacy ) )
    {
        m_PrivacyMode = privacy;
    }

    float volume = 1.0f;
    if( m_AppConfig->GetValue<float>( "preview_volume", volume ) )
    {
        m_Volume = std::clamp( volume, 0.0f, 1.0f );
    }
}

void ImageScraper::MediaPreviewPanel::SetIconFont( ImFont* iconFont )
{
    m_IconFont = iconFont;
}
