#pragma once

#include "ui/IUiPanel.h"
#include "ui/MediaPreviewPanel.h"
#include "ui/DownloadHistoryPanel.h"

#include <filesystem>

struct ImFont;
typedef unsigned int GLuint;

namespace ImageScraper
{
    class MediaPreviewControlPanel : public IUiPanel
    {
    public:
        struct IconTexture
        {
            GLuint m_Texture{ 0 };
            int    m_Width{ 0 };
            int    m_Height{ 0 };
        };

        ~MediaPreviewControlPanel( );
        MediaPreviewControlPanel( MediaPreviewPanel* previewPanel,
                                  DownloadHistoryPanel* historyPanel,
                                  ImFont* iconFont,
                                  const std::filesystem::path& assetRoot );
        void SetBlocked( bool blocked ) { m_Blocked = blocked; }
        void Update( ) override;

    private:
        void EnsureIconsLoaded( );
        void ReleaseIcons( );

        MediaPreviewPanel*    m_PreviewPanel{ nullptr };
        DownloadHistoryPanel* m_HistoryPanel{ nullptr };
        ImFont*               m_IconFont{ nullptr };
        std::filesystem::path m_AssetRoot{ };
        bool                  m_Blocked{ false };

        IconTexture           m_PreviousIcon{ };
        IconTexture           m_NextIcon{ };
        IconTexture           m_PlayIcon{ };
        IconTexture           m_PauseIcon{ };
        IconTexture           m_MutedIcon{ };
        IconTexture           m_VolumeIcon{ };
        IconTexture           m_FullscreenIcon{ };
        bool                  m_IconsAttempted{ false };

        // Hover-popup volume slider state
        bool                  m_VolumePopupOpen{ false };
        double                m_VolumeHoverLeftAt{ 0.0 };  // ImGui::GetTime() of last "all hovers lost"; 0 means currently hovered
    };
}
