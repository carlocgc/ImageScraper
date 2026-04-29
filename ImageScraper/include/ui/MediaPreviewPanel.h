#pragma once

#include "io/JsonFile.h"
#include "ui/IUiPanel.h"
#include "ui/MediaAudioPlayer.h"
#include "ui/VideoPlayer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include <chrono>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <future>

namespace ImageScraper
{
    class MediaPreviewPanel : public IUiPanel
    {
    public:
        ~MediaPreviewPanel( );
        void Update( ) override;

        // Thread-safe - may be called from a worker thread
        void OnFileDownloaded( const std::string& filepath );

        // Main-thread only - forces a load even if media is currently playing
        void RequestPreview( const std::string& filepath );

        // Main-thread only - synchronously clears all display state;
        // waits for any in-progress background decode to finish so file handles are released.
        void ClearPreview( );

        // Main-thread only - clears the preview if the given filepath is currently loaded
        // or being decoded; no-op otherwise. Waits for any in-progress decode to finish.
        void ReleaseFileIfCurrent( const std::string& filepath );

        // Main-thread only - toggle between playing and paused; no-op when no media or loading
        void TogglePlayPause( );
        void ToggleMute( );

        // True when media is actively animating (GifPlaying or VideoPlaying)
        bool IsPlaying( ) const;
        bool IsMuted( ) const;

        // True when TogglePlayPause would have any effect
        bool CanPlayPause( ) const;
        bool CanMute( ) const;

        // True when the preview is currently hidden by the user's privacy toggle
        bool IsPrivacyMode( ) const { return m_PrivacyMode; }

        // True when the loaded media supports interactive seek (multi-frame GIF or
        // open video with known duration).
        bool CanScrub( ) const;

        // Current playback progress in 0..1, or -1 if not currently scrubbable.
        float GetProgress( ) const;

        // Optional human-readable position label for the scrub tooltip
        // (e.g. "00:12 / 01:34" for video, "frame 4 / 12" for GIFs). Empty if N/A.
        std::string GetProgressLabel( ) const;

        // Interactive scrub. Driven by the control panel.
        // BeginScrub: pause playback (remember play-state).
        // UpdateScrub: live throttled seek; videos snap to keyframes.
        // EndScrub: commit the final position with an exact seek; resume playback if it was playing.
        void BeginScrub( );
        void UpdateScrub( float normalized );
        void EndScrub( float normalized );

        // Loads persisted preferences (currently: privacy mode toggle)
        void LoadPanelState( std::shared_ptr<JsonFile> appConfig );

        // Optional icon font used for the privacy toggle. nullptr falls back to a text glyph.
        void SetIconFont( ImFont* iconFont );

    private:
        enum class MediaState
        {
            None,
            // GIF states
            StaticFrame,
            LoadingFullFrames,
            GifPlaying,
            // Video states
            VideoPlaying,
            VideoPaused,
        };

        // Holds preview data produced on a background thread before texture upload on the main thread.
        struct DecodedMedia
        {
            std::vector<unsigned char> m_PixelData;     // RGBA, all frames contiguous
            std::vector<int>           m_FrameDelaysMs;
            std::unique_ptr<VideoPlayer> m_VideoPlayer{ };
            int         m_Width{ 0 };
            int         m_Height{ 0 };
            int         m_Frames{ 1 };
            bool        m_HasAudio{ false };
            std::string m_FilePath;
            bool        m_IsVideo{ false };
        };

        void KickDecodeIfNeeded( );
        void KickAudioPrepareIfNeeded( );
        void KickFullGifDecode( );
        void UploadDecoded( DecodedMedia&& decoded );
        void FreeTextures( );
        void ApplyPreparedAudio( );

        void AdvanceVideoFrame( );
        void OnPrivacyModeChanged( );
        void ResetPlaybackToStartPaused( );
        void StartVideoPlayback( );
        void PauseVideoPlayback( );
        void RestartVideoPlayback( );
        void StartAudioPlaybackFromCurrentTime( );
        void StopAudioPlayback( );
        void UploadCurrentVideoFrame( ) const;
        double GetPlaybackTimeSeconds( ) const;

        static std::unique_ptr<DecodedMedia> DecodeFile( const std::string& filepath, bool firstFrameOnly );
        static std::unique_ptr<DecodedMedia> DecodeStillImageFile( const std::string& filepath );
        static std::unique_ptr<DecodedMedia> DecodeVideoFile( const std::string& filepath );
        static bool IsGif( const std::string& filepath );
        static bool IsVideo( const std::string& filepath );
        static std::string FormatFileSize( const std::string& filepath );

        // Latest path posted by OnFileDownloaded (worker thread → Update)
        std::mutex  m_PathMutex{ };
        std::string m_LatestPath{ };
        bool        m_HasLatestPath{ false };

        // Decoded result posted by the decode task (worker thread → Update)
        std::mutex                    m_DecodedMutex{ };
        std::unique_ptr<DecodedMedia> m_PendingDecoded{ };

        std::atomic_bool    m_IsDecoding{ false };
        std::atomic_bool    m_CancelDecode{ false };  // set by RequestPreview to discard in-flight result
        std::future<void>   m_DecodeFuture{ };
        std::mutex                       m_PendingAudioMutex{ };
        std::unique_ptr<MediaAudioPlayer> m_PendingAudioPlayer{ };
        std::atomic_bool                 m_IsPreparingAudio{ false };
        std::atomic_bool                 m_CancelAudioPrepare{ false };
        std::future<void>                m_AudioPrepareFuture{ };
        std::string         m_LoadingFilePath{ };  // full path of file currently being decoded
        bool                m_ForceLoad{ false };
        bool                m_PlayOnUpload{ false };

        // Current display state - only touched on the main thread
        std::vector<GLuint> m_Textures{ };
        std::vector<int>    m_FrameDelaysMs{ };
        int         m_Width{ 0 };
        int         m_Height{ 0 };
        int         m_CurrentFrame{ 0 };
        float       m_FrameAccumMs{ 0.0f };
        std::string m_CurrentFilePath{ };
        MediaState  m_MediaState{ MediaState::None };
        bool        m_HasAudio{ false };
        bool        m_IsMuted{ true };
        bool        m_PrivacyMode{ false };

        std::shared_ptr<JsonFile> m_AppConfig{ };
        ImFont*                   m_IconFont{ nullptr };

        // Video playback - single reused texture, decoded frame-by-frame
        std::unique_ptr<VideoPlayer> m_VideoPlayer{ };
        std::unique_ptr<MediaAudioPlayer> m_AudioPlayer{ };
        int m_VideoFrameIndex{ 0 };  // frames decoded since last open/seek, for progress bar
        std::vector<uint8_t>         m_VideoFrameBuffer{ };  // reused RGBA scratch buffer
        double                       m_PlaybackTimeSeconds{ 0.0 };
        std::chrono::steady_clock::time_point m_PlaybackStartedAt{ };
        bool                         m_IsPlaybackClockRunning{ false };

        // Scrubbing state - main thread only.
        bool                         m_IsScrubbing{ false };
        bool                         m_WasPlayingBeforeScrub{ false };
        std::chrono::steady_clock::time_point m_LastScrubSeekAt{ };
    };
}
