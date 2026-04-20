#pragma once

#include "ui/IUiPanel.h"
#include "ui/VideoPlayer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <future>
#include <filesystem>

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

        // True when media is actively animating (GifPlaying or VideoPlaying)
        bool IsPlaying( ) const;

        // True when TogglePlayPause would have any effect
        bool CanPlayPause( ) const;

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
            std::string m_FilePath;
            bool        m_IsVideo{ false };
        };

        void KickDecodeIfNeeded( );
        void KickFullGifDecode( );
        void UploadDecoded( DecodedMedia&& decoded );
        void FreeTextures( );

        void AdvanceVideoFrame( );

        static std::unique_ptr<DecodedMedia> DecodeFile( const std::string& filepath, bool firstFrameOnly );
        static std::unique_ptr<DecodedMedia> DecodeVideoFile( const std::string& filepath );
        static bool IsGif( const std::string& filepath );
        static bool IsVideo( const std::string& filepath );
        static std::filesystem::path GetProviderRoot( const std::string& filepath );
        static std::string GetProviderName( const std::string& filepath );
        static std::string GetSubfolderLabel( const std::string& filepath );
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
        std::string         m_LoadingFilePath{ };  // full path of file currently being decoded
        bool                m_ForceLoad{ false };

        // Current display state - only touched on the main thread
        std::vector<GLuint> m_Textures{ };
        std::vector<int>    m_FrameDelaysMs{ };
        int         m_Width{ 0 };
        int         m_Height{ 0 };
        int         m_CurrentFrame{ 0 };
        float       m_FrameAccumMs{ 0.0f };
        std::string m_CurrentFilePath{ };
        MediaState  m_MediaState{ MediaState::None };

        // Video playback - single reused texture, decoded frame-by-frame
        std::unique_ptr<VideoPlayer> m_VideoPlayer{ };
        int m_VideoFrameIndex{ 0 };  // frames decoded since last open/seek, for progress bar
        std::vector<uint8_t>         m_VideoFrameBuffer{ };  // reused RGBA scratch buffer
    };
}
