#pragma once

#include "ui/IUiPanel.h"
#include "ui/MediaPreviewPanel.h"
#include "ui/DownloadHistoryPanel.h"

struct ImFont;

namespace ImageScraper
{
    class MediaPreviewControlPanel : public IUiPanel
    {
    public:
        MediaPreviewControlPanel( MediaPreviewPanel* previewPanel,
                                  DownloadHistoryPanel* historyPanel,
                                  ImFont* iconFont );
        void Update( ) override;

    private:
        MediaPreviewPanel*    m_PreviewPanel{ nullptr };
        DownloadHistoryPanel* m_HistoryPanel{ nullptr };
        ImFont*               m_IconFont{ nullptr };
    };
}
