#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3_loader.h"
#include "imgui/imgui_impl_opengl3.h"

#include "GLFW/glfw3.h"

#include "log/Logger.h"
#include "services/Service.h"
#include "services/IServiceSink.h"
#include "ui/IUiPanel.h"
#include "ui/LogPanel.h"
#include "ui/DownloadOptionsPanel.h"
#include "ui/DownloadProgressPanel.h"
#include "ui/MediaPreviewPanel.h"
#include "ui/DownloadHistoryPanel.h"
#include "ui/CredentialsPanel.h"
#include "ui/MediaPreviewControlPanel.h"
#include "io/JsonFile.h"

#include <stdio.h>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace ImageScraper
{
    static void GLFW_ErrorCallback( int error, const char* description )
    {
        LogError( "[%s] GLFW Error %d: %s", error, description );
    }

    class FrontEnd : public IServiceSink
    {
    public:
        FrontEnd( int maxLogLines );
        ~FrontEnd( );

        bool Init( const std::vector<std::shared_ptr<Service>>& services, std::shared_ptr<JsonFile> userConfig, std::shared_ptr<JsonFile> appConfig );
        void Update( );
        void SetInputState( InputState state );
        void Render( );
        void Log( const LogLine& line );
        void SetLogFilePath( const std::string& path );

        GLFWwindow* GetWindow( ) const { return m_WindowPtr; }
        LogLevel    GetLogLevel( ) const;

        // IServiceSink
        bool IsCancelled( ) override;
        void OnRunComplete( ) override;
        void OnCurrentDownloadProgress( float progress ) override;
        void OnTotalDownloadProgress( int current, int total ) override;
        int  GetSigningInProvider( ) override;
        void OnSignInComplete( ContentProvider provider ) override;
        void OnFileDownloaded( const std::string& filepath, const std::string& sourceUrl ) override;

    private:
        void ShowDemoWindow( );
        void SetupDefaultLayout( ImGuiID dockspaceId );

        GLFWwindow* m_WindowPtr{ nullptr };
        int         m_MaxLogLines{ 0 };
        std::string m_IniPath{ };
        bool        m_LayoutInitialised{ false };

        std::unique_ptr<LogPanel>              m_LogPanel{ };
        std::unique_ptr<DownloadOptionsPanel>  m_DownloadOptionsPanel{ };
        std::unique_ptr<DownloadProgressPanel> m_DownloadProgressPanel{ };
        std::unique_ptr<MediaPreviewPanel>          m_MediaPreviewPanel{ };
        std::unique_ptr<DownloadHistoryPanel>       m_DownloadHistoryPanel{ };
        std::unique_ptr<CredentialsPanel>           m_CredentialsPanel{ };
        std::unique_ptr<MediaPreviewControlPanel>   m_MediaPreviewControlPanel{ };
    };
}
