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

ImageScraper::DownloadHistoryPanel::DownloadHistoryPanel( PreviewCallback onPreviewRequested, ReleaseCallback onReleaseRequested )
    : m_OnPreviewRequested{ std::move( onPreviewRequested ) }
    , m_OnReleaseRequested{ std::move( onReleaseRequested ) }
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

    // --- action buttons ---
    const bool hasSelection =
        m_SelectedIndex >= 0 && m_SelectedIndex < m_History.GetSize( );
    const std::string providerName =
        hasSelection ? GetProviderName( m_History[ m_SelectedIndex ].m_FilePath ) : "";
    const bool hasProvider = !providerName.empty( );

    ImGui::BeginDisabled( !hasSelection || m_Blocked );
    if( ImGui::Button( "Delete Selected", ImVec2( 0, 0 ) ) )
    {
        const std::string filepath = m_History[ m_SelectedIndex ].m_FilePath;

        // Release any open handle in the media preview before attempting deletion.
        if( m_OnReleaseRequested )
        {
            m_OnReleaseRequested( filepath );
        }

        std::error_code ec;
        std::filesystem::remove( filepath, ec );
        if( ec )
        {
            WarningLog( "[%s] Failed to delete file: %s",
                        __FUNCTION__, ec.message( ).c_str( ) );
        }
        else
        {
            EvictThumbnail( filepath );
            m_History.RemoveAt( m_SelectedIndex );
            AdvanceSelectionAndPreview( );
            Save( );
        }
    }
    ImGui::EndDisabled( );

    ImGui::SameLine( );

    const std::string delAllLabel =
        hasProvider ? ( "Delete All " + providerName ) : "Delete All";

    ImGui::BeginDisabled( !hasSelection || !hasProvider || m_Blocked );
    if( ImGui::Button( delAllLabel.c_str( ), ImVec2( 0, 0 ) ) )
    {
        ImGui::OpenPopup( "Confirm Delete All##hist" );
    }
    ImGui::EndDisabled( );

    ImGui::SetNextWindowSize( ImVec2( 380, 0 ), ImGuiCond_Always );
    if( ImGui::BeginPopupModal( "Confirm Delete All##hist", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::Spacing( );
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.6f, 0.1f, 1.0f ) );
        ImGui::TextWrapped(
            "This will permanently delete all downloaded %s content from disk.",
            providerName.c_str( ) );
        ImGui::PopStyleColor( );
        ImGui::Spacing( );

        ImGui::PushStyleColor( ImGuiCol_Button,        ImVec4( 0.7f, 0.3f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.9f, 0.4f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,  ImVec4( 0.5f, 0.2f, 0.0f, 1.0f ) );

        if( ImGui::Button( "Delete All", ImVec2( 110, 0 ) ) )
        {
            const auto providerRoot =
                GetProviderRoot( m_History[ m_SelectedIndex ].m_FilePath );

            // Clear the media preview before deleting so any open file handle is released.
            if( m_OnPreviewRequested )
            {
                m_OnPreviewRequested( "" );
            }

            std::error_code ec;
            std::filesystem::remove_all( providerRoot, ec );
            if( ec )
            {
                LogError( "[%s] Failed to delete %s downloads: %s",
                          __FUNCTION__, providerName.c_str( ), ec.message( ).c_str( ) );
            }

            // Only remove history entries for files that were actually deleted.
            RemoveEntriesWithPrefix( providerRoot.string( ) );
            AdvanceSelectionAndPreview( );
            ImGui::CloseCurrentPopup( );
        }

        ImGui::PopStyleColor( 3 );
        ImGui::SameLine( );
        if( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) )
        {
            ImGui::CloseCurrentPopup( );
        }
        ImGui::EndPopup( );
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

            // Skip entries whose file has been removed from disk since download
            if( !std::filesystem::exists( entry.m_FilePath ) )
            {
                continue;
            }

            ImGui::PushID( i );
            ImGui::TableNextRow( );

            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( entry.m_Timestamp.c_str( ) );

            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( entry.m_FileSize.c_str( ) );

            ImGui::TableSetColumnIndex( 2 );
            const bool isSelected = ( m_SelectedIndex == i );
            ImGui::Selectable( entry.m_FileName.c_str( ), isSelected, ImGuiSelectableFlags_SpanAllColumns );

            if( isSelected && m_ScrollToSelected )
            {
                ImGui::SetScrollHereY( 0.5f );
                m_ScrollToSelected = false;
            }

            if( ImGui::IsItemClicked( ImGuiMouseButton_Left ) )
            {
                m_SelectedIndex = i;
                SaveSelectedPath( );
                if( m_OnPreviewRequested )
                {
                    m_OnPreviewRequested( entry.m_FilePath );
                }
            }

            if( ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
            {
                OpenInExplorer( entry.m_FilePath );
            }

            if( ImGui::IsItemHovered( ) )
            {
                ImGui::BeginTooltip( );

                const ThumbnailEntry thumb = GetOrLoadThumbnail( entry.m_FilePath );
                float dispW = k_TooltipMaxSize;
                if( thumb.m_Texture != 0 )
                {
                    dispW = static_cast<float>( thumb.m_Width );
                    float dispH = static_cast<float>( thumb.m_Height );
                    // Scale down so width never exceeds k_TooltipMaxSize, preserving aspect ratio
                    if( dispW > k_TooltipMaxSize )
                    {
                        const float scale = k_TooltipMaxSize / dispW;
                        dispW *= scale;
                        dispH *= scale;
                    }

                    ImGui::Image( static_cast<ImTextureID>( thumb.m_Texture ), ImVec2( dispW, dispH ) );
                    ImGui::Separator( );
                }

                ImGui::PushTextWrapPos( ImGui::GetCursorPosX( ) + dispW );
                ImGui::TextDisabled( "Left click: preview  |  Right click: reveal in Explorer" );
                ImGui::PopTextWrapPos( );
                ImGui::EndTooltip( );
            }

            ImGui::TableSetColumnIndex( 3 );
            ImGui::TextUnformatted( entry.m_SourceUrl.c_str( ) );

            ImGui::PopID( );
        }

        ImGui::EndTable( );
    }

    if( m_SelectedIndex >= 0
        && m_SelectedIndex < m_History.GetSize( )
        && ImGui::IsWindowFocused( ImGuiFocusedFlags_ChildWindows )
        && ImGui::IsKeyPressed( ImGuiKey_Delete ) )
    {
        const std::string filepath = m_History[ m_SelectedIndex ].m_FilePath;

        // Release any open handle in the media preview before attempting deletion.
        if( m_OnReleaseRequested )
        {
            m_OnReleaseRequested( filepath );
        }

        std::error_code ec;
        std::filesystem::remove( filepath, ec );
        if( ec )
        {
            WarningLog( "[%s] Failed to delete file: %s", __FUNCTION__, ec.message( ).c_str( ) );
        }
        else
        {
            EvictThumbnail( filepath );
            m_History.RemoveAt( m_SelectedIndex );
            AdvanceSelectionAndPreview( );
            Save( );
        }
    }

    ImGui::End( );
}

