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

static void glfw_error_callback( int error, const char* description )
{
    fprintf( stderr, "GLFW Error %d: %s\n", error, description );
}

namespace ImageScraper
{
    class Config;
    class Service;

    enum class InputState : uint8_t
    {
        Free = 0,
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
        void AppendLogContent( const std::string& line );
        GLFWwindow* GetWindow( ) { return m_WindowPtr; };

    private:
        void ShowDemoWindow( );
        void UpdateUrlInput( );
        void UpdateLogWindow( );

        std::shared_ptr<Config> m_Config{ nullptr };
        GLFWwindow* m_WindowPtr{ nullptr };

        InputState m_InputState{ InputState::Free };

        std::string m_UrlField{ };
        bool m_StartProcess{ false };

        RingBuffer<std::string> m_LogContent;
    };
}


