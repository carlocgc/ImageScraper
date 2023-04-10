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

#include "collections/RingBuffer.h"
#include "log/Logger.h"

namespace ImageScraper
{
    static void GLFW_ErrorCallback( int error, const char* description )
    {
        ErrorLog( "[%s] GLFW Error %d: %s", error, description );
    }

    class Config;
    class Service;

    enum class InputState : uint8_t
    {
        Free,
        Blocked,
    };

    class FrontEnd
    {
    public:
        FrontEnd( std::shared_ptr<Config> config, int maxLogLines );
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
        void UpdateUrlInput( );
        void UpdateLogWindow( );

        std::shared_ptr<Config> m_Config{ nullptr };
        GLFWwindow* m_WindowPtr{ nullptr };

        InputState m_InputState{ InputState::Free };

        // TODO Move all input logic into components

        // Url input
        std::string m_UrlField{ };
        bool m_StartProcess{ false };

        // Log
        RingBuffer<LogLine> m_LogContent;
        bool m_AutoScroll{ true };
        bool m_DebugLogging{ false };
        bool m_ScrollToBottom{ false };
        ImGuiTextFilter m_Filter;
        LogLevel m_LogLevel{ LogLevel::Error };
    };
}



