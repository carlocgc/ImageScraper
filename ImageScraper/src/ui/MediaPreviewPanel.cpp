#define NOMINMAX
#include "ui/MediaPreviewPanel.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "stb/stb_image.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>

ImageScraper::MediaPreviewPanel::~MediaPreviewPanel( )
{
    m_CancelAudioPrepare = true;
    m_CancelDecode       = true;

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
    // Upload any newly decoded preview media (GPU buffer copies - main thread only).
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

    ApplyPreparedAudio( );

    // Kick a fast first-frame decode when a new file arrives.
    KickDecodeIfNeeded( );

    if( m_MediaState == MediaState::VideoPlaying )
    {
        AdvanceVideoFrame( );
    }

    ImVec2 contentScreenMin{ 0.0f, 0.0f };
    ImVec2 contentScreenMax{ 0.0f, 0.0f };
    bool   windowOpen = false;
    ImGui::SetNextWindowSize( ImVec2( 480, 480 ), ImGuiCond_FirstUseEver );

    if( ImGui::Begin( "Media Preview", nullptr ) )
    {
        windowOpen = true;

        const ImVec2 winPos = ImGui::GetWindowPos( );
        contentScreenMin = ImVec2( winPos.x + ImGui::GetWindowContentRegionMin( ).x,
                                   winPos.y + ImGui::GetWindowContentRegionMin( ).y );
        contentScreenMax = ImVec2( winPos.x + ImGui::GetWindowContentRegionMax( ).x,
                                   winPos.y + ImGui::GetWindowContentRegionMax( ).y );

        if( m_Textures.empty( ) )
        {
            ImGui::TextDisabled( m_IsDecoding ? "Loading preview..." : "No media loaded yet" );
        }
        else
        {
            if( m_MediaState == MediaState::GifPlaying && m_Textures.size( ) > 1 )
            {
                m_FrameAccumMs += ImGui::GetIO( ).DeltaTime * 1000.0f;

                while( true )
                {
                    const int delayMs = m_FrameDelaysMs.empty( ) ? 100 : m_FrameDelaysMs[ m_CurrentFrame ];
                    if( m_FrameAccumMs < static_cast<float>( delayMs ) )
                    {
                        break;
                    }
                    m_FrameAccumMs -= static_cast<float>( delayMs );
                    m_CurrentFrame = ( m_CurrentFrame + 1 ) % static_cast<int>( m_Textures.size( ) );
                }
            }

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
            ImGui::SetCursorPos( ImVec2( ImGui::GetCursorPosX( ) + offsetX, ImGui::GetCursorPosY( ) + offsetY ) );

            const GLuint tex = m_Textures[ m_CurrentFrame ];
            ImGui::Image( static_cast<ImTextureID>( tex ), ImVec2( drawW, drawH ) );

            if( ImGui::IsItemHovered( ) )
            {
                ImGui::SetTooltip( "%s", m_CurrentFilePath.c_str( ) );
            }
        }
    }
    ImGui::End( );

    if( windowOpen )
    {
        constexpr float k_Pad      = 6.0f;
        constexpr float k_BadgePad = 3.0f;
        ImDrawList* dl             = ImGui::GetForegroundDrawList( );
        const ImU32 colText    = ImGui::GetColorU32( ImGuiCol_Text );
        const ImU32 colBadgeBg = IM_COL32( 0, 0, 0, 140 );

        auto DrawBadge = [ & ]( ImVec2 pos, const char* text )
        {
            const ImVec2 sz = ImGui::CalcTextSize( text );
            dl->AddRectFilled(
                ImVec2( pos.x - k_BadgePad, pos.y - k_BadgePad ),
                ImVec2( pos.x + sz.x + k_BadgePad, pos.y + sz.y + k_BadgePad ),
                colBadgeBg, 3.0f );
            dl->AddText( pos, colText, text );
            return ImVec2( sz.x + ( k_BadgePad * 2.0f ), sz.y + ( k_BadgePad * 2.0f ) );
        };

        const float badgeY = contentScreenMin.y + k_Pad;

        if( !m_CurrentFilePath.empty( ) )
        {
            const std::string name = std::filesystem::path( m_CurrentFilePath ).filename( ).string( );
            const ImVec2 nameBadgeSize = DrawBadge( ImVec2( contentScreenMin.x + k_Pad, badgeY ), name.c_str( ) );

            std::vector<std::string> metadataBadges{ };
            const std::string providerName = DownloadHelpers::GetProviderName( m_CurrentFilePath );
            const std::string subfolderLabel = DownloadHelpers::GetSubfolderLabel( m_CurrentFilePath );
            const std::string fileSize = FormatFileSize( m_CurrentFilePath );

            if( !providerName.empty( ) )
            {
                metadataBadges.push_back( providerName );
            }

            if( !subfolderLabel.empty( ) )
            {
                metadataBadges.push_back( subfolderLabel );
            }

            if( !fileSize.empty( ) )
            {
                metadataBadges.push_back( fileSize );
            }

            float badgeRowY = badgeY + nameBadgeSize.y + 4.0f;
            const float badgeX = contentScreenMin.x + k_Pad;

            for( const std::string& badgeText : metadataBadges )
            {
                const ImVec2 badgeSize = DrawBadge( ImVec2( badgeX, badgeRowY ), badgeText.c_str( ) );
                badgeRowY += badgeSize.y + 4.0f;
            }
        }

        {
            float progress = -1.0f;

            if( ( m_MediaState == MediaState::GifPlaying || m_MediaState == MediaState::StaticFrame )
                && m_Textures.size( ) > 1 )
            {
                progress = static_cast<float>( m_CurrentFrame ) /
                           static_cast<float>( m_Textures.size( ) - 1 );
            }
            else if( ( m_MediaState == MediaState::VideoPlaying || m_MediaState == MediaState::VideoPaused )
                     && m_VideoPlayer )
            {
                const double duration = m_VideoPlayer->GetDuration( );
                if( duration > 0.0 )
                {
                    const double currentTime = std::clamp( GetPlaybackTimeSeconds( ), 0.0, duration );
                    progress = static_cast<float>( currentTime / duration );
                }
            }

            if( progress >= 0.0f )
            {
                constexpr float k_BarH = 4.0f;
                const float barW  = contentScreenMax.x - contentScreenMin.x;
                const float barY0 = contentScreenMax.y - k_BarH;
                const float barY1 = contentScreenMax.y;

                dl->AddRectFilled(
                    ImVec2( contentScreenMin.x, barY0 ),
                    ImVec2( contentScreenMax.x, barY1 ),
                    IM_COL32( 0, 0, 0, 120 ) );
                dl->AddRectFilled(
                    ImVec2( contentScreenMin.x, barY0 ),
                    ImVec2( contentScreenMin.x + barW * progress, barY1 ),
                    IM_COL32( 100, 180, 255, 220 ) );
            }
        }

        if( m_IsDecoding && m_MediaState == MediaState::LoadingFullFrames )
        {
            constexpr float k_BarH  = 4.0f;
            constexpr float k_SegW  = 0.3f;

            const float barW  = contentScreenMax.x - contentScreenMin.x;
            const float barY0 = contentScreenMax.y - k_BarH;
            const float barY1 = contentScreenMax.y;
            const float segW  = barW * k_SegW;

            const float t     = static_cast<float>( std::fabs( std::sin( ImGui::GetTime( ) * 1.5 ) ) );
            const float segX0 = contentScreenMin.x + ( barW - segW ) * t;
            const float segX1 = segX0 + segW;

            dl->AddRectFilled(
                ImVec2( contentScreenMin.x, barY0 ),
                ImVec2( contentScreenMax.x, barY1 ),
                IM_COL32( 0, 0, 0, 120 ) );
            dl->AddRectFilled(
                ImVec2( segX0, barY0 ),
                ImVec2( segX1, barY1 ),
                IM_COL32( 100, 180, 255, 220 ) );
        }
    }
}

