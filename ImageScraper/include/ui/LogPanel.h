#pragma once

#include "ui/IUiPanel.h"
#include "log/Logger.h"
#include "collections/RingBuffer.h"
#include "imgui/imgui.h"

#include <atomic>

namespace ImageScraper
{
    class LogPanel : public IUiPanel
    {
    public:
        LogPanel( int maxLogLines );

        void Update( ) override;

        void Log( const LogLine& line );
        void OnCurrentDownloadProgress( float progress );
        void OnTotalDownloadProgress( int current, int total );

        // Pass false to reset all progress state when a run ends
        void SetRunning( bool running );

        LogLevel GetLogLevel( ) const { return m_LogLevel; }

    private:
        RingBuffer<LogLine> m_LogContent;
        ImGuiTextFilter     m_Filter{ };
        LogLevel            m_LogLevel{ LogLevel::Error };
        bool                m_AutoScroll{ true };
        bool                m_DebugLogging{ false };
        bool                m_ScrollToBottom{ false };

        std::atomic<float> m_CurrentDownloadProgress{ 0.f };
        std::atomic<float> m_TotalProgress{ 0.f };
        std::atomic_int    m_CurrentDownloadNum{ 0 };
        std::atomic_int    m_TotalDownloadsCount{ 0 };
        bool               m_Running{ false };
    };
}
