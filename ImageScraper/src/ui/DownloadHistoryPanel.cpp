#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "ui/DownloadHistoryPanel.h"
#include "utils/DownloadUtils.h"
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
#include <vector>

namespace
{
    enum class SortColumn : ImGuiID
    {
        Name    = 0,
        Size    = 1,
        Type    = 2,
        Created = 3
    };

    constexpr const char* kDownloadHistoryKey       = "download_history";
    constexpr const char* kDownloadsSelectedPathKey = "downloads_selected_path";
    constexpr const char* kLegacySelectedPathKey    = "history_selected_path";
    constexpr const char* kDeleteConfirmPopupId     = "Confirm Delete##downloads";

    std::string ToLowerCopy( const std::string& value )
    {
        std::string lower = value;
        std::transform( lower.begin( ), lower.end( ), lower.begin( ), []( unsigned char c )
        {
            return static_cast<char>( std::tolower( c ) );
        } );
        return lower;
    }

    std::filesystem::path NormalisePath( const std::filesystem::path& path )
    {
        std::error_code ec;
        std::filesystem::path normalised = std::filesystem::weakly_canonical( path, ec );
        if( ec )
        {
            normalised = path.lexically_normal( );
        }

        normalised.make_preferred( );
        return normalised;
    }

    std::string GetPathDisplayName( const std::filesystem::path& path )
    {
        std::string label = path.filename( ).string( );
        if( label.empty( ) )
        {
            label = NormalisePath( path ).string( );
        }

        return label;
    }

    int CompareStringsCaseInsensitive( const std::string& lhs, const std::string& rhs )
    {
        const std::string lhsLower = ToLowerCopy( lhs );
        const std::string rhsLower = ToLowerCopy( rhs );
        if( lhsLower < rhsLower )
        {
            return -1;
        }

        if( lhsLower > rhsLower )
        {
            return 1;
        }

        return 0;
    }

    uintmax_t GetPathFileSizeBytes( const std::filesystem::path& path )
    {
        std::error_code ec;
        const uintmax_t bytes = std::filesystem::file_size( path, ec );
        return ec ? 0 : bytes;
    }

    unsigned long long GetPathCreationTimeTicks( const std::filesystem::path& path )
    {
        WIN32_FILE_ATTRIBUTE_DATA attributeData{ };
        if( !GetFileAttributesExW( path.wstring( ).c_str( ), GetFileExInfoStandard, &attributeData ) )
        {
            return 0;
        }

        ULARGE_INTEGER fileTime{ };
        fileTime.LowPart  = attributeData.ftCreationTime.dwLowDateTime;
        fileTime.HighPart = attributeData.ftCreationTime.dwHighDateTime;
        return fileTime.QuadPart;
    }
}

ImageScraper::DownloadHistoryPanel::DownloadHistoryPanel( PreviewCallback onPreviewRequested, ReleaseCallback onReleaseRequested )
    : m_OnPreviewRequested{ std::move( onPreviewRequested ) }
    , m_OnReleaseRequested{ std::move( onReleaseRequested ) }
{
}