void ImageScraper::MediaPreviewPanel::OnFileDownloaded( const std::string& filepath )
{
    std::lock_guard<std::mutex> lock( m_PathMutex );
    m_LatestPath    = filepath;
    m_HasLatestPath = true;
}

void ImageScraper::MediaPreviewPanel::RequestPreview( const std::string& filepath )
{
    if( filepath.empty( ) )
    {
        ClearPreview( );
        return;
    }

    m_CancelDecode       = true;
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
        m_LatestPath    = filepath;
        m_HasLatestPath = true;
    }

    m_ForceLoad    = true;
    m_PlayOnUpload = false;
}

void ImageScraper::MediaPreviewPanel::ClearPreview( )
{
    m_CancelDecode       = true;
    m_CancelAudioPrepare = true;

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
        std::lock_guard<std::mutex> lock( m_PathMutex );
        m_HasLatestPath = false;
    }

    StopAudioPlayback( );
    FreeTextures( );
    m_VideoPlayer.reset( );
    m_AudioPlayer.reset( );
    m_MediaState             = MediaState::None;
    m_CurrentFilePath.clear( );
    m_LoadingFilePath.clear( );
    m_VideoFrameBuffer.clear( );
    m_VideoFrameIndex        = 0;
    m_HasAudio               = false;
    m_PlayOnUpload           = false;
    m_IsPlaybackClockRunning = false;
    m_PlaybackTimeSeconds    = 0.0;
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
    if( m_MediaState == MediaState::StaticFrame )
    {
        if( m_Textures.size( ) > 1 )
        {
            m_MediaState = MediaState::GifPlaying;
        }
        else
        {
            KickFullGifDecode( );
        }
    }
    else if( m_MediaState == MediaState::GifPlaying )
    {
        m_MediaState   = MediaState::StaticFrame;
        m_FrameAccumMs = 0.0f;
    }
    else if( m_MediaState == MediaState::VideoPlaying )
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
        m_AudioPlayer->SetMuted( m_IsMuted );
    }
}

