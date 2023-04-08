#include "app/App.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3_loader.h"
#include "imgui/imgui_impl_opengl3.h"

#include "GLFW/glfw3.h"

#include "services/RedditService.h"
#include "log/Logger.h"

static void glfw_error_callback( int error, const char* description )
{
    fprintf( stderr, "GLFW Error %d: %s\n", error, description );
}

App::App( )
{
    m_Services.push_back( std::make_shared<RedditService>( ) );
}

int App::Run( )
{
    glfwSetErrorCallback( glfw_error_callback );
    if( !glfwInit( ) )
    {
        return 1;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow( 1280, 720, "Image Scraper", NULL, NULL );
    if( window == NULL )
    {
        return 1;
    }

    glfwMakeContextCurrent( window );
    glfwSwapInterval( 1 ); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION( );
    ImGui::CreateContext( );
    ImGuiIO& io = ImGui::GetIO( ); ( void )io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark( );

    // When view ports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle( );
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        style.WindowRounding = 0.0f;
        style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
    }

    // Setup Platform/Renderer back ends
    ImGui_ImplGlfw_InitForOpenGL( window, true );
    ImGui_ImplOpenGL3_Init( glsl_version );

    while( !glfwWindowShouldClose( window ) ) // main loop
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents( );

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame( );
        ImGui_ImplGlfw_NewFrame( );
        ImGui::NewFrame( );

        std::string input;

        if( m_UI.GetUserInput( input ) )
        {
            if( input == "exit" )
            {
                InfoLog( "[%s] Exiting...", __FUNCTION__ );
                return 0;
            }

            InfoLog( "[%s] Processing input: %s", __FUNCTION__, input.c_str( ) );

            for( auto service : m_Services )
            {
                if( service->HandleUrl( m_Config, input ) )
                {
                    break;
                }
            }
        }

        // Check main thread queue for callbacks

        // Rendering
        ImGui::Render( );
        int display_w, display_h;
        glfwGetFramebufferSize( window, &display_w, &display_h );
        glViewport( 0, 0, display_w, display_h );
        ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );
        glClearColor( clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w );
        glClear( GL_COLOR_BUFFER_BIT );
        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData( ) );

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext( );
            ImGui::UpdatePlatformWindows( );
            ImGui::RenderPlatformWindowsDefault( );
            glfwMakeContextCurrent( backup_current_context );
        }

        glfwSwapBuffers( window );
    }

    return 0;
}
