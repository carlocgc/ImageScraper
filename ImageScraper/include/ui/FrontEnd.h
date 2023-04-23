#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3_loader.h"
#include "imgui/imgui_impl_opengl3.h"

#include "GLFW/glfw3.h"

#include <stdio.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

#include "log/Logger.h"
#include "collections/RingBuffer.h"
#include "services/Service.h"
#include <deque>

#define INVALID_CONTENT_PROVIDER -1

namespace ImageScraper
{
    static void GLFW_ErrorCallback( int error, const char* description )
    {
        ErrorLog( "[%s] GLFW Error %d: %s", error, description );
    }

    enum class InputState : uint8_t
    {
        Free,
        Blocked,
    };

    class FrontEnd
    {
    public:
        FrontEnd( const int maxLogLines );
        ~FrontEnd( );

        bool Init( const std::vector<std::shared_ptr<Service>>& services );
        void Update( );
        void SetInputState( const InputState& state );
        void Render( );
        void Log( const LogLine& line );
        bool IsCancelled( ) { return m_DownloadCancelled.load( ); }
        void UpdateCurrentDownloadProgress( const float progress );
        void UpdateTotalDownloadsProgress( const int current, const int total );
        void CompleteSignIn( ContentProvider provider );

        GLFWwindow* GetWindow( ) const { return m_WindowPtr; };
        LogLevel GetLogLevel( ) const { return m_LogLevel; };
        int GetSigningInProvider( ) { return m_SigningInProvider.load(); };

    private:
        // TODO Move all input logic into components
        void ShowDemoWindow( );
        void UpdateProviderOptionsWindow( );
        void UpdateProviderWidgets( );
        void UpdateRedditWidgets( );
        void UpdateTumblrWidgets( );
        void UpdateFourChanWidgets( );
        void UpdateSignInButton( );
        void UpdateRunCancelButton( );
        void UpdateLogWindow( );

        UserInputOptions BuildRedditInputOptions( );
        UserInputOptions BuildTumblrInputOptions( );
        UserInputOptions BuildFourChanInputOptions( );

        bool HandleUserInput( );
        void Reset( );
        bool CanSignIn( ) const;
        void CancelSignIn( );
        std::shared_ptr<Service> GetCurrentProvider( );

        GLFWwindow* m_WindowPtr{ nullptr };
        InputState m_InputState{ InputState::Free };
        std::vector<std::shared_ptr<Service>> m_Services{ };

        // Generic options
        int m_ContentProvider{ };
        bool m_StartProcess{ false };
        bool m_Running{ false };
        std::atomic_bool m_DownloadCancelled{ false };
        std::atomic<int> m_SigningInProvider{ INVALID_CONTENT_PROVIDER };

        // Reddit options
        std::string m_SubredditName{ };
        RedditScope m_RedditScope{ RedditScope::Hot };
        RedditScopeTimeFrame m_RedditScopeTimeFrame{ RedditScopeTimeFrame::All };
        int m_RedditMaxMediaItems{ REDDIT_LIMIT_DEFAULT };

        // Tumblr options
        std::string m_TumblrUser{ };

        // 4chan options
        std::string m_FourChanBoard{ };
        int m_FourChanMaxThreads{ FOURCHAN_THREAD_MAX };
        int m_FourChanMaxMediaItems{ FOURCHAN_MEDIA_DEFAULT };

        // Log window
        RingBuffer<LogLine> m_LogContent;
        ImGuiTextFilter m_Filter{ };
        LogLevel m_LogLevel{ LogLevel::Error };
        bool m_AutoScroll{ true };
        bool m_DebugLogging{ false };
        bool m_ScrollToBottom{ false };

        // Progress
        std::atomic<float> m_CurrentDownloadProgress{ 0.f };
        std::atomic<float> m_TotalProgress{ 0.f };
        std::atomic_int m_CurrentDownloadNum{ 0 };
        std::atomic_int m_TotalDownloadsCount{ 0 };
    };
}




