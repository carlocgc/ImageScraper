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

#include "services/Service.h"
#include "config/Config.h"

static void glfw_error_callback( int error, const char* description )
{
    fprintf( stderr, "GLFW Error %d: %s\n", error, description );
}

namespace ImageScraper
{
    enum class InputState : uint8_t
    {
        Free = 0,
        Blocked,
    };

    class FrontEnd
    {
    public:
        FrontEnd( std::shared_ptr<Config> config );
        ~FrontEnd( );

        bool Init( );
        bool GetUserInput( std::string& out );
        void Update( );
        bool HandleUserInput( std::vector<std::shared_ptr<Service>>& services );
        void SetInputState( const InputState& state );
        void Render( );
        GLFWwindow* GetWindow( ) { return m_WindowPtr; };
    private:
        void UpdateWidgets( );

        std::shared_ptr<Config> m_Config{ nullptr };
        GLFWwindow* m_WindowPtr{ nullptr };

        InputState m_InputState{ InputState::Free };
        std::string m_UrlField{ };
        bool m_StartProcess{ false };
    };
}