void ImageScraper::DownloadHistoryPanel::RemoveEntriesWithPrefix( const std::string& rootDir )
{
    const std::string prefix = std::filesystem::path( rootDir ).make_preferred( ).string( );
    bool changed = false;

    for( int i = m_History.GetSize( ) - 1; i >= 0; --i )
    {
        const std::string& fp     = m_History[ i ].m_FilePath;
        const std::string  native = std::filesystem::path( fp ).make_preferred( ).string( );
        if( native.rfind( prefix, 0 ) != 0 )
        {
            continue;
        }

        // Only remove the history entry if the file was actually deleted from disk.
        if( std::filesystem::exists( fp ) )
        {
            continue;
        }

        EvictThumbnail( fp );
        m_History.RemoveAt( i );
        changed = true;
    }

    if( m_SelectedIndex >= m_History.GetSize( ) )
    {
        m_SelectedIndex = -1;
    }

    if( changed )
    {
        Save( );
    }
}

void ImageScraper::DownloadHistoryPanel::EvictThumbnail( const std::string& filepath )
{
    auto it = m_ThumbnailCache.find( filepath );
    if( it != m_ThumbnailCache.end( ) )
    {
        if( it->second.m_Texture != 0 )
        {
            glDeleteTextures( 1, &it->second.m_Texture );
        }
        m_ThumbnailCache.erase( it );
    }
}

