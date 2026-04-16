#include "ui/FrontEnd.h"
#include "ui/DownloadProgressPanel.h"
#include "log/Logger.h"

#include "imgui/imgui_internal.h"

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

bool ImageScraper::FrontEnd::Init( const std::vector<std::shared_ptr<Service>>& services, std::shared_ptr<JsonFile> userConfig )
{
    m_DownloadOptionsPanel  = std::make_unique<DownloadOptionsPanel>( services );
    m_DownloadProgressPanel = std::make_unique<DownloadProgressPanel>( );
    m_MediaPreviewPanel     = std::make_unique<MediaPreviewPanel>( );
    m_DownloadHistoryPanel  = std::make_unique<DownloadHistoryPanel>(
        [ this ]( const std::string& filepath ) { m_MediaPreviewPanel->RequestPreview( filepath ); } );
    m_CredentialsPanel      = std::make_unique<CredentialsPanel>( userConfig );

    glfwSetErrorCallback( GLFW_ErrorCallback );
    if( !glfwInit( ) )
    {
        return false;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );

    m_WindowPtr = glfwCreateWindow( 1600, 900, "Image Scraper", NULL, NULL );
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

    char exePath[ MAX_PATH ];
    GetModuleFileNameA( nullptr, exePath, MAX_PATH );
    m_IniPath = ( std::filesystem::path( exePath ).parent_path( ) / "imgui.ini" ).string( );
    io.IniFilename = m_IniPath.c_str( );

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

    const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport( );

    if( !m_LayoutInitialised )
    {
        m_LayoutInitialised = true;
        // Only build the default layout on a genuinely fresh run.
        // If imgui.ini already exists ImGui restores the saved layout itself
        // and we must not overwrite it.
        if( !std::filesystem::exists( m_IniPath ) )
        {
            SetupDefaultLayout( dockspaceId );
        }
    }

    ShowDemoWindow( );

    const bool wasRunning = m_DownloadOptionsPanel->IsRunning( );
    m_DownloadOptionsPanel->Update( );
    if( !wasRunning && m_DownloadOptionsPanel->IsRunning( ) )
    {
        m_LogPanel->SetRunning( true );
        m_DownloadProgressPanel->SetRunning( true );
    }

    m_LogPanel->Update( );
    m_DownloadProgressPanel->Update( );
    m_MediaPreviewPanel->Update( );
    m_DownloadHistoryPanel->Update( );
    m_CredentialsPanel->Update( );
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

void ImageScraper::FrontEnd::SetupDefaultLayout( ImGuiID dockspaceId )
{
    ImGui::DockBuilderRemoveNode( dockspaceId );
    ImGui::DockBuilderAddNode( dockspaceId, ImGuiDockNodeFlags_DockSpace );
    ImGui::DockBuilderSetNodeSize( dockspaceId, ImGui::GetMainViewport( )->Size );

    // Carve off a narrow strip at the bottom for download progress
    ImGuiID dockTop, dockBottom;
    ImGui::DockBuilderSplitNode( dockspaceId, ImGuiDir_Down, 80.0f / 900.0f, &dockBottom, &dockTop );

    // Split the remaining area into left and right columns
    ImGuiID dockLeft, dockRight;
    ImGui::DockBuilderSplitNode( dockTop, ImGuiDir_Left, 732.0f / 1600.0f, &dockLeft, &dockRight );

    // Split the left column into top (options) and bottom (log/history)
    ImGuiID dockTopLeft, dockBottomLeft;
    ImGui::DockBuilderSplitNode( dockLeft, ImGuiDir_Up, 350.0f / 841.0f, &dockTopLeft, &dockBottomLeft );

    // Dock all panels - order within a node does not reliably control tab selection
    ImGui::DockBuilderDockWindow( "Download Options", dockTopLeft );
    ImGui::DockBuilderDockWindow( "Credentials",      dockTopLeft );
    ImGui::DockBuilderDockWindow( "Output",           dockBottomLeft );
    ImGui::DockBuilderDockWindow( "Download History", dockBottomLeft );
    ImGui::DockBuilderDockWindow( "Media Preview",    dockRight );
    ImGui::DockBuilderDockWindow( "Download Progress", dockBottom );

#ifdef _DEBUG
    ImGui::DockBuilderDockWindow( "Dear ImGui Demo", dockRight );
#endif

    ImGui::DockBuilderFinish( dockspaceId );

    // Explicitly select the default visible tab in each multi-window node.
    // DockBuilderDockWindow dock order does not reliably control this.
    auto selectTab = []( ImGuiID nodeId, const char* windowName )
    {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode( nodeId );
        if( node )
            node->SelectedTabId = ImHashStr( windowName );
    };

    selectTab( dockTopLeft,    "Credentials" );
    selectTab( dockBottomLeft, "Download History" );
    selectTab( dockRight,      "Media Preview" );
}

void ImageScraper::FrontEnd::ShowDemoWindow( )
{
#ifdef _DEBUG
    bool show = true;
    ImGui::ShowDemoWindow( &show );
#endif
}

// IServiceSink

bool ImageScraper::FrontEnd::IsCancelled( )
{
    return m_DownloadOptionsPanel->IsCancelled( );
}

void ImageScraper::FrontEnd::OnRunComplete( )
{
    m_DownloadOptionsPanel->OnRunComplete( );
    m_LogPanel->SetRunning( false );
    m_DownloadProgressPanel->SetRunning( false );
}

void ImageScraper::FrontEnd::OnCurrentDownloadProgress( float progress )
{
    m_DownloadProgressPanel->OnCurrentDownloadProgress( progress );
}

void ImageScraper::FrontEnd::OnTotalDownloadProgress( int current, int total )
{
    m_DownloadProgressPanel->OnTotalDownloadProgress( current, total );
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
