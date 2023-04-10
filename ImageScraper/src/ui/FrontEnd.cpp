#include "ui/FrontEnd.h"
#include "log/Logger.h"
#include "services/Service.h"
#include "config/Config.h"

ImageScraper::FrontEnd::FrontEnd( std::shared_ptr<Config> config, int maxLogLines )
    : m_Config{ config }
    , m_LogContent{ maxLogLines }
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

    ImGui::BeginDisabled( m_InputState == InputState::Blocked );

    ShowDemoWindow( );
    UpdateUrlInput( );

    ImGui::EndDisabled( );

    UpdateLogWindow( );
}

bool ImageScraper::FrontEnd::HandleUserInput( std::vector<std::shared_ptr<Service>>& services )
{
    if( m_InputState == InputState::Blocked )
    {
        return false;
    }

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
        if( service->HandleUrl( *m_Config, *this, m_UrlField ) )
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

void ImageScraper::FrontEnd::AppendLogContent( const std::string& line )
{
    m_LogContent.Push( line );
}

void ImageScraper::FrontEnd::ShowDemoWindow( )
{
    bool show = true;
    ImGui::ShowDemoWindow( &show );
}

void ImageScraper::FrontEnd::UpdateUrlInput( )
{
    char url[ 64 ] = "";
    if( ImGui::InputText( "Sub reddit Name", url, 64 ) )
    {
        m_UrlField = url;
    }

    m_StartProcess = ImGui::Button( "Start", ImVec2( 50, 25 ) );
}

void ImageScraper::FrontEnd::UpdateLogWindow( )
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 5.0f );
    ImGui::BeginChild( "OutputHeader", ImVec2( 0, 260 ), true, window_flags );
    if( ImGui::BeginMenuBar( ) )
    {
        if( ImGui::BeginMenu( "Output" ) )
        {
            ImGui::EndMenu( );
        }
        ImGui::EndMenuBar( );
    }
    ImGui::BeginChild( "OutputContent", ImVec2( ImGui::GetContentRegionAvail( ).x * 0.5f, 260 ), false, window_flags );
    for( int i = 0; i < m_LogContent.GetSize( ); i++ )
    {
        ImGui::Text( m_LogContent[ i ].c_str( ) );
    }
    ImGui::EndChild( );
    ImGui::EndChild( );
    ImGui::PopStyleVar( );
}

void ImageScraper::FrontEnd::SetInputState( const InputState& state )
{
    m_InputState = state;
}