void ImageScraper::DownloadHistoryPanel::AdvanceSelectionAndPreview( )
{
    const int size = m_History.GetSize( );
    if( size == 0 )
    {
        m_SelectedIndex = -1;
        m_OnPreviewRequested( "" );
        return;
    }

    const int start = std::min( m_SelectedIndex, size - 1 );

    // Search toward older items first (lower index)
    for( int i = start; i >= 0; --i )
    {
        if( std::filesystem::exists( m_History[ i ].m_FilePath ) )
        {
            m_SelectedIndex = i;
            m_OnPreviewRequested( m_History[ i ].m_FilePath );
            return;
        }
    }

    // Then toward newer
    for( int i = start + 1; i < size; ++i )
    {
        if( std::filesystem::exists( m_History[ i ].m_FilePath ) )
        {
            m_SelectedIndex = i;
            m_OnPreviewRequested( m_History[ i ].m_FilePath );
            return;
        }
    }

    m_SelectedIndex = -1;
    m_OnPreviewRequested( "" );
}

void ImageScraper::DownloadHistoryPanel::SelectNext( )
{
    for( int i = m_SelectedIndex - 1; i >= 0; --i )
    {
        if( std::filesystem::exists( m_History[ i ].m_FilePath ) )
        {
            m_SelectedIndex    = i;
            m_ScrollToSelected = true;
            m_OnPreviewRequested( m_History[ i ].m_FilePath );
            SaveSelectedPath( );
            return;
        }
    }
}

void ImageScraper::DownloadHistoryPanel::SelectPrevious( )
{
    const int size = m_History.GetSize( );
    for( int i = m_SelectedIndex + 1; i < size; ++i )
    {
        if( std::filesystem::exists( m_History[ i ].m_FilePath ) )
        {
            m_SelectedIndex    = i;
            m_ScrollToSelected = true;
            m_OnPreviewRequested( m_History[ i ].m_FilePath );
            SaveSelectedPath( );
            return;
        }
    }
}

bool ImageScraper::DownloadHistoryPanel::HasNext( ) const
{
    for( int i = m_SelectedIndex - 1; i >= 0; --i )
    {
        if( std::filesystem::exists( m_History[ i ].m_FilePath ) )
        {
            return true;
        }
    }
    return false;
}

bool ImageScraper::DownloadHistoryPanel::HasPrevious( ) const
{
    const int size = m_History.GetSize( );
    for( int i = m_SelectedIndex + 1; i < size; ++i )
    {
        if( std::filesystem::exists( m_History[ i ].m_FilePath ) )
        {
            return true;
        }
    }
    return false;
}

std::filesystem::path ImageScraper::DownloadHistoryPanel::GetProviderRoot( const std::string& filepath )
{
    std::filesystem::path result;
    bool foundDownloads = false;
    for( const auto& part : std::filesystem::path( filepath ) )
    {
        result /= part;
        if( foundDownloads )
        {
            return result;    // first component after "Downloads" = provider root
        }
        if( part == "Downloads" )
        {
            foundDownloads = true;
        }
    }
    return { };
}

