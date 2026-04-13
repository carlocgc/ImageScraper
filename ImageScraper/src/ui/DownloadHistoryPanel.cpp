#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "ui/DownloadHistoryPanel.h"
#include "log/Logger.h"

#include <filesystem>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

ImageScraper::DownloadHistoryPanel::DownloadHistoryPanel( )
{
}

void ImageScraper::DownloadHistoryPanel::Update( )
{
    FlushPending( );

    ImGui::SetNextWindowSize( ImVec2( 700, 300 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Download History", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    if( m_History.IsEmpty( ) )
    {
        ImGui::TextDisabled( "No downloads yet" );
        ImGui::End( );
        return;
    }

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    if( ImGui::BeginTable( "HistoryTable", 3, tableFlags ) )
    {
        ImGui::TableSetupScrollFreeze( 0, 1 );
        ImGui::TableSetupColumn( "Time",       ImGuiTableColumnFlags_WidthFixed, 90.0f );
        ImGui::TableSetupColumn( "Filename",   ImGuiTableColumnFlags_WidthStretch, 0.35f );
        ImGui::TableSetupColumn( "Source URL", ImGuiTableColumnFlags_WidthStretch, 0.65f );
        ImGui::TableHeadersRow( );

        // Iterate newest-first: walk from (End-1) down to Start
        const int size = m_History.GetSize( );
        for( int i = size - 1; i >= 0; --i )
        {
            const DownloadHistoryEntry& entry = m_History[ i ];

            ImGui::TableNextRow( );

            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( entry.m_Timestamp.c_str( ) );

            ImGui::TableSetColumnIndex( 1 );
            const bool clicked = ImGui::Selectable(
                entry.m_FileName.c_str( ),
                false,
                ImGuiSelectableFlags_SpanAllColumns );

            if( clicked )
            {
                OpenInExplorer( entry.m_FilePath );
            }

            if( ImGui::IsItemHovered( ) )
            {
                ImGui::SetTooltip( "%s\nClick to reveal in Explorer", entry.m_FilePath.c_str( ) );
            }

            ImGui::TableSetColumnIndex( 2 );
            ImGui::TextUnformatted( entry.m_SourceUrl.c_str( ) );
        }

        ImGui::EndTable( );
    }

    ImGui::End( );
}

void ImageScraper::DownloadHistoryPanel::OnFileDownloaded( const std::string& filepath, const std::string& sourceUrl )
{
    DownloadHistoryEntry entry;
    entry.m_FilePath  = filepath;
    entry.m_FileName  = ExtractFileName( filepath );
    entry.m_SourceUrl = sourceUrl;
    entry.m_Timestamp = FormatTimestamp( );

    std::lock_guard<std::mutex> lock( m_PendingMutex );
    m_Pending.push_back( std::move( entry ) );
}

void ImageScraper::DownloadHistoryPanel::FlushPending( )
{
    std::vector<DownloadHistoryEntry> pending;
    {
        std::lock_guard<std::mutex> lock( m_PendingMutex );
        if( m_Pending.empty( ) )
        {
            return;
        }
        pending.swap( m_Pending );
    }

    for( auto& entry : pending )
    {
        m_History.Push( std::move( entry ) );
    }
}

void ImageScraper::DownloadHistoryPanel::OpenInExplorer( const std::string& filepath )
{
    // Convert to native Windows path and select the file in Explorer
    const std::wstring wpath = std::filesystem::path( filepath ).make_preferred( ).wstring( );
    const std::wstring args  = L"/select,\"" + wpath + L"\"";
    ShellExecuteW( nullptr, L"open", L"explorer.exe", args.c_str( ), nullptr, SW_SHOW );
}

std::string ImageScraper::DownloadHistoryPanel::FormatTimestamp( )
{
    const auto now   = std::chrono::system_clock::now( );
    const std::time_t t = std::chrono::system_clock::to_time_t( now );
    std::tm tm{ };
    localtime_s( &tm, &t );

    std::ostringstream ss;
    ss << std::put_time( &tm, "%H:%M:%S" );
    return ss.str( );
}

std::string ImageScraper::DownloadHistoryPanel::ExtractFileName( const std::string& filepath )
{
    return std::filesystem::path( filepath ).filename( ).string( );
}
