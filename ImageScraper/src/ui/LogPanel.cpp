#include "ui/LogPanel.h"

ImageScraper::LogPanel::LogPanel( int maxLogLines )
    : m_LogContent{ static_cast<std::size_t>( maxLogLines ) }
{
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

    if( ImGui::BeginChild( "ScrollingRegion", ImVec2( 0, 0 ), false, ImGuiWindowFlags_HorizontalScrollbar ) )
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
