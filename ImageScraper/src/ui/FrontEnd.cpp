#include "ui/FrontEnd.h"
#include "ui/DownloadProgressPanel.h"
#include "log/Logger.h"

#include "imgui/imgui_internal.h"

namespace
{
    ImFont* LoadMediaControlsIconFont( ImGuiIO& io )
    {
        constexpr float k_IconFontSize = 36.0f;
        static const ImWchar k_IconRanges[] = {
            0xE74F, 0xE74F, // Mute
            0xE767, 0xE769, // Volume, Play, Pause
            0xE890, 0xE890, // View (privacy off)
            0xE892, 0xE893, // Previous, Next
            0xE992, 0xE992, // Volume0
            0xED1A, 0xED1A, // Hide (privacy on)
            0,
        };

        const std::filesystem::path fontPath{ "C:\\Windows\\Fonts\\segmdl2.ttf" };
        if( !std::filesystem::exists( fontPath ) )
        {
            ImageScraper::Logger::Log( ImageScraper::LogLevel::Warning,
                                       "[%s] Media control icon font not found: %s",
                                       __FUNCTION__,
                                       fontPath.string( ).c_str( ) );
            return nullptr;
        }

        ImFontConfig fontConfig{ };
        fontConfig.PixelSnapH = true;
        fontConfig.GlyphOffset = ImVec2( 0.0f, 1.0f );

        ImFont* iconFont = io.Fonts->AddFontFromFileTTF(
            fontPath.string( ).c_str( ),
            k_IconFontSize,
            &fontConfig,
            k_IconRanges );

        if( iconFont == nullptr )
        {
            ImageScraper::Logger::Log( ImageScraper::LogLevel::Warning,
                                       "[%s] Failed to load media control icon font from: %s",
                                       __FUNCTION__,
                                       fontPath.string( ).c_str( ) );
        }

        return iconFont;
    }
}

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

bool ImageScraper::FrontEnd::Init( const std::vector<std::shared_ptr<Service>>& services, std::shared_ptr<JsonFile> userConfig, std::shared_ptr<JsonFile> appConfig )
{
    char exePath[ MAX_PATH ];
    GetModuleFileNameA( nullptr, exePath, MAX_PATH );
    const std::filesystem::path exeDir = std::filesystem::path( exePath ).parent_path( );

    m_DownloadOptionsPanel  = std::make_unique<DownloadOptionsPanel>( services );
    m_DownloadProgressPanel = std::make_unique<DownloadProgressPanel>( );
    m_MediaPreviewPanel     = std::make_unique<MediaPreviewPanel>( );
    m_DownloadHistoryPanel  = std::make_unique<DownloadHistoryPanel>(
        [ this ]( const std::string& filepath ) { m_MediaPreviewPanel->RequestPreview( filepath ); },
        [ this ]( const std::string& filepath ) { m_MediaPreviewPanel->ReleaseFileIfCurrent( filepath ); } );
    m_CredentialsPanel      = std::make_unique<CredentialsPanel>( userConfig );
    m_DownloadHistoryPanel->Load( appConfig, exeDir / "Downloads" );
    m_DownloadOptionsPanel->LoadPanelState( appConfig );
    m_LogPanel->LoadPanelState( appConfig );
    m_MediaPreviewPanel->LoadPanelState( appConfig );

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

    io.Fonts->AddFontDefault( );
    ImFont* mediaControlsIconFont = LoadMediaControlsIconFont( io );
    m_MediaPreviewPanel->SetIconFont( mediaControlsIconFont );

    m_IniPath = ( exeDir / "imgui.ini" ).string( );
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

    m_MediaPreviewControlPanel = std::make_unique<MediaPreviewControlPanel>(
        m_MediaPreviewPanel.get( ),
        m_DownloadHistoryPanel.get( ),
        mediaControlsIconFont );

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

    // Panel update order controls default tab focus within each dock node.
    // ImGui focuses the first panel to call Begin() in each shared node, with
    // the exception of the very last Begin() call in the frame which also
    // receives focus.  Keep this ordering intentional:
    //   Top-left node    - Download Options (first) focused, Credentials secondary
    //   Right node       - Media Preview (first) focused, Dear ImGui Demo secondary
    //   Bottom-left node - Downloads (first) focused, Output secondary
    //   Bottom node      - Download Progress (last, sole occupant)
    const bool wasRunning = m_DownloadOptionsPanel->IsRunning( );
    m_DownloadOptionsPanel->Update( );
    if( !wasRunning && m_DownloadOptionsPanel->IsRunning( ) )
    {
        m_LogPanel->SetRunning( true );
        m_DownloadProgressPanel->SetRunning( true );
    }

    const bool isRunning = m_DownloadOptionsPanel->IsRunning( );

    m_CredentialsPanel->Update( );
    m_MediaPreviewPanel->Update( );
    m_MediaPreviewControlPanel->SetBlocked( isRunning );
    m_MediaPreviewControlPanel->Update( );
    m_DownloadHistoryPanel->SetBlocked(
        isRunning ||
        m_DownloadOptionsPanel->GetSigningInProvider( ) != INVALID_CONTENT_PROVIDER );
    m_DownloadHistoryPanel->Update( );
    m_LogPanel->Update( );

    ShowDemoWindow( );

    m_DownloadProgressPanel->Update( );
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

    // Split right node: narrow controls strip at the bottom, main preview above
    ImGuiID dockRightMain, dockRightControls;
    ImGui::DockBuilderSplitNode( dockRight, ImGuiDir_Down, 80.0f / 841.0f,
                                 &dockRightControls, &dockRightMain );

    // Dock all panels
    ImGui::DockBuilderDockWindow( "Download Options",  dockTopLeft );
    ImGui::DockBuilderDockWindow( "Credentials",       dockTopLeft );
    ImGui::DockBuilderDockWindow( "Output",            dockBottomLeft );
    ImGui::DockBuilderDockWindow( "Downloads",         dockBottomLeft );
    ImGui::DockBuilderDockWindow( "Media Preview",     dockRightMain );
    ImGui::DockBuilderDockWindow( "Media Controls",    dockRightControls );
    ImGui::DockBuilderDockWindow( "Download Progress", dockBottom );

#ifdef _DEBUG
    ImGui::DockBuilderDockWindow( "Dear ImGui Demo", dockRight );
#endif

    ImGui::DockBuilderFinish( dockspaceId );
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
    m_DownloadHistoryPanel->OnFileDownloaded( filepath, sourceUrl );
}
