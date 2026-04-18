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

        // Main-thread only - synchronously clears all display state
        void ClearPreview( );

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

        // Holds decoded pixel data produced on a background thread (images + GIFs only)
        struct DecodedImage
        {
            std::vector<unsigned char> m_PixelData;     // RGBA, all frames contiguous
            std::vector<int>           m_FrameDelaysMs;
            int         m_Width{ 0 };
            int         m_Height{ 0 };
            int         m_Frames{ 1 };
            std::string m_FilePath;
        };

        void KickDecodeIfNeeded( );
        void KickFullGifDecode( );
        void UploadDecoded( const DecodedImage& decoded );
        void FreeTextures( );

        // Video helpers
        void OpenVideo( const std::string& filepath );
        void AdvanceVideoFrame( );

        static std::unique_ptr<DecodedImage> DecodeFile( const std::string& filepath, bool firstFrameOnly );
        static bool IsGif( const std::string& filepath );
        static bool IsVideo( const std::string& filepath );

        // Latest path posted by OnFileDownloaded (worker thread → Update)
        std::mutex  m_PathMutex{ };
        std::string m_LatestPath{ };
        bool        m_HasLatestPath{ false };

        // Decoded result posted by the decode task (worker thread → Update) - images/GIFs only
        std::mutex                    m_DecodedMutex{ };
        std::unique_ptr<DecodedImage> m_PendingDecoded{ };

        std::atomic_bool    m_IsDecoding{ false };
        std::future<void>   m_DecodeFuture{ };
        std::string         m_LoadingFileName{ };
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
        std::vector<uint8_t>         m_VideoFrameBuffer{ };  // reused RGBA scratch buffer
    };
}
