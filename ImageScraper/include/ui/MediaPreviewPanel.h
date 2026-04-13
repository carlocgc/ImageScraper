#pragma once

#include "ui/IUiPanel.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include <string>
#include <vector>
#include <mutex>

namespace ImageScraper
{
    class MediaPreviewPanel : public IUiPanel
    {
    public:
        ~MediaPreviewPanel( );
        void Update( ) override;

        // Thread-safe — may be called from a worker thread
        void OnFileDownloaded( const std::string& filepath );

    private:
        void LoadPending( );
        void LoadStaticImage( const unsigned char* data, int width, int height );
        void LoadGif( const unsigned char* data, int width, int height, int frames, const int* delays );
        void FreeTextures( );
        bool IsGif( const std::string& filepath ) const;

        std::vector<GLuint> m_Textures{ };
        std::vector<int>    m_FrameDelaysMs{ };
        int   m_Width{ 0 };
        int   m_Height{ 0 };
        int   m_CurrentFrame{ 0 };
        float m_FrameAccumMs{ 0.0f };

        std::string m_CurrentFilePath{ };

        std::mutex  m_PendingMutex{ };
        std::string m_PendingFilePath{ };
        bool        m_HasPending{ false };
    };
}
