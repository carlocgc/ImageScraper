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

        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 4, 1 ) );

        if( copy_to_clipboard )
        {
            ImGui::LogToClipboard( );
        }

        const int size = m_LogContent.GetSize( );
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

void ImageScraper::LogPanel::Log( const LogLine& line )
{
    m_LogContent.Push( line );
}

void ImageScraper::LogPanel::OnCurrentDownloadProgress( float progress )
{
    m_CurrentDownloadProgress.store( progress, std::memory_order_relaxed );
}

void ImageScraper::LogPanel::OnTotalDownloadProgress( int current, int total )
{
    m_CurrentDownloadNum.store( current );
    m_TotalDownloadsCount.store( total );
    const float progress = static_cast<float>( current ) / static_cast<float>( total );
    m_TotalProgress.store( progress );
}

void ImageScraper::LogPanel::SetRunning( bool running )
{
    m_Running = running;
    if( !running )
    {
        m_CurrentDownloadProgress.store( 0.f, std::memory_order_relaxed );
        m_TotalProgress.store( 0.f, std::memory_order_relaxed );
        m_CurrentDownloadNum.store( 0 );
        m_TotalDownloadsCount.store( 0 );
    }
}
