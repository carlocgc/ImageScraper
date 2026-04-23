#pragma once

#include "ui/IUiPanel.h"
#include "ui/VideoPlayer.h"
#include "io/JsonFile.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <filesystem>
#include <future>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ImageScraper
{
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

        // Navigate to the next older file in the downloads view; fires preview callback.
        void SelectNext( );

        // Navigate to the next newer file in the downloads view; fires preview callback.
        void SelectPrevious( );

        // True if there is a valid older file to navigate to.
        bool HasNext( ) const;

        // True if there is a valid newer file to navigate to.
        bool HasPrevious( ) const;

    private:
        struct DecodedThumbnail
        {
            std::string                m_FilePath{ };
            std::vector<unsigned char> m_PixelData{ };
            int                        m_Width{ 0 };
            int                        m_Height{ 0 };
        };

        struct TreeNodeSnapshot
        {
            std::filesystem::path       m_Path{ };
            std::string                 m_PathString{ };
            std::string                 m_Label{ };
            bool                        m_IsDirectory{ false };
            uintmax_t                   m_SizeBytes{ 0 };
            std::string                 m_SizeLabel{ };
            std::string                 m_TypeLabel{ };
            unsigned long long          m_CreationTicks{ 0 };
            std::string                 m_CreationLabel{ };
            std::vector<TreeNodeSnapshot> m_Children{ };
        };

        void FlushPending( );
        void FlushDecodedThumbnails( );
        void InvalidateTreeCaches( );
        void RefreshTreeSnapshot( const ImGuiTableSortSpecs* sortSpecs );
        std::optional<TreeNodeSnapshot> BuildTreeNodeSnapshot(
            const std::filesystem::path& path,
            ImGuiID sortColumnUserId,
            ImGuiSortDirection sortDirection ) const;
        void SaveSelectedPath( );
        void SetSelection( const std::string& path, bool scrollToSelected, bool requestPreview );
        void ClearSelection( bool requestPreview );
        void DeletePath( const std::filesystem::path& path );
        void AdvanceSelectionAndPreview( int preferredIndex = -1 );
        void RenderTreeNode( const TreeNodeSnapshot& node, bool* openDeleteConfirm );
        void ShowPathContextMenu( const TreeNodeSnapshot& node, bool* openDeleteConfirm );
        void ShowPathTooltip( const TreeNodeSnapshot& node );
        bool IsSelectedPath( const std::filesystem::path& path ) const;
        bool HasSelectedDescendant( const std::string& pathString ) const;
        bool CanDeletePath( const std::filesystem::path& path ) const;
        bool IsRootPath( const std::string& pathString ) const;
        const std::vector<std::filesystem::path>& GetNavigableFiles( ) const;
        int  FindNavigableIndexByPath( const std::string& filepath ) const;
        void EvictThumbnailsInPath( const std::filesystem::path& targetPath, bool treatAsDirectory );
        void EvictThumbnail( const std::string& filepath );
        static void OpenInExplorer( const std::filesystem::path& path );
        static std::string ExtractFileName( const std::string& filepath );
        static std::string GetFileTypeLabel( const std::string& filepath );
        static std::string FormatFileSize( const std::string& filepath );
        static std::string GetSizeColumnLabel( const std::filesystem::path& path );
        static std::string GetTypeColumnLabel( const std::filesystem::path& path );
        static std::string GetCreationTimeColumnLabel( const std::filesystem::path& path );
        static std::string MakePreferredPathString( const std::filesystem::path& path );
        static bool IsPathStringWithinRoot( const std::string& path, const std::string& root );
        static bool IsPathWithinRoot( const std::filesystem::path& path, const std::filesystem::path& root );

        // Returns thumbnail entry for the filepath; entry.m_Texture == 0 means unavailable.
        // Loads on first call, caches the result - never retries failed loads.
        ThumbnailEntry GetOrLoadThumbnail( const std::string& filepath );
        void RequestThumbnailLoad( const std::string& filepath );
        void UploadDecodedThumbnail( DecodedThumbnail&& decoded );
        static DecodedThumbnail DecodeThumbnail( const std::string& filepath );
        static DecodedThumbnail DecodeFrameThumbnail( const std::string& filepath );
        static bool IsSupportedMediaExtension( const std::string& filepath );
        static bool IsVideoExtension( const std::string& filepath );

        static constexpr uintmax_t  k_MaxThumbnailBytes = 5 * 1024 * 1024; // 5 MB
        static constexpr float      k_TooltipMaxSize    = 200.f;

        PreviewCallback           m_OnPreviewRequested{ };
        ReleaseCallback           m_OnReleaseRequested{ };
        std::shared_ptr<JsonFile> m_AppConfig{ };
        std::filesystem::path     m_DownloadsRoot{ };
        std::string               m_DownloadsRootString{ };

        std::mutex               m_PendingMutex{ };
        std::vector<std::string> m_PendingPaths{ };

        // Thumbnail cache: filepath -> texture + dimensions (texture == 0 means failed/skipped)
        std::unordered_map<std::string, ThumbnailEntry>    m_ThumbnailCache{ };
        std::mutex                                         m_DecodedThumbnailMutex{ };
        std::vector<DecodedThumbnail>                      m_DecodedThumbnails{ };
        std::unordered_map<std::string, std::future<void>> m_ThumbnailFutures{ };
        std::unordered_set<std::string>                    m_InFlightThumbnails{ };
        std::optional<TreeNodeSnapshot>                    m_TreeSnapshot{ };
        ImGuiID                                            m_TreeSortColumnUserId{ 0 };
        ImGuiSortDirection                                 m_TreeSortDirection{ ImGuiSortDirection_Ascending };
        bool                                               m_TreeDirty{ true };
        mutable std::vector<std::filesystem::path>         m_NavigableFilesCache{ };
        mutable std::unordered_map<std::string, int>       m_NavigableFileIndexByPath{ };
        mutable bool                                       m_NavigableFilesDirty{ true };

        std::string m_SelectedPath{ };
        std::string m_DeleteConfirmPath{ };
        bool        m_Blocked{ false };
        bool        m_ScrollToSelected{ false };
    };
}
