#pragma once

#include "ui/IUiPanel.h"
#include "imgui/imgui.h"

#include <atomic>

namespace ImageScraper
{
    class DownloadProgressPanel : public IUiPanel
    {
    public:
        void Update( ) override;

        void OnCurrentDownloadProgress( float progress );
        void OnTotalDownloadProgress( int current, int total );
        void SetRunning( bool running );

    private:
        std::atomic<float> m_CurrentDownloadProgress{ 0.f };
        std::atomic<float> m_TotalProgress{ 0.f };
        std::atomic_int    m_CurrentDownloadNum{ 0 };
        std::atomic_int    m_TotalDownloadsCount{ 0 };
        bool               m_Running{ false };
    };
}
