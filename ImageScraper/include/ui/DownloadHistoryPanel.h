#pragma once

#include "ui/IUiPanel.h"
#include "ui/VideoPlayer.h"
#include "collections/RingBuffer.h"
#include "io/JsonFile.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace ImageScraper
{
    struct DownloadHistoryEntry
    {
        std::string m_FilePath;
        std::string m_FileName;
        std::string m_FileSize;
        std::string m_SourceUrl;
        std::string m_Timestamp;
    };

    class DownloadHistoryPanel : public IUiPanel
    {
    public:
        using PreviewCallback = std::function<void( const std::string& filepath )>;

        struct ThumbnailEntry
        {
            GLuint m_Texture{ 0 };
            int    m_Width{ 0 };
            int    m_Height{ 0 };
        };

        explicit DownloadHistoryPanel( PreviewCallback onPreviewRequested );
        ~DownloadHistoryPanel( );
        void Update( ) override;

        void Load( std::shared_ptr<JsonFile> appConfig );

        // Thread-safe - may be called from a worker thread
        void OnFileDownloaded( const std::string& filepath, const std::string& sourceUrl );

    private:
        void FlushPending( );
        void Save( );
        static void OpenInExplorer( const std::string& filepath );
        static std::string FormatTimestamp( );
        static std::string ExtractFileName( const std::string& filepath );
        static std::string FormatFileSize( const std::string& filepath );

        // Returns thumbnail entry for the filepath; entry.m_Texture == 0 means unavailable.
        // Loads on first call, caches the result - never retries failed loads.
        ThumbnailEntry GetOrLoadThumbnail( const std::string& filepath );
        static ThumbnailEntry LoadVideoThumbnail( const std::string& filepath );
        static bool IsSupportedMediaExtension( const std::string& filepath );
        static bool IsVideoExtension( const std::string& filepath );

        static constexpr int        k_Capacity          = 200;
        static constexpr uintmax_t  k_MaxThumbnailBytes = 5 * 1024 * 1024; // 5 MB
        static constexpr float      k_TooltipMaxSize    = 200.f;

        PreviewCallback                   m_OnPreviewRequested{ };
        RingBuffer<DownloadHistoryEntry>  m_History{ k_Capacity };
        std::shared_ptr<JsonFile>         m_AppConfig{ };

        std::mutex                        m_PendingMutex{ };
        std::vector<DownloadHistoryEntry> m_Pending{ };

        // Thumbnail cache: filepath → texture + dimensions (texture == 0 means failed/skipped)
        std::unordered_map<std::string, ThumbnailEntry> m_ThumbnailCache{ };

        // Tracks all filepaths currently in m_History to prevent duplicate entries
        std::unordered_set<std::string> m_KnownPaths{ };
    };
}
