#include "ui/LogPanel.h"

ImageScraper::LogPanel::LogPanel( int maxLogLines )
    : m_LogContent{ static_cast<std::size_t>( maxLogLines ) }
{
}

void ImageScraper::LogPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = std::move( appConfig );
    m_WordWrap = LoadWordWrapEnabledFromConfig( m_AppConfig );
}

void ImageScraper::LogPanel::Update( )
{
    ImGui::SetNextWindowSize( ImVec2( 1280, 360 ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "Output", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    if( ImGui::BeginPopup( "Options" ) )
    {
        ImGui::Checkbox( "Auto-scroll", &m_AutoScroll );
        ImGui::Checkbox( "Debug Logs", &m_DebugLogging );

        bool wordWrapEnabled = m_WordWrap;
        if( ImGui::Checkbox( "Word Wrap", &wordWrapEnabled ) )
        {
            SetWordWrapEnabled( wordWrapEnabled );
        }

        ImGui::EndPopup( );
    }

    m_LogLevel = m_DebugLogging ? LogLevel::Debug : LogLevel::Error;

    if( ImGui::Button( "Options" ) )
    {
        ImGui::OpenPopup( "Options" );
    }

    ImGui::SameLine( );
    bool copy_to_clipboard = ImGui::Button( "Copy" );

    ImGui::SameLine( );
    m_Filter.Draw( "Filter (\"incl,-excl\") (\"error\")", 180 );

    ImGui::Separator( );

    const ImGuiWindowFlags scrollingRegionFlags = m_WordWrap ? ImGuiWindowFlags_None
                                                             : ImGuiWindowFlags_HorizontalScrollbar;

    if( ImGui::BeginChild( "ScrollingRegion", ImVec2( 0, 0 ), false, scrollingRegionFlags ) )
    {
        if( ImGui::BeginPopupContextWindow( ) )
        {
            if( ImGui::Selectable( "Clear" ) )
            {
                m_LogContent.Clear( );
            }
            ImGui::EndPopup( );
        }

        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 4, 1 ) );

        if( copy_to_clipboard )
        {
            ImGui::LogToClipboard( );
        }

        if( m_WordWrap )
        {
            ImGui::PushTextWrapPos( 0.0f );
        }

        for( const LogLine& line : m_LogContent )
        {
            if( line.m_Level > m_LogLevel )
            {
                continue;
            }

            const char* item = line.m_String.c_str( );
            if( !m_Filter.PassFilter( item ) )
            {
                continue;
            }

            ImVec4 color;
            bool has_color = false;
            if( line.m_Level == LogLevel::Error )
            {
                color = ImVec4( 1.0f, 0.4f, 0.4f, 1.0f );
                has_color = true;
            }
            else if( line.m_Level == LogLevel::Warning )
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

        if( m_WordWrap )
        {
            ImGui::PopTextWrapPos( );
        }

        if( copy_to_clipboard )
        {
            ImGui::LogFinish( );
        }

        if( m_ScrollToBottom || ( m_AutoScroll && ImGui::GetScrollY( ) >= ImGui::GetScrollMaxY( ) ) )
        {
            ImGui::SetScrollHereY( 1.0f );
        }

        m_ScrollToBottom = false;

        ImGui::PopStyleVar( );
    }

    ImGui::EndChild( );

    ImGui::End( );
}

void ImageScraper::LogPanel::Log( const LogLine& line )
{
    m_LogContent.Push( line );
}

void ImageScraper::LogPanel::SetRunning( bool running )
{
    m_Running = running;
}

void ImageScraper::LogPanel::SetWordWrapEnabled( bool enabled )
{
    if( m_WordWrap == enabled )
    {
        return;
    }

    m_WordWrap = enabled;
    PersistWordWrapState( );
}

void ImageScraper::LogPanel::PersistWordWrapState( )
{
    if( !m_AppConfig )
    {
        return;
    }

    if( !SaveWordWrapEnabledToConfig( m_AppConfig, m_WordWrap ) )
    {
        LogError( "[%s] Failed to save log panel word wrap state.", __FUNCTION__ );
    }
}
