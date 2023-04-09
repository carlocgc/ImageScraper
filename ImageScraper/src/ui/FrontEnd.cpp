#include "ui/FrontEnd.h"
#include "log/Logger.h"

ImageScraper::FrontEnd::FrontEnd( std::shared_ptr<Config> config )
    : m_Config{ config }
{
}

ImageScraper::FrontEnd::~FrontEnd( )
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown( );
    ImGui_ImplGlfw_Shutdown( );
    ImGui::DestroyContext( );

    glfwDestroyWindow( m_WindowPtr );
    glfwTerminate( );
}

bool ImageScraper::FrontEnd::Init( )
{
    glfwSetErrorCallback( glfw_error_callback );
    if( !glfwInit( ) )
    {
        return false;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );

    // Create window with graphics context
    m_WindowPtr = glfwCreateWindow( 1280, 720, "Image Scraper", NULL, NULL );
    if( m_WindowPtr == NULL )
    {
        return false;
    }

    glfwMakeContextCurrent( m_WindowPtr );
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
    ImGui_ImplGlfw_InitForOpenGL( m_WindowPtr, true );
    ImGui_ImplOpenGL3_Init( glsl_version );

    return true;
}

bool ImageScraper::FrontEnd::GetUserInput( std::string& out )
{
    return false;
}

void ImageScraper::FrontEnd::Update( )
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

    UpdateWidgets( );
}

bool ImageScraper::FrontEnd::HandleUserInput( std::vector<std::shared_ptr<Service>>& services )
{
    if( !m_StartProcess )
    {
        return false;
    }

    if( m_UrlField.empty( ) )
    {
        return false;
    }

    InfoLog( "[%s] Processing input: %s", __FUNCTION__, m_UrlField.c_str( ) );

    if( m_UrlField == "exit" )
    {
        InfoLog( "[%s] Exiting...", __FUNCTION__ );
        return true;
    }

    for( auto service : services )
    {
        if( service->HandleUrl( *m_Config, m_UrlField ) )
        {
            m_UrlField.clear( );
            return true;
        }
        else
        {
            ErrorLog( "[%s] Invalid Url: %s", __FUNCTION__, m_UrlField );
            //TODO Show error in UI widgets
        }
    }

    return false;
}

void ImageScraper::FrontEnd::Render( )
{
    ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );

    // Rendering
    ImGui::Render( );
    int display_w, display_h;
    glfwGetFramebufferSize( m_WindowPtr, &display_w, &display_h );
    glViewport( 0, 0, display_w, display_h );
    glClearColor( clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w );
    glClear( GL_COLOR_BUFFER_BIT );
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData( ) );

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(m_WindowPtr) directly)
    auto io = ImGui::GetIO( );
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext( );
        ImGui::UpdatePlatformWindows( );
        ImGui::RenderPlatformWindowsDefault( );
        glfwMakeContextCurrent( backup_current_context );
    }

    glfwSwapBuffers( m_WindowPtr );
}

void ImageScraper::FrontEnd::UpdateWidgets( )
{
    ImGui::BeginDisabled( m_InputState == InputState::Blocked );

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    bool show_demo_window = false;
    if( show_demo_window )
    {
        ImGui::ShowDemoWindow( &show_demo_window );
    }

    char url[ 64 ] = "";
    if( ImGui::InputText( "default", url, 64 ) )
    {
        m_UrlField = url;
    }

    m_StartProcess = ImGui::Button( "Start", ImVec2( 50, 25 ) );

    ImGui::EndDisabled( );
}

void ImageScraper::FrontEnd::SetInputState( const InputState& state )
{
    m_InputState = state;
}
