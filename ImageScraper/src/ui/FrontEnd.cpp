#include "ui/FrontEnd.h"
#include "log/Logger.h"
#include "config/Config.h"

#include <algorithm>

ImageScraper::FrontEnd::FrontEnd( int maxLogLines )
    : m_LogContent{ static_cast< std::size_t >( maxLogLines ) }
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
    glfwSetErrorCallback( GLFW_ErrorCallback );
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
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

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

    ImGui::DockSpaceOverViewport( );

    // Demo window start

    ShowDemoWindow( );

    // Demo window End

    // Input window start

    ImGui::SetNextWindowSize( ImVec2( 640, 200 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Input", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    UpdateProviderWidgets( );

    UpdateCommonWidgets( );

    ImGui::End( );

    // Input window end

    // Log window start

    UpdateLogWindowWidgets( );

    // Log window End
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

    DebugLog( "[%s] Started processing user input...", __FUNCTION__ );

    m_StartProcess = false;
    m_Running = true;

    UserInputOptions inputOptions{ };

    const ContentProvider provider = static_cast< ContentProvider >( m_ContentProvider );
    switch( provider )
    {
    case ImageScraper::ContentProvider::Reddit:

        if( m_SubredditName.empty( ) )
        {
            return false;
        }

        DebugLog( "[%s] Building Reddit user input data", __FUNCTION__ );
        inputOptions = BuildRedditInputOptions( );
        break;
    case ImageScraper::ContentProvider::Tumblr:

        if( m_TumblrUser.empty( ) )
        {
            return false;
        }

        DebugLog( "[%s] Building Tumblr user input data", __FUNCTION__ );
        inputOptions = BuildTumblrInputOptions( );
        break;
    case ImageScraper::ContentProvider::FourChan:

        if( m_FourChanBoard.empty( ) )
        {
            return false;
        }

        DebugLog( "[%s] Building 4chan user input data", __FUNCTION__ );
        inputOptions = BuildFourChanInputOptions( );
        break;
    default:
        DebugLog( "[%s] Invalid ContentProvider, check ContentProvider constant", __FUNCTION__ );
        return false;
        break;
    }

    for( auto service : services )
    {
        if( service->HandleUserInput( inputOptions ) )
        {
            DebugLog( "[%s] User input handled!", __FUNCTION__ );
            return true;
        }
    }

    DebugLog( "[%s] User input not handled, check for missing services!", __FUNCTION__ );
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

void ImageScraper::FrontEnd::Log( const LogLine& line )
{
    m_LogContent.Push( line );
}

void ImageScraper::FrontEnd::UpdateCurrentDownloadProgress( const float progress )
{
    m_CurrentDownloadProgress.store( progress, std::memory_order_relaxed );
}

void ImageScraper::FrontEnd::UpdateTotalDownloadsProgress( const int current, const int total )
{
    m_CurrentDownloadNum.store( current );
    m_TotalDownloadsCount.store( total );
    const float progress = static_cast< float >( current ) / static_cast< float >( total );
    m_TotalProgress.store( progress );
}

void ImageScraper::FrontEnd::ShowDemoWindow( )
{
    bool show = true;
    ImGui::ShowDemoWindow( &show );
}

void ImageScraper::FrontEnd::UpdateProviderWidgets( )
{
    ImGui::BeginDisabled( m_InputState == InputState::Blocked );

    if( ImGui::BeginChild( "ContentProvider", ImVec2( 500, 25 ), false ) )
    {
        ImGui::Combo( "Content Provider", &m_ContentProvider, s_ContentProviderStrings, IM_ARRAYSIZE( s_ContentProviderStrings ) );
    }

    ImGui::EndChild( );

    const ContentProvider provider = static_cast< ContentProvider >( m_ContentProvider );

    switch( provider )
    {
    case ImageScraper::ContentProvider::Reddit:
        UpdateRedditWidgets( );
        break;
    case ImageScraper::ContentProvider::Tumblr:
        UpdateTumblrWidgets( );
        break;
    case ImageScraper::ContentProvider::FourChan:
        UpdateFourChanWidgets( );
        break;
    default:
        break;
    }

    ImGui::EndDisabled( );
}

void ImageScraper::FrontEnd::UpdateRedditWidgets( )
{
    if( ImGui::BeginChild( "SubredditName", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_SubredditName.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Subreddit (e.g. Gifs)", buffer, INPUT_STRING_MAX, flags, nullptr, this ) )
        {
            m_SubredditName = buffer;
        }
    }

    ImGui::EndChild( );

    if( ImGui::BeginChild( "SubredditScope", ImVec2( 500, 25 ), false ) )
    {
        int redditScope = static_cast< int >( m_RedditScope );
        ImGui::Combo( "Scope", &redditScope, s_RedditScopeStrings, IM_ARRAYSIZE( s_RedditScopeStrings ) );
        m_RedditScope = static_cast< RedditScope >( redditScope );
    }

    ImGui::EndChild( );

    RedditScope scope = static_cast< RedditScope >( m_RedditScope );

    if( scope == RedditScope::Top ||
        scope == RedditScope::Controversial ||
        scope == RedditScope::Sort )
    {
        if( ImGui::BeginChild( "SubredditScopeTimeFrame", ImVec2( 500, 25 ), false ) )
        {
            int redditScopeTimeFrame = static_cast< int >( m_RedditScopeTimeFrame );
            ImGui::Combo( "Time Frame", &redditScopeTimeFrame, s_RedditScopeTimeFrameStrings, IM_ARRAYSIZE( s_RedditScopeTimeFrameStrings ) );
            m_RedditScopeTimeFrame = static_cast< RedditScopeTimeFrame >( redditScopeTimeFrame );
        }

        ImGui::EndChild( );
    }

    if( ImGui::BeginChild( "RedditMaxMediaItems", ImVec2( 500, 25 ), false ) )
    {
        ImGui::InputInt( "Max Downloads", &m_RedditMaxMediaItems );
        m_RedditMaxMediaItems = std::clamp( m_RedditMaxMediaItems, REDDIT_LIMIT_MIN, REDDIT_LIMIT_MAX );
    }

    ImGui::EndChild( );
}

void ImageScraper::FrontEnd::UpdateTumblrWidgets( )
{
    if( ImGui::BeginChild( "TumblrUser", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_TumblrUser.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Tumblr User", buffer, INPUT_STRING_MAX, flags, nullptr, this ) )
        {
            m_TumblrUser = buffer;
        }
    }

    ImGui::EndChild( );
}

void ImageScraper::FrontEnd::UpdateFourChanWidgets( )
{
    if( ImGui::BeginChild( "FourChanBoard", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_FourChanBoard.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Board (e.g. v, sci )", buffer, INPUT_STRING_MAX, flags, nullptr, this ) )
        {
            m_FourChanBoard = buffer;
        }
    }

    ImGui::EndChild( );

    /*
    if( ImGui::BeginChild( "FourChanThreadMax", ImVec2( 500, 25 ), false ) )
    {
        ImGui::InputInt( "Max Threads", &m_FourChanMaxThreads );
        m_FourChanMaxThreads = std::clamp( m_FourChanMaxThreads, FOURCHAN_THREAD_MIN, FOURCHAN_THREAD_MAX );
    }

    ImGui::EndChild( );

    if( ImGui::BeginChild( "FourChanMediaMax", ImVec2( 500, 25 ), false ) )
    {
        ImGui::InputInt( "Max Media Items", &m_FourChanMaxMediaItems );
        m_FourChanMaxMediaItems = std::clamp( m_FourChanMaxMediaItems, FOURCHAN_MEDIA_MIN, FOURCHAN_MEDIA_MAX );
    }

    ImGui::EndChild( );
    */
}

void ImageScraper::FrontEnd::UpdateCommonWidgets( )
{
    if( !m_Running )
    {
        if( ImGui::BeginChild( "RunButton", ImVec2( 300, 0 ), false ) )
        {
            m_StartProcess = ImGui::Button( "Run", ImVec2( 100, 40 ) );
        }

        ImGui::EndChild( );
    }
    else
    {
        ImGui::BeginDisabled( m_Cancelled.load( ) );

        if( ImGui::BeginChild( "CancelButton", ImVec2( 300, 0 ), false ) )
        {
            if( ImGui::Button( "Cancel", ImVec2( 100, 40 ) ) )
            {
                m_Cancelled.store( true );
            }
        }

        ImGui::EndChild( );

        ImGui::EndDisabled( );
    }
}

void ImageScraper::FrontEnd::UpdateLogWindowWidgets( )
{
    ImGui::SetNextWindowSize( ImVec2( 1280, 360 ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "Output", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    // Options menu
    if( ImGui::BeginPopup( "Options" ) )
    {
        ImGui::Checkbox( "Auto-scroll", &m_AutoScroll );
        ImGui::Checkbox( "Debug Logs", &m_DebugLogging );

        ImGui::EndPopup( );
    }

    m_LogLevel = m_DebugLogging ? LogLevel::Debug : LogLevel::Error;

    // Options, Filter
    if( ImGui::Button( "Options" ) )
    {
        ImGui::OpenPopup( "Options" );
    }

    ImGui::SameLine( );
    bool copy_to_clipboard = ImGui::Button( "Copy" );

    ImGui::SameLine( );
    m_Filter.Draw( "Filter (\"incl,-excl\") (\"error\")", 180 );

    ImGui::Separator( );

    // Reserve enough left-over height for 1 separator + 1 input text
    // *2 to allow for progress bars
    const float footer_height_to_reserve = ImGui::GetStyle( ).ItemSpacing.y + ImGui::GetFrameHeightWithSpacing( ) * 2;
    if( ImGui::BeginChild( "ScrollingRegion", ImVec2( 0, -footer_height_to_reserve ), false, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        if( ImGui::BeginPopupContextWindow( ) )
        {
            if( ImGui::Selectable( "Clear" ) )
            {
                m_LogContent.Clear( );
            }
            ImGui::EndPopup( );
        }

        // Display every line as a separate entry so we can change their color or add custom widgets.
        // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
        // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
        // to only process visible items. The clipper will automatically measure the height of your first item and then
        // "seek" to display only items in the visible area.
        // To use the clipper we can replace your standard loop:
        //      for (int i = 0; i < Items.Size; i++)
        //   With:
        //      ImGuiListClipper clipper;
        //      clipper.Begin(Items.Size);
        //      while (clipper.Step())
        //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        // - That your items are evenly spaced (same height)
        // - That you have cheap random access to your elements (you can access them given their index,
        //   without processing all the ones before)
        // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
        // We would need random-access on the post-filtered list.
        // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
        // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
        // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
        // to improve this example code!
        // If your items are of variable height:
        // - Split them into same height items would be simpler and facilitate random-seeking into your list.
        // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 4, 1 ) ); // Tighten spacing

        if( copy_to_clipboard )
        {
            ImGui::LogToClipboard( );
        }

        int size = m_LogContent.GetSize( );
        for( int i = 0; i < size; i++ )
        {
            if( m_LogContent[ i ].m_Level > m_LogLevel )
            {
                continue;
            }

            const char* item = m_LogContent[ i ].m_String.c_str( );
            if( !m_Filter.PassFilter( item ) )
            {
                continue;
            }

            // Normally you would store more information in your item than just a string.
            // (e.g. make Items[] an array of structure, store color/type etc.)
            ImVec4 color;
            bool has_color = false;
            if( m_LogContent[ i ].m_Level == LogLevel::Error )
            {
                color = ImVec4( 1.0f, 0.4f, 0.4f, 1.0f );
                has_color = true;
            }
            else if( m_LogContent[ i ].m_Level == LogLevel::Warning )
            {
                color = ImVec4( 1.0f, 0.8f, 0.0f, 1.0f );
                has_color = true;
            }
            else if( strncmp( item, "# ", 2 ) == 0 )
            {
                color = ImVec4( 1.0f, 0.8f, 0.6f, 1.0f );
                has_color = true;
            }

            if( has_color )
            {
                ImGui::PushStyleColor( ImGuiCol_Text, color );
            }

            ImGui::TextUnformatted( item );

            if( has_color )
            {
                ImGui::PopStyleColor( );
            }
        }

        if( copy_to_clipboard )
        {
            ImGui::LogFinish( );
        }

        // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
        // Using a scrollbar or mouse-wheel will take away from the bottom edge.
        if( m_ScrollToBottom || ( m_AutoScroll && ImGui::GetScrollY( ) >= ImGui::GetScrollMaxY( ) ) )
        {
            ImGui::SetScrollHereY( 1.0f );
        }

        m_ScrollToBottom = false;

        ImGui::PopStyleVar( );
    }

    ImGui::EndChild( );

    ImGui::Separator( );

    const float progressLabelWidth = 150.f;

    ImGui::ProgressBar( m_CurrentDownloadProgress, ImVec2( ImGui::GetStyle( ).ItemInnerSpacing.x - progressLabelWidth, 0.0f ) );
    ImGui::SameLine( 0.0f, ImGui::GetStyle( ).ItemInnerSpacing.x );
    ImGui::Text( "Current Download" );

    char buf[ 32 ] = "";
    if( m_Running )
    {
        sprintf_s( buf, 32, "%d/%d", m_CurrentDownloadNum.load( ), m_TotalDownloadsCount.load( ) );
    }
    ImGui::ProgressBar( m_TotalProgress, ImVec2( ImGui::GetStyle( ).ItemInnerSpacing.x - progressLabelWidth, 0.f ), buf );
    ImGui::SameLine( 0.0f, ImGui::GetStyle( ).ItemInnerSpacing.x );
    ImGui::Text( "Total Downloads" );

    ImGui::End( );
}

ImageScraper::UserInputOptions ImageScraper::FrontEnd::BuildRedditInputOptions( )
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::Reddit;
    options.m_SubredditName = m_SubredditName;

    auto toLower = [ ]( unsigned char ch ) { return std::tolower( ch ); };

    std::string scope = s_RedditScopeStrings[ static_cast< int >( m_RedditScope ) ];
    std::transform( scope.begin( ), scope.end( ), scope.begin( ), toLower );

    options.m_RedditScope = scope;

    if( m_RedditScope == RedditScope::Top || m_RedditScope == RedditScope::Controversial || m_RedditScope == RedditScope::Sort )
    {
        std::string scopeTimeFrame = s_RedditScopeTimeFrameStrings[ static_cast< int >( m_RedditScopeTimeFrame ) ];
        std::transform( scopeTimeFrame.begin( ), scopeTimeFrame.end( ), scopeTimeFrame.begin( ), toLower );

        options.m_RedditScopeTimeFrame = scopeTimeFrame;
    }

    options.m_RedditMaxMediaItems = m_RedditMaxMediaItems;

    return options;
}

ImageScraper::UserInputOptions ImageScraper::FrontEnd::BuildTumblrInputOptions( )
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::Tumblr;
    options.m_TumblrUser = m_TumblrUser;
    return options;
}

ImageScraper::UserInputOptions ImageScraper::FrontEnd::BuildFourChanInputOptions( )
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::FourChan;
    options.m_FourChanBoard = m_FourChanBoard;
    options.m_FourChanMaxThreads = m_FourChanMaxThreads;
    options.m_FourChanMaxMediaItems = m_FourChanMaxMediaItems;
    return options;
}

void ImageScraper::FrontEnd::Reset( )
{
    m_Running = false;
    m_Cancelled.store( false );
    m_CurrentDownloadNum.store( 0 );
    m_TotalDownloadsCount.store( 0 );
    m_CurrentDownloadProgress.store( 0.f );
    m_TotalProgress.store( 0.f );
}

void ImageScraper::FrontEnd::SetInputState( const InputState& state )
{
    m_InputState = state;

    if( m_InputState == InputState::Free )
    {
        Reset( );
    }
}