std::string ImageScraper::DownloadHistoryPanel::GetProviderName( const std::string& filepath )
{
    return GetProviderRoot( filepath ).filename( ).string( );
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

void ImageScraper::DownloadHistoryPanel::Load( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = std::move( appConfig );
    if( !m_AppConfig )
    {
        return;
    }

    Json entries;
    if( !m_AppConfig->GetValue<Json>( "download_history", entries ) || !entries.is_array( ) )
    {
        return;
    }

    for( const auto& obj : entries )
    {
        DownloadHistoryEntry entry;
        entry.m_FilePath  = obj.value( "file_path",  "" );
        entry.m_FileName  = obj.value( "file_name",  "" );
        entry.m_FileSize  = obj.value( "file_size",  "" );
        entry.m_SourceUrl = obj.value( "source_url", "" );
        entry.m_Timestamp = obj.value( "timestamp",  "" );

        if( !entry.m_FilePath.empty( ) && std::filesystem::exists( entry.m_FilePath ) )
        {
            m_History.Push( std::move( entry ) );
        }
    }

    LogDebug( "[%s] Loaded %d history entries", __FUNCTION__, m_History.GetSize( ) );

    // Restore the last selected item, or default to the newest entry
    const int histSize = m_History.GetSize( );
    if( histSize == 0 )
    {
        return;
    }

    std::string selectedPath;
    m_AppConfig->GetValue<std::string>( "history_selected_path", selectedPath );

    if( !selectedPath.empty( ) )
    {
        for( int i = 0; i < histSize; ++i )
        {
            if( m_History[ i ].m_FilePath == selectedPath )
            {
                m_SelectedIndex    = i;
                m_ScrollToSelected = true;
                if( m_OnPreviewRequested )
                {
                    m_OnPreviewRequested( m_History[ i ].m_FilePath );
                }
                return;
            }
        }
    }

    // Fall back to the most recent entry
    m_SelectedIndex    = histSize - 1;
    m_ScrollToSelected = true;
    if( m_OnPreviewRequested )
    {
        m_OnPreviewRequested( m_History[ m_SelectedIndex ].m_FilePath );
    }
}

void ImageScraper::DownloadHistoryPanel::Save( )
{
    if( !m_AppConfig )
    {
        return;
    }

    Json entries = Json::array( );
    const int size = m_History.GetSize( );
    for( int i = 0; i < size; ++i )
    {
        const DownloadHistoryEntry& entry = m_History[ i ];
        if( !std::filesystem::exists( entry.m_FilePath ) )
        {
            continue;
        }
        entries.push_back( {
            { "file_path",  entry.m_FilePath  },
            { "file_name",  entry.m_FileName  },
            { "file_size",  entry.m_FileSize  },
            { "source_url", entry.m_SourceUrl },
            { "timestamp",  entry.m_Timestamp }
        } );
    }

    m_AppConfig->SetValue<Json>( "download_history", entries );
    if( !m_AppConfig->Serialise( ) )
    {
        WarningLog( "[%s] Failed to save download history", __FUNCTION__ );
    }
}

void ImageScraper::DownloadHistoryPanel::SaveSelectedPath( )
{
    if( !m_AppConfig )
    {
        return;
    }

    const std::string path =
        ( m_SelectedIndex >= 0 && m_SelectedIndex < m_History.GetSize( ) )
        ? m_History[ m_SelectedIndex ].m_FilePath : "";
    m_AppConfig->SetValue<std::string>( "history_selected_path", path );
    m_AppConfig->Serialise( );
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

    // Select the newest entry (the last one pushed) so the history highlight
    // tracks the file that was just shown in the media preview.
    m_SelectedIndex    = m_History.GetSize( ) - 1;
    m_ScrollToSelected = true;
    SaveSelectedPath( );

    Save( );
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

    if( !IsSupportedMediaExtension( filepath ) )
    {
        return { };
    }

    if( IsVideoExtension( filepath ) )
    {
        ThumbnailEntry entry = LoadVideoThumbnail( filepath );
        m_ThumbnailCache[ filepath ] = entry;
        return entry;
    }

    // GIFs: stbi_load only decodes the first frame so file size is irrelevant to memory use.
    // For all other formats the decoded bitmap scales with file size, so cap it.
    const std::string ext = std::filesystem::path( filepath ).extension( ).string( );
    const bool isGif = ( ext == ".gif" || ext == ".GIF" );
    if( !isGif )
    {
        std::error_code ec;
        const auto bytes = std::filesystem::file_size( filepath, ec );
        if( ec || bytes > k_MaxThumbnailBytes )
        {
            return { };
        }
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

bool ImageScraper::DownloadHistoryPanel::IsSupportedMediaExtension( const std::string& filepath )
{
    std::string ext = std::filesystem::path( filepath ).extension( ).string( );
    std::transform( ext.begin( ), ext.end( ), ext.begin( ), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );

    static const std::unordered_set<std::string> k_Supported =
    {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tga", ".webp",
        ".mp4", ".webm", ".mov", ".mkv", ".avi"
    };

    return k_Supported.count( ext ) > 0;
}

bool ImageScraper::DownloadHistoryPanel::IsVideoExtension( const std::string& filepath )
{
    std::string ext = std::filesystem::path( filepath ).extension( ).string( );
    std::transform( ext.begin( ), ext.end( ), ext.begin( ), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );

    static const std::unordered_set<std::string> k_Video =
    {
        ".mp4", ".webm", ".mov", ".mkv", ".avi"
    };

    return k_Video.count( ext ) > 0;
}

ImageScraper::DownloadHistoryPanel::ThumbnailEntry ImageScraper::DownloadHistoryPanel::LoadVideoThumbnail( const std::string& filepath )
{
    VideoPlayer player;
    if( !player.Open( filepath ) )
    {
        return { };
    }

    std::vector<uint8_t> rgba;
    if( !player.DecodeNextFrame( rgba ) )
    {
        return { };
    }

    const int w = player.GetWidth( );
    const int h = player.GetHeight( );

    GLuint tex = 0;
    glGenTextures( 1, &tex );
    glBindTexture( GL_TEXTURE_2D, tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data( ) );
    glBindTexture( GL_TEXTURE_2D, 0 );

    return ThumbnailEntry{ tex, w, h };
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
