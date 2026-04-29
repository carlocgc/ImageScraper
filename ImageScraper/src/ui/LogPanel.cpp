#include "ui/LogPanel.h"

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace
{
    constexpr ImVec4 k_ColorError   = ImVec4( 1.0f, 0.4f, 0.4f, 1.0f );
    constexpr ImVec4 k_ColorWarning = ImVec4( 1.0f, 0.8f, 0.0f, 1.0f );
    constexpr ImVec4 k_ColorSuccess = ImVec4( 0.4f, 0.9f, 0.55f, 1.0f );
    constexpr ImVec4 k_ColorMarker  = ImVec4( 1.0f, 0.8f, 0.6f, 1.0f );

    bool LinePassesPanel( const ImageScraper::LogLine& line, ImageScraper::LogLevel level, ImGuiTextFilter& filter )
    {
        if( line.m_Level > level )
        {
            return false;
        }
        return filter.PassFilter( line.m_String.c_str( ) );
    }

    bool TryGetLineColor( const ImageScraper::LogLine& line, ImVec4& outColor )
    {
        if( line.m_Level == ImageScraper::LogLevel::Error )
        {
            outColor = k_ColorError;
            return true;
        }
        if( line.m_Level == ImageScraper::LogLevel::Warning )
        {
            outColor = k_ColorWarning;
            return true;
        }
        if( line.m_Level == ImageScraper::LogLevel::Success )
        {
            outColor = k_ColorSuccess;
            return true;
        }
        if( std::strncmp( line.m_String.c_str( ), "# ", 2 ) == 0 )
        {
            outColor = k_ColorMarker;
            return true;
        }
        return false;
    }
}

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
    if( !ImGui::Begin( "Event Log", nullptr ) )
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
        if( ImGui::BeginPopupContextWindow( "##eventLogCtx", ImGuiPopupFlags_MouseButtonRight ) )
        {
            const bool hasSelection = !m_SelectedIds.empty( );
            if( ImGui::MenuItem( "Copy", nullptr, false, hasSelection ) )
            {
                CopyLinesToClipboard( /*selectedOnly=*/true );
            }
            if( ImGui::MenuItem( "Copy All" ) )
            {
                CopyLinesToClipboard( /*selectedOnly=*/false );
            }
            if( ImGui::MenuItem( "Clear" ) )
            {
                m_LogContent.Clear( );
                m_SelectedIds.clear( );
                m_SelectionAnchorId = 0;
            }
            ImGui::Separator( );
            const bool canOpenLocation = !m_LogFilePath.empty( );
            if( ImGui::MenuItem( "Open log file location", nullptr, false, canOpenLocation ) )
            {
                OpenLogFileLocation( );
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

        // Build filtered ID list once for shift-range selection.
        std::vector<uint64_t> visibleIds;
        visibleIds.reserve( m_LogContent.Size( ) );
        for( const LogLine& line : m_LogContent )
        {
            if( LinePassesPanel( line, m_LogLevel, m_Filter ) )
            {
                visibleIds.push_back( line.m_Id );
            }
        }

        // Right-click in the empty area below the last line should also offer the menu.
        // Click on empty area (left button) clears selection.
        const ImGuiIO& io = ImGui::GetIO( );

        for( const LogLine& line : m_LogContent )
        {
            if( !LinePassesPanel( line, m_LogLevel, m_Filter ) )
            {
                continue;
            }

            const char* item = line.m_String.c_str( );

            ImVec4 color;
            const bool hasColor = TryGetLineColor( line, color );

            const bool isSelected = m_SelectedIds.find( line.m_Id ) != m_SelectedIds.end( );

            // Compute the visible text height (respecting word wrap) so the Selectable's
            // hit-rect matches the rendered text. Selectable's own auto-sizing measures
            // the literal label and ignores PushTextWrapPos, which is why we draw the
            // text as a separate overlay on top.
            const float wrapW   = m_WordWrap ? ImGui::GetContentRegionAvail( ).x : -1.0f;
            const ImVec2 textSize = ImGui::CalcTextSize( item, nullptr, false, wrapW );

            const ImVec2 cursorBefore = ImGui::GetCursorPos( );

            char selLabel[ 32 ];
            std::snprintf( selLabel, sizeof( selLabel ), "##sel_%llu", static_cast<unsigned long long>( line.m_Id ) );
            ImGui::SetNextItemAllowOverlap( );

            const bool selectableClicked = ImGui::Selectable(
                selLabel, isSelected,
                ImGuiSelectableFlags_None,
                ImVec2( 0.0f, textSize.y ) );
            const bool rightClickedItem  = ImGui::IsItemClicked( ImGuiMouseButton_Right );

            // Overlay the visible colored text exactly where the Selectable was drawn.
            ImGui::SetCursorPos( cursorBefore );
            if( hasColor )
            {
                ImGui::PushStyleColor( ImGuiCol_Text, color );
            }
            ImGui::TextUnformatted( item );  // wraps when PushTextWrapPos is in effect
            if( hasColor )
            {
                ImGui::PopStyleColor( );
            }

            if( selectableClicked )
            {
                const bool ctrl  = io.KeyCtrl;
                const bool shift = io.KeyShift;

                if( shift && m_SelectionAnchorId != 0 )
                {
                    auto itAnchor = std::find( visibleIds.begin( ), visibleIds.end( ), m_SelectionAnchorId );
                    auto itClick  = std::find( visibleIds.begin( ), visibleIds.end( ), line.m_Id );
                    if( itAnchor != visibleIds.end( ) && itClick != visibleIds.end( ) )
                    {
                        if( itAnchor > itClick ) std::swap( itAnchor, itClick );
                        if( !ctrl )
                        {
                            m_SelectedIds.clear( );
                        }
                        for( auto it = itAnchor; it != itClick + 1; ++it )
                        {
                            m_SelectedIds.insert( *it );
                        }
                    }
                }
                else if( ctrl )
                {
                    if( isSelected )
                    {
                        m_SelectedIds.erase( line.m_Id );
                    }
                    else
                    {
                        m_SelectedIds.insert( line.m_Id );
                    }
                    m_SelectionAnchorId = line.m_Id;
                }
                else
                {
                    m_SelectedIds.clear( );
                    m_SelectedIds.insert( line.m_Id );
                    m_SelectionAnchorId = line.m_Id;
                }
            }
            else if( rightClickedItem && m_SelectedIds.find( line.m_Id ) == m_SelectedIds.end( ) )
            {
                // Right-clicking outside the existing selection replaces it with the
                // clicked line so context-menu actions operate on something sensible.
                m_SelectedIds.clear( );
                m_SelectedIds.insert( line.m_Id );
                m_SelectionAnchorId = line.m_Id;
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

        // Click in empty space (left button) within the scrolling region clears selection.
        if( ImGui::IsWindowHovered( ) && ImGui::IsMouseClicked( ImGuiMouseButton_Left )
            && !ImGui::IsAnyItemHovered( ) )
        {
            m_SelectedIds.clear( );
            m_SelectionAnchorId = 0;
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

void ImageScraper::LogPanel::CopyLinesToClipboard( bool selectedOnly )
{
    std::string out;
    out.reserve( 256 );

    for( const LogLine& line : m_LogContent )
    {
        if( !LinePassesPanel( line, m_LogLevel, m_Filter ) )
        {
            continue;
        }
        if( selectedOnly && m_SelectedIds.find( line.m_Id ) == m_SelectedIds.end( ) )
        {
            continue;
        }
        // Lines already terminate with '\n' from Logger::Log.
        out.append( line.m_String );
    }

    if( !out.empty( ) )
    {
        ImGui::SetClipboardText( out.c_str( ) );
    }
}

void ImageScraper::LogPanel::OpenLogFileLocation( ) const
{
#ifdef _WIN32
    if( m_LogFilePath.empty( ) )
    {
        return;
    }
    // Opens Explorer at the parent directory and selects the current log file.
    const std::string params = std::string( "/select,\"" ) + m_LogFilePath + "\"";
    ::ShellExecuteA( nullptr, "open", "explorer.exe", params.c_str( ), nullptr, SW_SHOWNORMAL );
#endif
}