ImageScraper::DownloadHistoryPanel::~DownloadHistoryPanel( )
{
    for( auto& [ filepath, future ] : m_ThumbnailFutures )
    {
        if( future.valid( ) )
        {
            future.wait( );
        }
    }

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
    FlushDecodedThumbnails( );

    ImGui::SetNextWindowSize( ImVec2( 700, 300 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Downloads", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    bool openDeleteConfirm = false;
    if( !m_SelectedPath.empty( ) && !std::filesystem::exists( std::filesystem::path{ m_SelectedPath } ) )
    {
        RemoveEntriesInPath( m_DownloadsRoot, true );
        AdvanceSelectionAndPreview( );
    }

    if( m_DownloadsRoot.empty( ) || !std::filesystem::exists( m_DownloadsRoot ) )
    {
        ImGui::TextDisabled( "No downloads yet" );
        if( !m_DownloadsRoot.empty( ) )
        {
            ImGui::Spacing( );
            ImGui::TextDisabled( "%s", MakePreferredPathString( m_DownloadsRoot ).c_str( ) );
        }
    }
    else
    {
        constexpr ImGuiTableFlags tableFlags =
            ImGuiTableFlags_BordersV |
            ImGuiTableFlags_BordersOuterH |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_Sortable |
            ImGuiTableFlags_ScrollY;

        ImGui::PushStyleColor( ImGuiCol_TableHeaderBg, IM_COL32( 52, 52, 56, 255 ) );
        ImGui::PushStyleColor( ImGuiCol_TableBorderStrong, IM_COL32( 62, 62, 68, 255 ) );
        ImGui::PushStyleColor( ImGuiCol_TableBorderLight, IM_COL32( 48, 48, 54, 255 ) );
        ImGui::PushStyleColor( ImGuiCol_TableRowBg, IM_COL32( 20, 20, 22, 255 ) );
        ImGui::PushStyleColor( ImGuiCol_TableRowBgAlt, IM_COL32( 30, 30, 33, 255 ) );
        ImGui::PushStyleColor( ImGuiCol_TreeLines, IM_COL32( 112, 112, 118, 170 ) );
        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2( 8.0f, 1.0f ) );

        if( ImGui::BeginTable( "DownloadsTable", 4, tableFlags, ImGui::GetContentRegionAvail( ) ) )
        {
            ImGui::TableSetupScrollFreeze( 0, 1 );
            ImGui::TableSetupColumn(
                "Name",
                ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort,
                0.0f,
                static_cast<ImGuiID>( SortColumn::Name ) );
            ImGui::TableSetupColumn(
                "Size",
                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
                120.0f,
                static_cast<ImGuiID>( SortColumn::Size ) );
            ImGui::TableSetupColumn(
                "Type",
                ImGuiTableColumnFlags_WidthFixed,
                160.0f,
                static_cast<ImGuiID>( SortColumn::Type ) );
            ImGui::TableSetupColumn(
                "Created",
                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
                145.0f,
                static_cast<ImGuiID>( SortColumn::Created ) );
            ImGui::TableHeadersRow( );

            RenderTreeNode( m_DownloadsRoot, &openDeleteConfirm );
            ImGui::EndTable( );
        }

        ImGui::PopStyleVar( );
        ImGui::PopStyleColor( 6 );
    }

    if( ImGui::IsWindowFocused( ImGuiFocusedFlags_ChildWindows )
        && ImGui::IsKeyPressed( ImGuiKey_Delete )
        && CanDeletePath( std::filesystem::path{ m_SelectedPath } ) )
    {
        m_DeleteConfirmPath = m_SelectedPath;
        openDeleteConfirm = true;
    }

    if( openDeleteConfirm && !m_DeleteConfirmPath.empty( ) )
    {
        ImGui::OpenPopup( kDeleteConfirmPopupId );
    }

    ImGui::SetNextWindowSize( ImVec2( 420, 0 ), ImGuiCond_Always );
    if( ImGui::BeginPopupModal( kDeleteConfirmPopupId, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        const std::filesystem::path deletePath = m_DeleteConfirmPath;
        std::error_code ec;
        const bool isDirectory = std::filesystem::is_directory( deletePath, ec );
        const std::string deleteLabel =
            deletePath.filename( ).empty( )
            ? MakePreferredPathString( deletePath )
            : deletePath.filename( ).string( );

        ImGui::Spacing( );
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.6f, 0.1f, 1.0f ) );
        if( isDirectory )
        {
            ImGui::TextWrapped(
                "This will permanently delete the folder %s and all of its contents from disk.",
                deleteLabel.c_str( ) );
        }
        else
        {
            ImGui::TextWrapped(
                "This will permanently delete the file %s from disk.",
                deleteLabel.c_str( ) );
        }
        ImGui::PopStyleColor( );
        ImGui::Spacing( );

        ImGui::PushStyleColor( ImGuiCol_Button,        ImVec4( 0.7f, 0.3f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.9f, 0.4f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,  ImVec4( 0.5f, 0.2f, 0.0f, 1.0f ) );

        if( ImGui::Button( "Delete", ImVec2( 110, 0 ) ) )
        {
            DeletePath( deletePath );
            m_DeleteConfirmPath.clear( );
            ImGui::CloseCurrentPopup( );
        }

        ImGui::PopStyleColor( 3 );
        ImGui::SameLine( );
        if( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) )
        {
            m_DeleteConfirmPath.clear( );
            ImGui::CloseCurrentPopup( );
        }

        ImGui::EndPopup( );
    }

    ImGui::End( );
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
        entry.m_FilePath = MakePreferredPathString( entry.m_FilePath );
        m_History.Push( std::move( entry ) );
    }

    // Follow the newest downloaded item so the tree highlight matches the preview.
    SetSelectionFromHistoryIndex( m_History.GetSize( ) - 1, true, false );

    Save( );
}

void ImageScraper::DownloadHistoryPanel::FlushDecodedThumbnails( )
{
    std::vector<DecodedThumbnail> decodedThumbnails;
    {
        std::lock_guard<std::mutex> lock( m_DecodedThumbnailMutex );
        if( m_DecodedThumbnails.empty( ) )
        {
            return;
        }

        decodedThumbnails.swap( m_DecodedThumbnails );
    }

    for( DecodedThumbnail& decoded : decodedThumbnails )
    {
        UploadDecodedThumbnail( std::move( decoded ) );
    }
}

