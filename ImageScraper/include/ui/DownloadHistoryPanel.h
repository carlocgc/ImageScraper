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
#include <filesystem>
#include <unordered_map>

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
        using PreviewCallback  = std::function<void( const std::string& filepath )>;
        // Called before a file is deleted on disk; impl should release any open handle for that file.
        using ReleaseCallback = std::function<void( const std::string& filepath )>;

        struct ThumbnailEntry
        {
            GLuint m_Texture{ 0 };
            int    m_Width{ 0 };
            int    m_Height{ 0 };
        };

        DownloadHistoryPanel( PreviewCallback onPreviewRequested, ReleaseCallback onReleaseRequested );
        ~DownloadHistoryPanel( );
        void Update( ) override;

        void Load( std::shared_ptr<JsonFile> appConfig );

        // Thread-safe - may be called from a worker thread
        void OnFileDownloaded( const std::string& filepath, const std::string& sourceUrl );

        // Called by FrontEnd each frame before Update() to propagate blocked state.
        void SetBlocked( bool blocked ) { m_Blocked = blocked; }

        // Navigate to the next-older item in the history list; fires preview callback.
        void SelectNext( );

        // Navigate to the next-newer item in the history list; fires preview callback.
        void SelectPrevious( );

        // True if there is a valid older item to navigate to.
        bool HasNext( ) const;

        // True if there is a valid newer item to navigate to.
        bool HasPrevious( ) const;

    private:
        void FlushPending( );
        void Save( );
        void SaveSelectedPath( );
        void EvictThumbnail( const std::string& filepath );
        void AdvanceSelectionAndPreview( );
        int  FindHistoryIndexByPath( const std::string& filepath ) const;
        int  FindExistingHistoryIndexAtOrBefore( int startIndex ) const;
        int  FindExistingHistoryIndexAfter( int startIndex ) const;
        void RemoveEntriesWithPrefix( const std::string& rootDir );
        static void OpenInExplorer( const std::string& filepath );
        static std::string FormatTimestamp( );
        static std::string ExtractFileName( const std::string& filepath );
        static std::string FormatFileSize( const std::string& filepath );
        static std::filesystem::path GetProviderRoot(   const std::string& filepath );
        static std::string           GetProviderName(   const std::string& filepath );
        // Absolute path of the content subfolder one level below the provider root.
        static std::filesystem::path GetSubfolderPath(  const std::string& filepath );
        // Display-formatted label for the subfolder (e.g. "r/aww", "/g/", "username").
        static std::string           GetSubfolderLabel( const std::string& filepath );

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
        ReleaseCallback                   m_OnReleaseRequested{ };
        RingBuffer<DownloadHistoryEntry>  m_History{ k_Capacity };
        std::shared_ptr<JsonFile>         m_AppConfig{ };

        std::mutex                        m_PendingMutex{ };
        std::vector<DownloadHistoryEntry> m_Pending{ };

        // Thumbnail cache: filepath → texture + dimensions (texture == 0 means failed/skipped)
        std::unordered_map<std::string, ThumbnailEntry> m_ThumbnailCache{ };

        int  m_SelectedIndex{ -1 };
        bool m_Blocked{ false };
        bool m_ScrollToSelected{ false };
    };
}
