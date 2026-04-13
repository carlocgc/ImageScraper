#include "ui/FrontEnd.h"
#include "log/Logger.h"

ImageScraper::FrontEnd::FrontEnd( int maxLogLines )
    : m_MaxLogLines{ maxLogLines }
{
    m_LogPanel = std::make_unique<LogPanel>( maxLogLines );
}

ImageScraper::FrontEnd::~FrontEnd( )
{
    ImGui_ImplOpenGL3_Shutdown( );
    ImGui_ImplGlfw_Shutdown( );
    ImGui::DestroyContext( );

    glfwDestroyWindow( m_WindowPtr );
    glfwTerminate( );
}

bool ImageScraper::FrontEnd::Init( const std::vector<std::shared_ptr<Service>>& services )
{
    m_DownloadOptionsPanel = std::make_unique<DownloadOptionsPanel>( services );
    m_MediaPreviewPanel    = std::make_unique<MediaPreviewPanel>( );
    m_DownloadHistoryPanel = std::make_unique<DownloadHistoryPanel>( );

    glfwSetErrorCallback( GLFW_ErrorCallback );
    if( !glfwInit( ) )
    {
        return false;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );

    m_WindowPtr = glfwCreateWindow( 1280, 720, "Image Scraper", NULL, NULL );
    if( m_WindowPtr == NULL )
    {
        return false;
    }

    glfwMakeContextCurrent( m_WindowPtr );
    glfwSwapInterval( 1 );

    IMGUI_CHECKVERSION( );
    ImGui::CreateContext( );
    ImGuiIO& io = ImGui::GetIO( ); ( void )io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark( );

    ImGuiStyle& style = ImGui::GetStyle( );
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        style.WindowRounding = 0.0f;
        style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL( m_WindowPtr, true );
    ImGui_ImplOpenGL3_Init( glsl_version );

    return true;
}

void ImageScraper::FrontEnd::Update( )
{
    glfwPollEvents( );

    ImGui_ImplOpenGL3_NewFrame( );
    ImGui_ImplGlfw_NewFrame( );
    ImGui::NewFrame( );

    ImGui::DockSpaceOverViewport( );

    ShowDemoWindow( );

    const bool wasRunning = m_DownloadOptionsPanel->IsRunning( );
    m_DownloadOptionsPanel->Update( );
    if( !wasRunning && m_DownloadOptionsPanel->IsRunning( ) )
    {
        m_LogPanel->SetRunning( true );
    }

    m_LogPanel->Update( );
    m_MediaPreviewPanel->Update( );
    m_DownloadHistoryPanel->Update( );
}

void ImageScraper::FrontEnd::Render( )
{
    ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );

    ImGui::Render( );
    int display_w, display_h;
    glfwGetFramebufferSize( m_WindowPtr, &display_w, &display_h );
    glViewport( 0, 0, display_w, display_h );
    glClearColor( clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w );
    glClear( GL_COLOR_BUFFER_BIT );
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData( ) );

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

void ImageScraper::FrontEnd::Log( const LogLine& line )
{
    m_LogPanel->Log( line );
}

void ImageScraper::FrontEnd::SetInputState( InputState state )
{
    m_DownloadOptionsPanel->SetInputState( state );
    if( state == InputState::Free )
    {
        m_LogPanel->SetRunning( false );
    }
}

ImageScraper::LogLevel ImageScraper::FrontEnd::GetLogLevel( ) const
{
    return m_LogPanel->GetLogLevel( );
}

void ImageScraper::FrontEnd::ShowDemoWindow( )
{
    bool show = true;
    ImGui::ShowDemoWindow( &show );
}

// IServiceSink

bool ImageScraper::FrontEnd::IsCancelled( )
{
    return m_DownloadOptionsPanel->IsCancelled( );
}

void ImageScraper::FrontEnd::OnRunComplete( )
{
    SetInputState( InputState::Free );
}

void ImageScraper::FrontEnd::OnCurrentDownloadProgress( float progress )
{
    m_LogPanel->OnCurrentDownloadProgress( progress );
}

void ImageScraper::FrontEnd::OnTotalDownloadProgress( int current, int total )
{
    m_LogPanel->OnTotalDownloadProgress( current, total );
}

int ImageScraper::FrontEnd::GetSigningInProvider( )
{
    return m_DownloadOptionsPanel->GetSigningInProvider( );
}

void ImageScraper::FrontEnd::OnSignInComplete( ContentProvider provider )
{
    m_DownloadOptionsPanel->OnSignInComplete( provider );
}

void ImageScraper::FrontEnd::OnFileDownloaded( const std::string& filepath, const std::string& sourceUrl )
{
    m_MediaPreviewPanel->OnFileDownloaded( filepath );
    m_DownloadHistoryPanel->OnFileDownloaded( filepath, sourceUrl );
}
