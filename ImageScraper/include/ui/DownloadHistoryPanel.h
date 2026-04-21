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
#include <future>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
        using ReleaseCallback = std::function<void( const std::string& filepath )>;

        struct ThumbnailEntry
        {
            GLuint m_Texture{ 0 };
            int    m_Width{ 0 };
            int    m_Height{ 0 };
            bool   m_IsLoading{ false };
        };

        DownloadHistoryPanel( PreviewCallback onPreviewRequested, ReleaseCallback onReleaseRequested );
        ~DownloadHistoryPanel( );
        void Update( ) override;

        void Load( std::shared_ptr<JsonFile> appConfig, const std::filesystem::path& downloadsRoot );

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
        struct DecodedThumbnail
        {
            std::string                m_FilePath{ };
            std::vector<unsigned char> m_PixelData{ };
            int                        m_Width{ 0 };
            int                        m_Height{ 0 };
        };

        void FlushPending( );
        void FlushDecodedThumbnails( );
        void Save( );
        void SaveSelectedPath( );
        void SetSelection( const std::string& path, bool scrollToSelected, bool requestPreview );
        void SetSelectionFromHistoryIndex( int index, bool scrollToSelected, bool requestPreview );
        void ClearSelection( bool requestPreview );
        void SyncHistorySelectionFromPath( );
        void DeletePath( const std::filesystem::path& path );
        void AdvanceSelectionAndPreview( );
        void RemoveEntriesInPath( const std::filesystem::path& targetPath, bool treatAsDirectory );
        void RenderTreeNode( const std::filesystem::path& path, bool* openDeleteConfirm );
        void ShowPathContextMenu( const std::filesystem::path& path, bool* openDeleteConfirm );
        void ShowPathTooltip( const std::filesystem::path& path );
        bool IsSelectedPath( const std::filesystem::path& path ) const;
        bool HasSelectedDescendant( const std::filesystem::path& path ) const;
        bool CanDeletePath( const std::filesystem::path& path ) const;
        bool IsRootPath( const std::filesystem::path& path ) const;
        const DownloadHistoryEntry* FindHistoryEntryByPath( const std::string& filepath ) const;
        int  FindHistoryIndexByPath( const std::string& filepath ) const;
        int  FindExistingHistoryIndexAtOrBefore( int startIndex ) const;
        int  FindExistingHistoryIndexAfter( int startIndex ) const;
        void EvictThumbnail( const std::string& filepath );
        static void OpenInExplorer( const std::filesystem::path& path );
        static std::string FormatTimestamp( );
        static std::string ExtractFileName( const std::string& filepath );
        static std::string GetFileTypeLabel( const std::string& filepath );
        static std::string FormatFileSize( const std::string& filepath );
        static std::string GetSizeColumnLabel( const std::filesystem::path& path );
        static std::string GetTypeColumnLabel( const std::filesystem::path& path );
        static std::string GetCreationTimeColumnLabel( const std::filesystem::path& path );
        static std::string MakePreferredPathString( const std::filesystem::path& path );
        static bool IsPathWithinRoot( const std::filesystem::path& path, const std::filesystem::path& root );

        // Returns thumbnail entry for the filepath; entry.m_Texture == 0 means unavailable.
        // Loads on first call, caches the result - never retries failed loads.
        ThumbnailEntry GetOrLoadThumbnail( const std::string& filepath );
        void RequestThumbnailLoad( const std::string& filepath );
        void UploadDecodedThumbnail( DecodedThumbnail&& decoded );
        static DecodedThumbnail DecodeThumbnail( const std::string& filepath );
        static DecodedThumbnail DecodeVideoThumbnail( const std::string& filepath );
        static bool IsSupportedMediaExtension( const std::string& filepath );
        static bool IsVideoExtension( const std::string& filepath );

        static constexpr int        k_Capacity          = 200;
        static constexpr uintmax_t  k_MaxThumbnailBytes = 5 * 1024 * 1024; // 5 MB
        static constexpr float      k_TooltipMaxSize    = 200.f;

        PreviewCallback                   m_OnPreviewRequested{ };
        ReleaseCallback                   m_OnReleaseRequested{ };
        RingBuffer<DownloadHistoryEntry>  m_History{ k_Capacity };
        std::shared_ptr<JsonFile>         m_AppConfig{ };
        std::filesystem::path             m_DownloadsRoot{ };

        std::mutex                        m_PendingMutex{ };
        std::vector<DownloadHistoryEntry> m_Pending{ };

        // Thumbnail cache: filepath -> texture + dimensions (texture == 0 means failed/skipped)
        std::unordered_map<std::string, ThumbnailEntry>    m_ThumbnailCache{ };
        std::mutex                                         m_DecodedThumbnailMutex{ };
        std::vector<DecodedThumbnail>                      m_DecodedThumbnails{ };
        std::unordered_map<std::string, std::future<void>> m_ThumbnailFutures{ };
        std::unordered_set<std::string>                    m_InFlightThumbnails{ };

        std::string m_SelectedPath{ };
        std::string m_DeleteConfirmPath{ };
        int         m_SelectedHistoryIndex{ -1 };
        bool        m_Blocked{ false };
        bool        m_ScrollToSelected{ false };
    };
}