void ImageScraper::DownloadHistoryPanel::Save( )
{
    if( !m_AppConfig )
    {
        return;
    }

    Json entries = Json::array( );
    for( const DownloadHistoryEntry& entry : m_History )
    {
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

    m_AppConfig->SetValue<Json>( kDownloadHistoryKey, entries );
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

    m_AppConfig->SetValue<std::string>( kDownloadsSelectedPathKey, m_SelectedPath );
    m_AppConfig->SetValue<std::string>( kLegacySelectedPathKey, m_SelectedPath );
    m_AppConfig->Serialise( );
}

void ImageScraper::DownloadHistoryPanel::SetSelection( const std::string& path, bool scrollToSelected, bool requestPreview )
{
    const std::string preferredPath = MakePreferredPathString( path );
    if( preferredPath.empty( ) || !std::filesystem::exists( preferredPath ) )
    {
        ClearSelection( requestPreview );
        return;
    }

    if( !m_DownloadsRoot.empty( ) && !IsPathWithinRoot( preferredPath, m_DownloadsRoot ) )
    {
        ClearSelection( requestPreview );
        return;
    }

    m_SelectedPath = preferredPath;
    m_ScrollToSelected = scrollToSelected;
    SyncHistorySelectionFromPath( );
    SaveSelectedPath( );

    if( !requestPreview || !m_OnPreviewRequested )
    {
        return;
    }

    std::error_code ec;
    if( std::filesystem::is_regular_file( std::filesystem::path{ preferredPath }, ec ) )
    {
        m_OnPreviewRequested( preferredPath );
        return;
    }

    m_OnPreviewRequested( "" );
}

void ImageScraper::DownloadHistoryPanel::SetSelectionFromHistoryIndex( int index, bool scrollToSelected, bool requestPreview )
{
    if( index < 0 || index >= m_History.GetSize( ) )
    {
        ClearSelection( requestPreview );
        return;
    }

    SetSelection( m_History[ index ].m_FilePath, scrollToSelected, requestPreview );
}

void ImageScraper::DownloadHistoryPanel::ClearSelection( bool requestPreview )
{
    m_SelectedPath.clear( );
    m_SelectedHistoryIndex = -1;
    m_ScrollToSelected = false;
    SaveSelectedPath( );

    if( requestPreview && m_OnPreviewRequested )
    {
        m_OnPreviewRequested( "" );
    }
}

void ImageScraper::DownloadHistoryPanel::SyncHistorySelectionFromPath( )
{
    m_SelectedHistoryIndex = FindHistoryIndexByPath( m_SelectedPath );
}

void ImageScraper::DownloadHistoryPanel::DeletePath( const std::filesystem::path& path )
{
    if( !CanDeletePath( path ) )
    {
        return;
    }

    std::error_code typeEc;
    const bool isDirectory = std::filesystem::is_directory( path, typeEc );
    const std::string pathString = MakePreferredPathString( path );
    const bool selectionImpacted = IsSelectedPath( path ) || HasSelectedDescendant( path );
    const int selectedHistoryIndex = selectionImpacted
        ? m_SelectedHistoryIndex
        : FindHistoryIndexByPath( pathString );

    if( isDirectory )
    {
        if( m_OnPreviewRequested )
        {
            m_OnPreviewRequested( "" );
        }
    }
    else if( m_OnReleaseRequested )
    {
        m_OnReleaseRequested( pathString );
    }

    std::error_code ec;
    if( isDirectory )
    {
        std::filesystem::remove_all( path, ec );
    }
    else
    {
        std::filesystem::remove( path, ec );
    }

    if( ec )
    {
        WarningLog( "[%s] Failed to delete %s: %s",
                    __FUNCTION__,
                    pathString.c_str( ),
                    ec.message( ).c_str( ) );
        return;
    }

    if( !isDirectory )
    {
        EvictThumbnail( pathString );
    }

    m_SelectedHistoryIndex = selectedHistoryIndex;
    RemoveEntriesInPath( path, isDirectory );
    if( selectionImpacted )
    {
        m_SelectedPath.clear( );
        m_SelectedHistoryIndex = selectedHistoryIndex;
    }

    AdvanceSelectionAndPreview( );
}

void ImageScraper::DownloadHistoryPanel::AdvanceSelectionAndPreview( )
{
    const int size = m_History.GetSize( );
    if( size == 0 )
    {
        ClearSelection( true );
        return;
    }

    const int start =
        ( m_SelectedHistoryIndex >= 0 )
        ? std::min( m_SelectedHistoryIndex, size - 1 )
        : size - 1;

    const int olderIndex = FindExistingHistoryIndexAtOrBefore( start );
    if( olderIndex >= 0 )
    {
        SetSelectionFromHistoryIndex( olderIndex, false, true );
        return;
    }

    const int newerIndex = FindExistingHistoryIndexAfter( start );
    if( newerIndex >= 0 )
    {
        SetSelectionFromHistoryIndex( newerIndex, false, true );
        return;
    }

    ClearSelection( true );
}

void ImageScraper::DownloadHistoryPanel::RemoveEntriesInPath( const std::filesystem::path& targetPath, bool treatAsDirectory )
{
    if( targetPath.empty( ) )
    {
        return;
    }

    const std::filesystem::path normalisedTarget = NormalisePath( targetPath );
    bool changed = false;
    std::vector<int> indicesToRemove{ };

    int historyIndex = m_History.GetSize( ) - 1;
    for( auto it = m_History.rbegin( ); it != m_History.rend( ); ++it, --historyIndex )
    {
        const std::filesystem::path entryPath = it->m_FilePath;
        const std::filesystem::path normalisedEntry = NormalisePath( entryPath );
        const bool matchesTarget =
            treatAsDirectory
            ? IsPathWithinRoot( normalisedEntry, normalisedTarget )
            : ( normalisedEntry == normalisedTarget );

        if( !matchesTarget || std::filesystem::exists( entryPath ) )
        {
            continue;
        }

        EvictThumbnail( it->m_FilePath );
        indicesToRemove.push_back( historyIndex );
        changed = true;
    }

    for( const int index : indicesToRemove )
    {
        m_History.RemoveAt( index );
    }

    if( m_SelectedHistoryIndex >= m_History.GetSize( ) )
    {
        m_SelectedHistoryIndex = -1;
    }

    if( changed )
    {
        Save( );
    }
}

void ImageScraper::DownloadHistoryPanel::RenderTreeNode( const std::filesystem::path& path, bool* openDeleteConfirm )
{
    std::error_code ec;
    if( !std::filesystem::exists( path, ec ) )
    {
        return;
    }

    const bool isDirectory = std::filesystem::is_directory( path, ec );
    if( ec )
    {
        return;
    }

    ImGui::TableNextRow( );
    ImGui::TableNextColumn( );

    if( isDirectory && HasSelectedDescendant( path ) && m_ScrollToSelected )
    {
        ImGui::SetNextItemOpen( true, ImGuiCond_Always );
    }

    std::string label = path.filename( ).string( );
    if( label.empty( ) )
    {
        label = MakePreferredPathString( path );
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_SpanAllColumns |
        ImGuiTreeNodeFlags_DrawLinesFull |
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_FramePadding |
        ImGuiTreeNodeFlags_NavLeftJumpsToParent;
    if( isDirectory )
    {
        if( IsRootPath( path ) )
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
    }
    else
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if( IsSelectedPath( path ) )
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const std::string pathString = MakePreferredPathString( path );

    ImGui::PushID( pathString.c_str( ) );
    const bool isOpen = ImGui::TreeNodeEx( label.c_str( ), flags );

    if( ImGui::IsItemClicked( ImGuiMouseButton_Left ) )
    {
        SetSelection( pathString, false, true );
    }

    if( IsSelectedPath( path ) && m_ScrollToSelected )
    {
        ImGui::SetScrollHereY( 0.5f );
        m_ScrollToSelected = false;
    }

    if( ImGui::IsItemHovered( ) && !ImGui::IsPopupOpen( nullptr, ImGuiPopupFlags_AnyPopupId ) )
    {
        ShowPathTooltip( path );
    }

    ShowPathContextMenu( path, openDeleteConfirm );

    ImGui::TableNextColumn( );
    ImGui::AlignTextToFramePadding( );
    if( isDirectory )
    {
        ImGui::TextDisabled( "--" );
    }
    else
    {
        ImGui::TextUnformatted( GetSizeColumnLabel( path ).c_str( ) );
    }

    ImGui::TableNextColumn( );
    ImGui::AlignTextToFramePadding( );
    ImGui::TextUnformatted( GetTypeColumnLabel( path ).c_str( ) );

    ImGui::TableNextColumn( );
    ImGui::AlignTextToFramePadding( );
    ImGui::TextUnformatted( GetCreationTimeColumnLabel( path ).c_str( ) );

    if( isDirectory && isOpen )
    {
        std::vector<std::filesystem::path> children{ };
        for( std::filesystem::directory_iterator it{ path, std::filesystem::directory_options::skip_permission_denied, ec }, end;
             !ec && it != end;
             it.increment( ec ) )
        {
            children.push_back( it->path( ) );
        }

        const ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs( );
        const ImGuiTableColumnSortSpecs* primarySortSpec =
            ( sortSpecs != nullptr && sortSpecs->SpecsCount > 0 )
            ? &sortSpecs->Specs[ 0 ]
            : nullptr;

        std::sort( children.begin( ), children.end( ), [ primarySortSpec, this ]( const auto& lhs, const auto& rhs )
        {
            std::error_code lhsEc;
            std::error_code rhsEc;
            const bool lhsIsDirectory = std::filesystem::is_directory( lhs, lhsEc );
            const bool rhsIsDirectory = std::filesystem::is_directory( rhs, rhsEc );
            if( lhsIsDirectory != rhsIsDirectory )
            {
                return lhsIsDirectory > rhsIsDirectory;
            }

            int comparison = 0;
            if( primarySortSpec != nullptr )
            {
                switch( static_cast<SortColumn>( primarySortSpec->ColumnUserID ) )
                {
                    case SortColumn::Name:
                        comparison = CompareStringsCaseInsensitive(
                            GetPathDisplayName( lhs ),
                            GetPathDisplayName( rhs ) );
                        break;

                    case SortColumn::Size:
                    {
                        const uintmax_t lhsSize = GetPathFileSizeBytes( lhs );
                        const uintmax_t rhsSize = GetPathFileSizeBytes( rhs );
                        comparison = ( lhsSize < rhsSize ) ? -1 : ( lhsSize > rhsSize ? 1 : 0 );
                        break;
                    }

                    case SortColumn::Type:
                        comparison = CompareStringsCaseInsensitive(
                            GetTypeColumnLabel( lhs ),
                            GetTypeColumnLabel( rhs ) );
                        break;

                    case SortColumn::Created:
                    {
                        const unsigned long long lhsCreated = GetPathCreationTimeTicks( lhs );
                        const unsigned long long rhsCreated = GetPathCreationTimeTicks( rhs );
                        comparison = ( lhsCreated < rhsCreated ) ? -1 : ( lhsCreated > rhsCreated ? 1 : 0 );
                        break;
                    }
                }

                if( primarySortSpec->SortDirection == ImGuiSortDirection_Descending )
                {
                    comparison = -comparison;
                }
            }

            if( comparison == 0 )
            {
                comparison = CompareStringsCaseInsensitive(
                    GetPathDisplayName( lhs ),
                    GetPathDisplayName( rhs ) );
            }

            return comparison < 0;
        } );

        for( const auto& child : children )
        {
            RenderTreeNode( child, openDeleteConfirm );
        }

        ImGui::TreePop( );
    }

    ImGui::PopID( );
}

void ImageScraper::DownloadHistoryPanel::ShowPathContextMenu( const std::filesystem::path& path, bool* openDeleteConfirm )
{
    if( !ImGui::BeginPopupContextItem( "DownloadsContextMenu" ) )
    {
        return;
    }

    const std::string pathString = MakePreferredPathString( path );
    if( !IsSelectedPath( path ) )
    {
        SetSelection( pathString, false, false );
    }

    if( ImGui::MenuItem( "Open in Explorer" ) )
    {
        OpenInExplorer( path );
        ImGui::CloseCurrentPopup( );
    }

    ImGui::Separator( );
    if( ImGui::MenuItem( "Delete", nullptr, false, CanDeletePath( path ) ) )
    {
        m_DeleteConfirmPath = pathString;
        *openDeleteConfirm = true;
        ImGui::CloseCurrentPopup( );
    }

    ImGui::EndPopup( );
}

void ImageScraper::DownloadHistoryPanel::ShowPathTooltip( const std::filesystem::path& path )
{
    const std::string pathString = MakePreferredPathString( path );
    std::error_code ec;
    const bool isDirectory = std::filesystem::is_directory( path, ec );

    ImGui::BeginTooltip( );

    if( isDirectory )
    {
        ImGui::TextUnformatted( path.filename( ).string( ).c_str( ) );
        ImGui::Separator( );
        ImGui::TextDisabled( "%s", pathString.c_str( ) );
        ImGui::EndTooltip( );
        return;
    }

    const ThumbnailEntry thumb = GetOrLoadThumbnail( pathString );
    float displayWidth = k_TooltipMaxSize;
    if( thumb.m_Texture != 0 )
    {
        displayWidth = static_cast<float>( thumb.m_Width );
        float displayHeight = static_cast<float>( thumb.m_Height );
        if( displayWidth > k_TooltipMaxSize )
        {
            const float scale = k_TooltipMaxSize / displayWidth;
            displayWidth *= scale;
            displayHeight *= scale;
        }

        ImGui::Image( static_cast<ImTextureID>( thumb.m_Texture ), ImVec2( displayWidth, displayHeight ) );
        ImGui::Separator( );
    }
    else if( thumb.m_IsLoading )
    {
        ImGui::TextDisabled( "Loading preview..." );
        ImGui::Separator( );
    }

    ImGui::TextUnformatted( ExtractFileName( pathString ).c_str( ) );
    ImGui::TextDisabled( "%s", GetFileTypeLabel( pathString ).c_str( ) );

    const DownloadHistoryEntry* entry = FindHistoryEntryByPath( pathString );
    if( entry != nullptr )
    {
        ImGui::TextDisabled( "Downloaded: %s", entry->m_Timestamp.c_str( ) );
        ImGui::TextDisabled( "Size: %s", entry->m_FileSize.c_str( ) );
        if( !entry->m_SourceUrl.empty( ) )
        {
            ImGui::Separator( );
            ImGui::PushTextWrapPos( ImGui::GetCursorPosX( ) + std::max( displayWidth, 260.0f ) );
            ImGui::TextUnformatted( entry->m_SourceUrl.c_str( ) );
            ImGui::PopTextWrapPos( );
        }
    }
    else
    {
        ImGui::TextDisabled( "Size: %s", FormatFileSize( pathString ).c_str( ) );
    }

    ImGui::EndTooltip( );
}

bool ImageScraper::DownloadHistoryPanel::IsSelectedPath( const std::filesystem::path& path ) const
{
    return !m_SelectedPath.empty( ) && m_SelectedPath == MakePreferredPathString( path );
}

bool ImageScraper::DownloadHistoryPanel::HasSelectedDescendant( const std::filesystem::path& path ) const
{
    if( m_SelectedPath.empty( ) )
    {
        return false;
    }

    return IsPathWithinRoot( m_SelectedPath, path );
}

bool ImageScraper::DownloadHistoryPanel::CanDeletePath( const std::filesystem::path& path ) const
{
    if( m_Blocked || path.empty( ) || m_DownloadsRoot.empty( ) || IsRootPath( path ) )
    {
        return false;
    }

    std::error_code ec;
    if( !std::filesystem::exists( path, ec ) || ec )
    {
        return false;
    }

    return IsPathWithinRoot( path, m_DownloadsRoot );
}

bool ImageScraper::DownloadHistoryPanel::IsRootPath( const std::filesystem::path& path ) const
{
    return !m_DownloadsRoot.empty( ) && NormalisePath( path ) == NormalisePath( m_DownloadsRoot );
}

const ImageScraper::DownloadHistoryEntry* ImageScraper::DownloadHistoryPanel::FindHistoryEntryByPath( const std::string& filepath ) const
{
    for( const DownloadHistoryEntry& entry : m_History )
    {
        if( entry.m_FilePath == filepath )
        {
            return &entry;
        }
    }

    return nullptr;
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

void ImageScraper::DownloadHistoryPanel::SelectNext( )
{
    if( m_SelectedHistoryIndex < 0 )
    {
        return;
    }

    const int nextIndex = FindExistingHistoryIndexAtOrBefore( m_SelectedHistoryIndex - 1 );
    if( nextIndex >= 0 )
    {
        SetSelectionFromHistoryIndex( nextIndex, true, true );
    }
}

void ImageScraper::DownloadHistoryPanel::SelectPrevious( )
{
    if( m_SelectedHistoryIndex < 0 )
    {
        return;
    }

    const int previousIndex = FindExistingHistoryIndexAfter( m_SelectedHistoryIndex );
    if( previousIndex >= 0 )
    {
        SetSelectionFromHistoryIndex( previousIndex, true, true );
    }
}

bool ImageScraper::DownloadHistoryPanel::HasNext( ) const
{
    return m_SelectedHistoryIndex >= 0
        && FindExistingHistoryIndexAtOrBefore( m_SelectedHistoryIndex - 1 ) >= 0;
}

bool ImageScraper::DownloadHistoryPanel::HasPrevious( ) const
{
    return m_SelectedHistoryIndex >= 0
        && FindExistingHistoryIndexAfter( m_SelectedHistoryIndex ) >= 0;
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

void ImageScraper::DownloadHistoryPanel::Load( std::shared_ptr<JsonFile> appConfig, const std::filesystem::path& downloadsRoot )
{
    m_AppConfig = std::move( appConfig );
    m_DownloadsRoot = downloadsRoot;
    if( !m_AppConfig )
    {
        return;
    }

    Json entries;
    if( !m_AppConfig->GetValue<Json>( kDownloadHistoryKey, entries ) || !entries.is_array( ) )
    {
        entries = Json::array( );
    }

    for( const auto& obj : entries )
    {
        DownloadHistoryEntry entry;
        entry.m_FilePath  = MakePreferredPathString( obj.value( "file_path", "" ) );
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

    std::string selectedPath;
    if( !m_AppConfig->GetValue<std::string>( kDownloadsSelectedPathKey, selectedPath ) || selectedPath.empty( ) )
    {
        m_AppConfig->GetValue<std::string>( kLegacySelectedPathKey, selectedPath );
    }

    selectedPath = MakePreferredPathString( selectedPath );
    if( !selectedPath.empty( ) && std::filesystem::exists( selectedPath ) )
    {
        SetSelection( selectedPath, true, true );
        return;
    }

    if( m_History.IsEmpty( ) )
    {
        return;
    }

    SetSelectionFromHistoryIndex( m_History.GetSize( ) - 1, true, true );
}

int ImageScraper::DownloadHistoryPanel::FindHistoryIndexByPath( const std::string& filepath ) const
{
    int historyIndex = 0;
    for( const DownloadHistoryEntry& entry : m_History )
    {
        if( entry.m_FilePath == filepath )
        {
            return historyIndex;
        }

        ++historyIndex;
    }

    return -1;
}

int ImageScraper::DownloadHistoryPanel::FindExistingHistoryIndexAtOrBefore( int startIndex ) const
{
    if( startIndex < 0 )
    {
        return -1;
    }

    int historyIndex = m_History.GetSize( ) - 1;
    for( auto it = m_History.crbegin( ); it != m_History.crend( ); ++it, --historyIndex )
    {
        if( historyIndex > startIndex )
        {
            continue;
        }

        if( std::filesystem::exists( it->m_FilePath ) )
        {
            return historyIndex;
        }
    }

    return -1;
}

int ImageScraper::DownloadHistoryPanel::FindExistingHistoryIndexAfter( int startIndex ) const
{
    int historyIndex = 0;
    for( const DownloadHistoryEntry& entry : m_History )
    {
        if( historyIndex <= startIndex )
        {
            ++historyIndex;
            continue;
        }

        if( std::filesystem::exists( entry.m_FilePath ) )
        {
            return historyIndex;
        }

        ++historyIndex;
    }

    return -1;
}

void ImageScraper::DownloadHistoryPanel::OpenInExplorer( const std::filesystem::path& path )
{
    std::filesystem::path nativePath = path;
    nativePath.make_preferred( );

    std::error_code ec;
    if( std::filesystem::is_directory( path, ec ) )
    {
        ShellExecuteW( nullptr, L"open", nativePath.wstring( ).c_str( ), nullptr, nullptr, SW_SHOW );
        return;
    }

    const std::wstring args = L"/select,\"" + nativePath.wstring( ) + L"\"";
    ShellExecuteW( nullptr, L"open", L"explorer.exe", args.c_str( ), nullptr, SW_SHOW );
}

std::string ImageScraper::DownloadHistoryPanel::FormatTimestamp( )
{
    const auto now = std::chrono::system_clock::now( );
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

std::string ImageScraper::DownloadHistoryPanel::GetFileTypeLabel( const std::string& filepath )
{
    std::string extension = std::filesystem::path( filepath ).extension( ).string( );
    if( extension.empty( ) )
    {
        return "-";
    }

    if( extension.front( ) == '.' )
    {
        extension.erase( extension.begin( ) );
    }

    std::transform( extension.begin( ), extension.end( ), extension.begin( ), []( unsigned char c )
    {
        return static_cast<char>( std::toupper( c ) );
    } );

    return extension.empty( ) ? "-" : extension;
}

std::string ImageScraper::DownloadHistoryPanel::MakePreferredPathString( const std::filesystem::path& path )
{
    if( path.empty( ) )
    {
        return { };
    }

    return NormalisePath( path ).string( );
}

bool ImageScraper::DownloadHistoryPanel::IsPathWithinRoot( const std::filesystem::path& path, const std::filesystem::path& root )
{
    if( path.empty( ) || root.empty( ) )
    {
        return false;
    }

    const std::filesystem::path normalisedPath = NormalisePath( path );
    const std::filesystem::path normalisedRoot = NormalisePath( root );

    auto pathIt = normalisedPath.begin( );
    auto rootIt = normalisedRoot.begin( );
    for( ; rootIt != normalisedRoot.end( ); ++rootIt, ++pathIt )
    {
        if( pathIt == normalisedPath.end( ) || *pathIt != *rootIt )
        {
            return false;
        }
    }

    return true;
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

    m_ThumbnailCache[ filepath ].m_IsLoading = true;
    RequestThumbnailLoad( filepath );
    return m_ThumbnailCache[ filepath ];
}

void ImageScraper::DownloadHistoryPanel::RequestThumbnailLoad( const std::string& filepath )
{
    if( m_InFlightThumbnails.count( filepath ) > 0 )
    {
        return;
    }

    m_InFlightThumbnails.insert( filepath );
    m_ThumbnailFutures[ filepath ] = std::async( std::launch::async, [ this, filepath ]( )
    {
        DecodedThumbnail decoded = DecodeThumbnail( filepath );

        std::lock_guard<std::mutex> lock( m_DecodedThumbnailMutex );
        m_DecodedThumbnails.push_back( std::move( decoded ) );
    } );
}

void ImageScraper::DownloadHistoryPanel::UploadDecodedThumbnail( DecodedThumbnail&& decoded )
{
    m_InFlightThumbnails.erase( decoded.m_FilePath );
    m_ThumbnailFutures.erase( decoded.m_FilePath );

    auto it = m_ThumbnailCache.find( decoded.m_FilePath );
    if( it == m_ThumbnailCache.end( ) )
    {
        return;
    }

    if( decoded.m_PixelData.empty( ) )
    {
        it->second = ThumbnailEntry{ };
        return;
    }

    GLuint tex = 0;
    glGenTextures( 1, &tex );
    glBindTexture( GL_TEXTURE_2D, tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, decoded.m_Width, decoded.m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, decoded.m_PixelData.data( ) );
    glBindTexture( GL_TEXTURE_2D, 0 );

    it->second = ThumbnailEntry{ tex, decoded.m_Width, decoded.m_Height, false };
}

ImageScraper::DownloadHistoryPanel::DecodedThumbnail ImageScraper::DownloadHistoryPanel::DecodeThumbnail( const std::string& filepath )
{
    if( IsVideoExtension( filepath ) )
    {
        return DecodeVideoThumbnail( filepath );
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
            return DecodedThumbnail{ filepath };
        }
    }

    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* data = stbi_load( filepath.c_str( ), &w, &h, &channels, STBI_rgb_alpha );
    if( !data )
    {
        return DecodedThumbnail{ filepath };
    }

    DecodedThumbnail decoded;
    decoded.m_FilePath = filepath;
    decoded.m_Width    = w;
    decoded.m_Height   = h;
    decoded.m_PixelData.assign( data, data + static_cast<size_t>( w ) * static_cast<size_t>( h ) * 4 );

    stbi_image_free( data );
    return decoded;
}

bool ImageScraper::DownloadHistoryPanel::IsSupportedMediaExtension( const std::string& filepath )
{
    std::string ext = std::filesystem::path( filepath ).extension( ).string( );
    std::transform( ext.begin( ), ext.end( ), ext.begin( ), []( unsigned char c )
    {
        return static_cast<char>( std::tolower( c ) );
    } );

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
    std::transform( ext.begin( ), ext.end( ), ext.begin( ), []( unsigned char c )
    {
        return static_cast<char>( std::tolower( c ) );
    } );

    static const std::unordered_set<std::string> k_Video =
    {
        ".mp4", ".webm", ".mov", ".mkv", ".avi"
    };

    return k_Video.count( ext ) > 0;
}

ImageScraper::DownloadHistoryPanel::DecodedThumbnail ImageScraper::DownloadHistoryPanel::DecodeVideoThumbnail( const std::string& filepath )
{
    DecodedThumbnail decoded;
    decoded.m_FilePath = filepath;

    VideoPlayer player;
    if( !player.Open( filepath ) )
    {
        return decoded;
    }

    std::vector<uint8_t> rgba;
    if( !player.DecodeNextFrame( rgba ) )
    {
        return decoded;
    }

    decoded.m_Width  = player.GetWidth( );
    decoded.m_Height = player.GetHeight( );

    const size_t pixelBytes = static_cast<size_t>( decoded.m_Width ) * static_cast<size_t>( decoded.m_Height ) * 4;
    if( rgba.size( ) > pixelBytes )
    {
        rgba.resize( pixelBytes );
    }

    decoded.m_PixelData = std::move( rgba );
    return decoded;
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
    else if( bytes < 1024ull * 1024ull * 1024ull )
    {
        ss << std::fixed << std::setprecision( 1 ) << ( bytes / ( 1024.0 * 1024.0 ) ) << " MB";
    }
    else
    {
        ss << std::fixed << std::setprecision( 1 ) << ( bytes / ( 1024.0 * 1024.0 * 1024.0 ) ) << " GB";
    }

    return ss.str( );
}

std::string ImageScraper::DownloadHistoryPanel::GetSizeColumnLabel( const std::filesystem::path& path )
{
    return FormatFileSize( path.string( ) );
}

std::string ImageScraper::DownloadHistoryPanel::GetTypeColumnLabel( const std::filesystem::path& path )
{
    std::error_code ec;
    if( std::filesystem::is_directory( path, ec ) )
    {
        return "Folder";
    }

    std::string extension = path.extension( ).string( );
    std::transform( extension.begin( ), extension.end( ), extension.begin( ), []( unsigned char c )
    {
        return static_cast<char>( std::tolower( c ) );
    } );

    static const std::unordered_set<std::string> imageTypes =
    {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tga", ".webp"
    };

    static const std::unordered_set<std::string> audioTypes =
    {
        ".wav", ".mp3", ".flac", ".ogg", ".aac", ".m4a"
    };

    static const std::unordered_set<std::string> videoTypes =
    {
        ".mp4", ".webm", ".mov", ".mkv", ".avi"
    };

    static const std::unordered_set<std::string> systemTypes =
    {
        ".ini", ".sys", ".dll", ".exe", ".bat", ".cmd"
    };

    if( imageTypes.count( extension ) > 0 )
    {
        return "Image file";
    }

    if( audioTypes.count( extension ) > 0 )
    {
        return "Audio file";
    }

    if( videoTypes.count( extension ) > 0 )
    {
        return "Video file";
    }

    if( systemTypes.count( extension ) > 0 )
    {
        return "System file";
    }

    if( extension.empty( ) )
    {
        return "File";
    }

    if( extension.front( ) == '.' )
    {
        extension.erase( extension.begin( ) );
    }

    if( extension.empty( ) )
    {
        return "File";
    }

    std::transform( extension.begin( ), extension.end( ), extension.begin( ), []( unsigned char c )
    {
        return static_cast<char>( std::toupper( c ) );
    } );

    return extension + " file";
}

std::string ImageScraper::DownloadHistoryPanel::GetCreationTimeColumnLabel( const std::filesystem::path& path )
{
    WIN32_FILE_ATTRIBUTE_DATA attributeData{ };
    if( !GetFileAttributesExW( path.wstring( ).c_str( ), GetFileExInfoStandard, &attributeData ) )
    {
        return "--";
    }

    SYSTEMTIME utcTime{ };
    if( !FileTimeToSystemTime( &attributeData.ftCreationTime, &utcTime ) )
    {
        return "--";
    }

    SYSTEMTIME localTime{ };
    if( !SystemTimeToTzSpecificLocalTime( nullptr, &utcTime, &localTime ) )
    {
        localTime = utcTime;
    }

    char buffer[ 32 ]{ };
    sprintf_s(
        buffer,
        "%04d-%02d-%02d %02d:%02d",
        localTime.wYear,
        localTime.wMonth,
        localTime.wDay,
        localTime.wHour,
        localTime.wMinute );

    return buffer;
}
