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

#include "log/Logger.h"
#include "collections/RingBuffer.h"
#include "services/Service.h"

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
        FrontEnd( int maxLogLines );
        ~FrontEnd( );

        bool Init( );
        bool GetUserInput( std::string& out );
        void Update( );
        bool HandleUserInput( std::vector<std::shared_ptr<Service>>& services );
        void SetInputState( const InputState& state );
        void Render( );
        void Log( const LogLine& line );
        GLFWwindow* GetWindow( ) const { return m_WindowPtr; };
        LogLevel GetLogLevel( ) const { return m_LogLevel; };

        int InputTextCallback( ImGuiInputTextCallbackData* data );
        static int InputTextCallbackProxy( ImGuiInputTextCallbackData* data )
        {
            FrontEnd* frontEnd = reinterpret_cast< FrontEnd* >( data->UserData );
            return frontEnd->InputTextCallback( data );
        }

    private:
        void ShowDemoWindow( );

        // TODO Move all input logic into components
        void UpdateProviderWidgets( );
        void UpdateRedditWidgets( );
        void UpdateTwitterWidgets( );
        void UpdateRunButtonWidget( );
        void UpdateLogWindowWidgets( );

        UserInputOptions BuildRedditInputOptions( );
        UserInputOptions BuildTwitterInputOptions( );

        void ResetInputFields( );

        GLFWwindow* m_WindowPtr{ nullptr };
        InputState m_InputState{ InputState::Free };

        // Generic options
        int m_ContentProvider{ };
        bool m_StartProcess{ false };

        // Reddit options
        std::string m_SubredditName{ };
        RedditScope m_RedditScope{ RedditScope::Hot };
        RedditScopeTimeFrame m_RedditScopeTimeFrame{ RedditScopeTimeFrame::All };
        int m_RedditLimit{ };

        // Twitter options
        std::string m_TwitterHandle{ };

        // Log window
        RingBuffer<LogLine> m_LogContent;
        ImGuiTextFilter m_Filter;
        LogLevel m_LogLevel{ LogLevel::Error };
        bool m_AutoScroll{ true };
        bool m_DebugLogging{ false };
        bool m_ScrollToBottom{ false };
    };
}




