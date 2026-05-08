#pragma once

#include <chrono>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ImageScraper
{
    class DownloadDeleteController
    {
    public:
        struct Request
        {
            std::filesystem::path m_TargetPath{ };
            bool                  m_IsDirectory{ false };
        };

        struct Progress
        {
            uintmax_t   m_TotalBytes{ 0 };
            uintmax_t   m_ProcessedBytes{ 0 };
            int         m_TotalEntries{ 0 };
            int         m_ProcessedEntries{ 0 };
            std::string m_CurrentPath{ };
            std::string m_Message{ };
            std::string m_Error{ };
        };

        enum class Stage
        {
            Idle,
            Scanning,
            Deleting,
        };

        struct Result
        {
            bool        m_Success{ false };
            std::string m_ErrorMessage{ };
        };

        ~DownloadDeleteController( );

        bool IsActive( ) const;
        bool HasCompletedWork( ) const;
        void Start( const Request& request );
        void Update( );
        std::optional<Result> TryConsumeCompletedResult( std::chrono::steady_clock::time_point now );
        Stage GetStage( ) const;
        Progress GetProgressSnapshot( ) const;
        float GetModalProgressFraction( std::chrono::steady_clock::time_point now ) const;

        static float CalculateWorkProgressFraction( uintmax_t totalBytes, uintmax_t processedBytes, int totalEntries, int processedEntries );
        static float CalculateModalProgressFraction(
            uintmax_t totalBytes,
            uintmax_t processedBytes,
            int totalEntries,
            int processedEntries,
            std::chrono::steady_clock::time_point startedAt,
            std::chrono::steady_clock::time_point visibleUntil,
            std::chrono::steady_clock::time_point now,
            bool deleteWorkStarted,
            bool deleteReady );
        static bool ShouldKeepProgressVisible(
            std::chrono::steady_clock::time_point visibleUntil,
            std::chrono::steady_clock::time_point now,
            bool deleteReady );

    private:
        struct DeleteOperationEntry
        {
            std::filesystem::path m_Path{ };
            uintmax_t             m_SizeBytes{ 0 };
            bool                  m_IsDirectory{ false };
        };

        void BuildDeletePlan(
            const std::filesystem::path& path,
            std::vector<DeleteOperationEntry>& deletePlan,
            Progress& progress );
        bool ExecuteDeletePlanEntry( const DeleteOperationEntry& entry, Progress& progress );
        void SetStage( Stage stage, const std::string& message );
        void UpdateProgress( const Progress& progress );

        mutable std::mutex                   m_StateMutex{ };
        std::future<std::pair<bool, std::string>> m_DeleteFuture{ };
        std::optional<std::pair<bool, std::string>> m_DeleteCompletedResult{ };
        Stage                               m_Stage{ Stage::Idle };
        Progress                            m_Progress{ };
        std::chrono::steady_clock::time_point m_StartedAt{ };
        std::chrono::steady_clock::time_point m_VisibleUntil{ };
    };
}
