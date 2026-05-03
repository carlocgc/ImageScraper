#pragma once

#include "io\JsonFile.h"
#include "ui\IUiPanel.h"
#include "updater\UpdateChecker.h"

#include <filesystem>
#include <functional>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ImageScraper
{
    enum class SettingsUpdateStatus
    {
        NotChecked,
        Checking,
        UpToDate,
        UpdateAvailable,
        Skipped,
        Failed,
    };

    class SettingsPanel : public IUiPanel
    {
    public:
        using DownloadRootChangedCallback = std::function<void( const std::filesystem::path& downloadRoot )>;

        static constexpr const char* s_CheckUpdatesOnStartupConfigKey = "check_updates_on_startup";
        static constexpr const char* s_LastUpdatePromptedVersionConfigKey = "last_update_prompted_version";
        static constexpr const char* s_SkippedUpdateVersionConfigKey = "skipped_update_version";
#ifdef _DEBUG
        static constexpr const char* s_CheckDevelopmentVersionConfigKey = "check_development_version_updates";
#endif

        SettingsPanel(
            std::shared_ptr<JsonFile> appConfig,
            std::string caBundlePath,
            std::filesystem::path defaultDownloadRoot,
            std::filesystem::path initialDownloadRoot,
            DownloadRootChangedCallback onDownloadRootChanged );

        void Update( ) override;
        void SetBlocked( bool blocked ) { m_Blocked = blocked; }

        bool IsCheckUpdatesOnStartupEnabled( ) const { return m_CheckUpdatesOnStartup; }
        bool IsDownloadLocationResolved( ) const { return m_DownloadLocationResolved; }
        bool IsLocationOperationActive( ) const;
        SettingsUpdateStatus GetUpdateStatus( ) const { return m_UpdateStatus; }

    private:
        enum class DownloadLocationOperationStage
        {
            Idle,
            Preparing,
            CheckingSpace,
            AwaitingUnknownSpaceConfirmation,
            Copying,
            CopyComplete,
            CopyCancelled,
            Deleting,
            DeleteComplete,
            DeleteCancelled,
        };

        struct CopyPlanEntry
        {
            std::filesystem::path m_SourcePath{ };
            std::filesystem::path m_DestinationPath{ };
            uintmax_t             m_SizeBytes{ 0 };
        };

        struct DeletePlanEntry
        {
            std::filesystem::path m_Path{ };
            uintmax_t             m_SizeBytes{ 0 };
            bool                  m_IsDirectory{ false };
        };

        struct LocationOperationProgress
        {
            uintmax_t   m_TotalBytes{ 0 };
            uintmax_t   m_ProcessedBytes{ 0 };
            uintmax_t   m_RequiredBytes{ 0 };
            uintmax_t   m_AvailableBytes{ 0 };
            int         m_TotalFiles{ 0 };
            int         m_ProcessedFiles{ 0 };
            int         m_SkippedFiles{ 0 };
            int         m_FailedFiles{ 0 };
            std::string m_CurrentPath{ };
            std::string m_Message{ };
        };

        void LoadState( );
        void PersistCheckUpdatesOnStartup( );
#ifdef _DEBUG
        void PersistCheckDevelopmentVersion( );
#endif
        void PersistStringValue( const std::string& key, const std::string& value );

        void StartUpdateCheck( bool manual );
        void OnUpdateCheckComplete( const UpdateCheckResult& result, bool manual );
        bool ShouldShowStartupPromptFor( const GitHubReleaseInfo& release ) const;

        void DrawUpdateStatus( ) const;
        void DrawUpdatePopup( );
        bool OpenReleasePage( const std::string& url ) const;
        void DrawDownloadsSettings( );
        void DrawMissingDownloadLocationPopup( );
        void DrawMoveDownloadsPopup( );
        void DrawLocationOperationPopup( );
        void DrawCopyCompletePopup( const LocationOperationProgress& progress );
        void ChooseDownloadLocation( );
        void UseDefaultDownloadLocation( );
        void RequestDownloadRootChange( const std::filesystem::path& nextRoot );
        void ApplyDownloadRoot( const std::filesystem::path& downloadRoot );
        void PersistDownloadRoot( const std::filesystem::path& downloadRoot );
        bool ShouldPromptToMoveDownloads( const std::filesystem::path& previousRoot, const std::filesystem::path& nextRoot ) const;
        void BeginCopyDownloadLocation( const std::filesystem::path& previousRoot, const std::filesystem::path& nextRoot );
        void StartCopyWorker( bool skipFreeSpaceCheck );
        void StartDeleteOriginalsWorker( );
        void RequestLocationOperationCancel( );
        void CompleteLocationChangeKeepingOriginals( );
        void ResetLocationOperation( );
        void ScanCopyPlan(
            const std::filesystem::path& previousRoot,
            const std::filesystem::path& nextRoot,
            std::vector<CopyPlanEntry>& copyPlan,
            LocationOperationProgress& progress );
        void CopyPlannedDownloads(
            const std::vector<CopyPlanEntry>& copyPlan,
            LocationOperationProgress& progress );
        void ScanDeletePlan(
            const std::filesystem::path& root,
            std::vector<DeletePlanEntry>& deletePlan,
            LocationOperationProgress& progress );
        void DeletePlannedOriginals(
            const std::vector<DeletePlanEntry>& deletePlan,
            LocationOperationProgress& progress );
        bool CheckFreeSpaceForCopy( const std::filesystem::path& nextRoot, LocationOperationProgress& progress );
        bool CopyFileWithProgress( const CopyPlanEntry& entry, LocationOperationProgress& progress );
        bool DeleteFileWithProgress( const DeletePlanEntry& entry, LocationOperationProgress& progress );
        void SetOperationStage( DownloadLocationOperationStage stage, const std::string& message = { } );
        void WaitForMinimumOperationStep( std::chrono::steady_clock::time_point startedAt );
        DownloadLocationOperationStage GetOperationStage( ) const;
        LocationOperationProgress GetOperationProgressSnapshot( ) const;
        void UpdateOperationProgress( const LocationOperationProgress& progress );
        std::string FormatBytes( uintmax_t bytes ) const;
        float CalculateProgressFraction( const LocationOperationProgress& progress ) const;
        bool IsDownloadRootAvailableOrCreatable( const std::filesystem::path& downloadRoot ) const;
        std::string FormatPathForDisplay( const std::filesystem::path& path ) const;
        static std::optional<std::filesystem::path> PickFolder( const std::filesystem::path& initialPath );

        std::shared_ptr<JsonFile> m_AppConfig{ };
        std::string m_CaBundlePath{ };
        std::filesystem::path m_DefaultDownloadRoot{ };
        std::filesystem::path m_DownloadRoot{ };
        std::filesystem::path m_PendingDownloadRoot{ };
        std::filesystem::path m_OperationPreviousRoot{ };
        std::filesystem::path m_OperationNextRoot{ };
        DownloadRootChangedCallback m_OnDownloadRootChanged{ };
        mutable std::mutex m_OperationMutex{ };
        std::future<void> m_OperationFuture{ };
        std::vector<CopyPlanEntry> m_CopyPlan{ };
        std::atomic_bool m_CancelLocationOperation{ false };
        DownloadLocationOperationStage m_LocationOperationStage{ DownloadLocationOperationStage::Idle };
        LocationOperationProgress m_LocationOperationProgress{ };
        bool m_CheckUpdatesOnStartup{ false };
        bool m_StartupCheckHandled{ false };
        bool m_CheckRunning{ false };
        bool m_ShowUpdatePopup{ false };
        bool m_Blocked{ false };
        bool m_DownloadLocationResolved{ true };
        bool m_ShowMissingDownloadLocationPopup{ false };
        bool m_ShowMoveDownloadsPopup{ false };
        SettingsUpdateStatus m_UpdateStatus{ SettingsUpdateStatus::NotChecked };
        GitHubReleaseInfo m_LatestRelease{ };
        std::string m_LastError{ };
        std::string m_LastUpdatePromptedVersion{ };
        std::string m_SkippedUpdateVersion{ };
        std::string m_DownloadLocationError{ };

#ifdef _DEBUG
        bool m_CheckDevelopmentVersion{ false };
#endif
    };
}
