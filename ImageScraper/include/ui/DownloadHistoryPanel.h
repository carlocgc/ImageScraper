#pragma once

#include "ui/IUiPanel.h"
#include "collections/RingBuffer.h"
#include "imgui/imgui.h"

#include <string>
#include <mutex>

namespace ImageScraper
{
    struct DownloadHistoryEntry
    {
        std::string m_FilePath;
        std::string m_FileName;
        std::string m_SourceUrl;
        std::string m_Timestamp;
    };

    class DownloadHistoryPanel : public IUiPanel
    {
    public:
        DownloadHistoryPanel( );
        void Update( ) override;

        // Thread-safe — may be called from a worker thread
        void OnFileDownloaded( const std::string& filepath, const std::string& sourceUrl );

    private:
        void FlushPending( );
        static void OpenInExplorer( const std::string& filepath );
        static std::string FormatTimestamp( );
        static std::string ExtractFileName( const std::string& filepath );

        static constexpr int k_Capacity = 200;

        RingBuffer<DownloadHistoryEntry> m_History{ k_Capacity };

        std::mutex                              m_PendingMutex{ };
        std::vector<DownloadHistoryEntry>       m_Pending{ };
    };
}