bool ImageScraper::MediaPreviewPanel::IsPlaying( ) const
{
    return m_MediaState == MediaState::GifPlaying ||
           m_MediaState == MediaState::VideoPlaying;
}

bool ImageScraper::MediaPreviewPanel::IsMuted( ) const
{
    return m_IsMuted;
}

bool ImageScraper::MediaPreviewPanel::CanPlayPause( ) const
{
    return m_MediaState == MediaState::StaticFrame  ||
           m_MediaState == MediaState::GifPlaying   ||
           m_MediaState == MediaState::VideoPlaying ||
           m_MediaState == MediaState::VideoPaused;
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
        filepath        = m_LatestPath;
        m_HasLatestPath = false;
    }

    m_LoadingFilePath = filepath;
    m_CancelDecode    = false;
    m_IsDecoding      = true;

    m_DecodeFuture = std::async( std::launch::async, [ this, filepath ]( )
    {
        auto decoded = DecodeFile( filepath, true );

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
    m_IsPreparingAudio   = true;

    const std::string filepath = m_CurrentFilePath;

    m_AudioPrepareFuture = std::async( std::launch::async, [ this, filepath ]( )
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

void ImageScraper::MediaPreviewPanel::KickFullGifDecode( )
{
    if( m_IsDecoding )
    {
        return;
    }

    m_MediaState      = MediaState::LoadingFullFrames;
    m_LoadingFilePath = m_CurrentFilePath;
    m_CancelDecode    = false;
    m_IsDecoding      = true;
    m_PlayOnUpload    = true;

    const std::string filepath = m_CurrentFilePath;

    m_DecodeFuture = std::async( std::launch::async, [ this, filepath ]( )
    {
        auto decoded = DecodeFile( filepath, false );

        if( !m_CancelDecode )
        {
            std::lock_guard<std::mutex> lock( m_DecodedMutex );
            m_PendingDecoded = std::move( decoded );
        }

        m_IsDecoding = false;
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

void ImageScraper::MediaPreviewPanel::StartVideoPlayback( )
{
    if( m_MediaState != MediaState::VideoPaused )
    {
        return;
    }

    m_PlaybackStartedAt     = std::chrono::steady_clock::now( );
    m_IsPlaybackClockRunning = true;
    m_MediaState            = MediaState::VideoPlaying;

    KickAudioPrepareIfNeeded( );
    StartAudioPlaybackFromCurrentTime( );
}

void ImageScraper::MediaPreviewPanel::PauseVideoPlayback( )
{
    if( m_MediaState != MediaState::VideoPlaying )
    {
        return;
    }

    m_PlaybackTimeSeconds    = GetPlaybackTimeSeconds( );
    m_IsPlaybackClockRunning = false;
    m_MediaState             = MediaState::VideoPaused;
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
    m_VideoFrameIndex     = 0;
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

    m_AudioPlayer->StartFrom( GetPlaybackTimeSeconds( ), m_IsMuted );
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

    glBindTexture( GL_TEXTURE_2D, m_Textures[ 0 ] );
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

        m_VideoFrameIndex++;
        UploadCurrentVideoFrame( );
    }
}

void ImageScraper::MediaPreviewPanel::UploadDecoded( DecodedMedia&& decoded )
{
    StopAudioPlayback( );
    FreeTextures( );
    m_VideoPlayer.reset( );
    m_AudioPlayer.reset( );
    m_HasAudio               = false;
    m_IsPlaybackClockRunning = false;
    m_PlaybackTimeSeconds    = 0.0;

    if( decoded.m_PixelData.empty( ) )
    {
        m_MediaState   = MediaState::None;
        m_PlayOnUpload = false;
        return;
    }

    m_CurrentFilePath = decoded.m_FilePath;
    m_Width           = decoded.m_Width;
    m_Height          = decoded.m_Height;
    m_CurrentFrame    = 0;
    m_FrameAccumMs    = 0.0f;
    m_FrameDelaysMs   = decoded.m_FrameDelaysMs;

    if( decoded.m_IsVideo )
    {
        m_HasAudio         = decoded.m_HasAudio;
        m_VideoFrameBuffer = std::move( decoded.m_PixelData );
        m_VideoPlayer      = std::move( decoded.m_VideoPlayer );

        GLuint textureId = 0;
        glGenTextures( 1, &textureId );
        glBindTexture( GL_TEXTURE_2D, textureId );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
                      decoded.m_Width, decoded.m_Height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE,
                      m_VideoFrameBuffer.data( ) );
        glBindTexture( GL_TEXTURE_2D, 0 );
        m_Textures.push_back( textureId );
        m_VideoFrameIndex = 0;
        m_MediaState      = MediaState::VideoPaused;

        KickAudioPrepareIfNeeded( );
        if( m_PlayOnUpload )
        {
            m_PlayOnUpload = false;
            StartVideoPlayback( );
        }
        return;
    }

    const int frameBytes = decoded.m_Width * decoded.m_Height * 4;

    m_Textures.reserve( static_cast<size_t>( decoded.m_Frames ) );
    for( int i = 0; i < decoded.m_Frames; ++i )
    {
        GLuint textureId = 0;
        glGenTextures( 1, &textureId );
        glBindTexture( GL_TEXTURE_2D, textureId );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
                      decoded.m_Width, decoded.m_Height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE,
                      decoded.m_PixelData.data( ) + i * frameBytes );
        m_Textures.push_back( textureId );
    }

    if( decoded.m_Frames > 1 )
    {
        m_MediaState = MediaState::GifPlaying;
    }
    else if( IsGif( decoded.m_FilePath ) )
    {
        m_MediaState = MediaState::StaticFrame;
    }
    else
    {
        m_MediaState = MediaState::None;
    }

    m_PlayOnUpload = false;
}

void ImageScraper::MediaPreviewPanel::FreeTextures( )
{
    if( !m_Textures.empty( ) )
    {
        glDeleteTextures( static_cast<GLsizei>( m_Textures.size( ) ), m_Textures.data( ) );
        m_Textures.clear( );
    }
    m_FrameDelaysMs.clear( );
    m_Width        = 0;
    m_Height       = 0;
    m_CurrentFrame = 0;
    m_FrameAccumMs = 0.0f;
}

std::unique_ptr<ImageScraper::MediaPreviewPanel::DecodedMedia>
ImageScraper::MediaPreviewPanel::DecodeFile( const std::string& filepath, bool firstFrameOnly )
{
    if( IsVideo( filepath ) )
    {
        return DecodeVideoFile( filepath );
    }

    if( IsGif( filepath ) && !firstFrameOnly )
    {
        if( auto decoded = DecodeVideoFile( filepath ) )
        {
            return decoded;
        }

        LogDebug( "[%s] Falling back to stb GIF decode for: %s", __FUNCTION__, filepath.c_str( ) );
    }

    std::ifstream file( filepath, std::ios::binary | std::ios::ate );
    if( !file.is_open( ) )
    {
        LogError( "[%s] Could not open file for preview: %s", __FUNCTION__, filepath.c_str( ) );
        return nullptr;
    }

    const std::streamsize size = file.tellg( );
    file.seekg( 0, std::ios::beg );
    std::vector<unsigned char> fileData( static_cast<size_t>( size ) );
    if( !file.read( reinterpret_cast<char*>( fileData.data( ) ), size ) )
    {
        LogError( "[%s] Could not read file for preview: %s", __FUNCTION__, filepath.c_str( ) );
        return nullptr;
    }

    auto decoded        = std::make_unique<DecodedMedia>( );
    decoded->m_FilePath = filepath;

    if( !firstFrameOnly && IsGif( filepath ) )
    {
        int* delays   = nullptr;
        int  width    = 0;
        int  height   = 0;
        int  frames   = 0;
        int  channels = 0;

        unsigned char* data = stbi_load_gif_from_memory(
            fileData.data( ), static_cast<int>( fileData.size( ) ),
            &delays, &width, &height, &frames, &channels, 4 );

        if( !data )
        {
            LogError( "[%s] stbi_load_gif_from_memory failed for: %s", __FUNCTION__, filepath.c_str( ) );
            return nullptr;
        }

        decoded->m_Width  = width;
        decoded->m_Height = height;
        decoded->m_Frames = frames;

        const int frameBytes = width * height * 4;
        decoded->m_PixelData.assign( data, data + static_cast<size_t>( frames ) * frameBytes );

        decoded->m_FrameDelaysMs.reserve( static_cast<size_t>( frames ) );
        for( int i = 0; i < frames; ++i )
        {
            const int delayMs = delays ? delays[ i ] : 100;
            decoded->m_FrameDelaysMs.push_back( std::max( delayMs, 20 ) );
        }

        stbi_image_free( data );
        if( delays )
        {
            free( delays );
        }
    }
    else
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* data = stbi_load_from_memory(
            fileData.data( ), static_cast<int>( fileData.size( ) ),
            &width, &height, &channels, 4 );

        if( !data )
        {
            LogDebug( "[%s] stbi_load_from_memory failed (unsupported format?) for: %s", __FUNCTION__, filepath.c_str( ) );
            return nullptr;
        }

        decoded->m_Width  = width;
        decoded->m_Height = height;
        decoded->m_Frames = 1;
        decoded->m_PixelData.assign( data, data + static_cast<size_t>( width ) * height * 4 );

        stbi_image_free( data );
    }

    return decoded;
}

std::unique_ptr<ImageScraper::MediaPreviewPanel::DecodedMedia>
ImageScraper::MediaPreviewPanel::DecodeVideoFile( const std::string& filepath )
{
    auto decoded        = std::make_unique<DecodedMedia>( );
    decoded->m_FilePath = filepath;
    decoded->m_IsVideo  = true;
    decoded->m_VideoPlayer = std::make_unique<VideoPlayer>( );

    if( !decoded->m_VideoPlayer->Open( filepath ) )
    {
        return nullptr;
    }

    decoded->m_HasAudio = decoded->m_VideoPlayer->HasAudio( );
    decoded->m_Width    = decoded->m_VideoPlayer->GetWidth( );
    decoded->m_Height   = decoded->m_VideoPlayer->GetHeight( );

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

    std::string ext = filepath.substr( dot + 1 );
    std::transform( ext.begin( ), ext.end( ), ext.begin( ),
        []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return ext == "gif";
}

bool ImageScraper::MediaPreviewPanel::IsVideo( const std::string& filepath )
{
    const auto dot = filepath.rfind( '.' );
    if( dot == std::string::npos )
    {
        return false;
    }

    std::string ext = filepath.substr( dot + 1 );
    std::transform( ext.begin( ), ext.end( ), ext.begin( ),
        []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return ext == "mp4" || ext == "webm" || ext == "mov" || ext == "mkv" || ext == "avi";
}

std::string ImageScraper::MediaPreviewPanel::FormatFileSize( const std::string& filepath )
{
    std::error_code ec;
    const auto bytes = std::filesystem::file_size( filepath, ec );
    if( ec )
    {
        return { };
    }

    std::ostringstream ss;
    if( bytes < 1024 )
    {
        ss << bytes << " B";
    }
    else if( bytes < 1024 * 1024 )
    {
        ss << std::fixed << std::setprecision( 1 ) << ( bytes / 1024.0 ) << " KB";
    }
    else
    {
        ss << std::fixed << std::setprecision( 1 ) << ( bytes / ( 1024.0 * 1024.0 ) ) << " MB";
    }

    return ss.str( );
}
