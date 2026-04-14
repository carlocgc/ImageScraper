#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "ui/DownloadHistoryPanel.h"
#include "stb/stb_image.h"
#include "log/Logger.h"

#include <filesystem>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <unordered_set>

ImageScraper::DownloadHistoryPanel::DownloadHistoryPanel( PreviewCallback onPreviewRequested )
    : m_OnPreviewRequested{ std::move( onPreviewRequested ) }
{
}

ImageScraper::DownloadHistoryPanel::~DownloadHistoryPanel( )
{
    for( auto& [ path, entry ] : m_ThumbnailCache )
    {
        if( entry.m_Texture != 0 )
        {
            glDeleteTextures( 1, &entry.m_Texture );
        }
    }
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

    if( ImGui::BeginTable( "HistoryTable", 4, tableFlags ) )
    {
        ImGui::TableSetupScrollFreeze( 0, 1 );
        ImGui::TableSetupColumn( "Time",       ImGuiTableColumnFlags_WidthFixed,   90.0f );
        ImGui::TableSetupColumn( "Size",       ImGuiTableColumnFlags_WidthFixed,   70.0f );
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
            ImGui::TextUnformatted( entry.m_FileSize.c_str( ) );

            ImGui::TableSetColumnIndex( 2 );
            ImGui::Selectable( entry.m_FileName.c_str( ), false, ImGuiSelectableFlags_SpanAllColumns );

            if( ImGui::IsItemClicked( ImGuiMouseButton_Left ) && m_OnPreviewRequested )
            {
                m_OnPreviewRequested( entry.m_FilePath );
            }

            if( ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
            {
                OpenInExplorer( entry.m_FilePath );
            }

            if( ImGui::IsItemHovered( ) )
            {
                ImGui::BeginTooltip( );

                const ThumbnailEntry thumb = GetOrLoadThumbnail( entry.m_FilePath );
                if( thumb.m_Texture != 0 )
                {
                    float dispW = static_cast<float>( thumb.m_Width );
                    float dispH = static_cast<float>( thumb.m_Height );
                    if( dispW > k_TooltipMaxSize || dispH > k_TooltipMaxSize )
                    {
                        const float scale = k_TooltipMaxSize / std::max( dispW, dispH );
                        dispW *= scale;
                        dispH *= scale;
                    }

                    ImGui::Image( reinterpret_cast<ImTextureID>( static_cast<uintptr_t>( thumb.m_Texture ) ), ImVec2( dispW, dispH ) );
                    ImGui::Separator( );
                }

                ImGui::TextUnformatted( entry.m_FilePath.c_str( ) );
                ImGui::TextDisabled( "Left click: preview  |  Right click: reveal in Explorer" );
                ImGui::EndTooltip( );
            }

            ImGui::TableSetColumnIndex( 3 );
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
    entry.m_FileSize  = FormatFileSize( filepath );
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

ImageScraper::DownloadHistoryPanel::ThumbnailEntry ImageScraper::DownloadHistoryPanel::GetOrLoadThumbnail( const std::string& filepath )
{
    auto it = m_ThumbnailCache.find( filepath );
    if( it != m_ThumbnailCache.end( ) )
    {
        return it->second;
    }

    // Mark as attempted before any early-out so we never retry a failed load
    m_ThumbnailCache[ filepath ] = ThumbnailEntry{ };

    if( !IsSupportedImageExtension( filepath ) )
    {
        return { };
    }

    std::error_code ec;
    const auto bytes = std::filesystem::file_size( filepath, ec );
    if( ec || bytes > k_MaxThumbnailBytes )
    {
        return { };
    }

    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load( filepath.c_str( ), &w, &h, &channels, STBI_rgb_alpha );
    if( !data )
    {
        return { };
    }

    GLuint tex = 0;
    glGenTextures( 1, &tex );
    glBindTexture( GL_TEXTURE_2D, tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
    glBindTexture( GL_TEXTURE_2D, 0 );

    stbi_image_free( data );

    ThumbnailEntry entry{ tex, w, h };
    m_ThumbnailCache[ filepath ] = entry;
    return entry;
}

bool ImageScraper::DownloadHistoryPanel::IsSupportedImageExtension( const std::string& filepath )
{
    std::string ext = std::filesystem::path( filepath ).extension( ).string( );
    // Lowercase the extension for case-insensitive comparison
    std::transform( ext.begin( ), ext.end( ), ext.begin( ), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );

    static const std::unordered_set<std::string> k_Supported =
    {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tga", ".webp"
    };

    return k_Supported.count( ext ) > 0;
}

std::string ImageScraper::DownloadHistoryPanel::FormatFileSize( const std::string& filepath )
{
    std::error_code ec;
    const auto bytes = std::filesystem::file_size( filepath, ec );
    if( ec )
    {
        return "?";
    }

    std::ostringstream ss;
    if( bytes < 1024 )
    {
        ss << bytes << " B";
    }
    else if( bytes < 1024 * 1024 )
    {
        ss << std::fixed << std::setprecision( 1 ) << ( bytes / 1024.0 ) << " KB";
    }
    else
    {
        ss << std::fixed << std::setprecision( 1 ) << ( bytes / ( 1024.0 * 1024.0 ) ) << " MB";
    }
    return ss.str( );
}
