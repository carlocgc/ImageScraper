#include "ui\SettingsPanel.h"

#include "async\TaskManager.h"
#include "config\DownloadLocationConfig.h"
#include "log\Logger.h"
#include "network\CurlHttpClient.h"
#include "services\ServiceOptionTypes.h"
#include "version\Version.h"

#include "imgui\imgui.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <shellapi.h>
    #include <shobjidl_core.h>
#endif

#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

namespace
{
    constexpr uintmax_t k_CopyBufferBytes = 1024 * 1024;
    constexpr uintmax_t k_MinFreeSpaceBufferBytes = 1024ull * 1024ull * 1024ull;
    constexpr int k_MinOperationStageMs = 750;

    bool DirectoryHasEntries( const std::filesystem::path& path )
    {
        std::error_code ec;
        if( !std::filesystem::exists( path, ec ) || ec || !std::filesystem::is_directory( path, ec ) || ec )
        {
            return false;
        }

        std::filesystem::directory_iterator it{ path, std::filesystem::directory_options::skip_permission_denied, ec };
        return !ec && it != std::filesystem::directory_iterator{ };
    }

    std::vector<std::filesystem::path> GetManagedProviderRoots( const std::filesystem::path& downloadRoot )
    {
        std::vector<std::filesystem::path> providerRoots{ };
        providerRoots.reserve( ImageScraper::CONTENT_PROVIDERS_COUNT );
        for( int providerIndex = 0; providerIndex < ImageScraper::CONTENT_PROVIDERS_COUNT; ++providerIndex )
        {
            providerRoots.push_back( downloadRoot / ImageScraper::s_ContentProviderStrings[ providerIndex ] );
        }

        return providerRoots;
    }

    bool DirectoryHasManagedDownloadEntries( const std::filesystem::path& downloadRoot )
    {
        for( const std::filesystem::path& providerRoot : GetManagedProviderRoots( downloadRoot ) )
        {
            if( DirectoryHasEntries( providerRoot ) )
            {
                return true;
            }
        }

        return false;
    }
}

ImageScraper::SettingsPanel::SettingsPanel(
    std::shared_ptr<JsonFile> appConfig,
    std::string caBundlePath,
    std::filesystem::path defaultDownloadRoot,
    std::filesystem::path initialDownloadRoot,
    DownloadRootChangedCallback onDownloadRootChanged )
    : m_AppConfig{ std::move( appConfig ) },
      m_CaBundlePath{ std::move( caBundlePath ) },
      m_DefaultDownloadRoot{ DownloadLocationConfig::NormalisePath( defaultDownloadRoot ) },
      m_DownloadRoot{ DownloadLocationConfig::NormalisePath( initialDownloadRoot ) },
      m_OnDownloadRootChanged{ std::move( onDownloadRootChanged ) }
{
    LoadState( );
    m_DownloadLocationResolved = IsDownloadRootAvailableOrCreatable( m_DownloadRoot );
    if( !m_DownloadLocationResolved )
    {
        m_ShowMissingDownloadLocationPopup = true;
        m_DownloadLocationError = "The saved download location could not be found.";
    }
}

bool ImageScraper::SettingsPanel::IsLocationOperationActive( ) const
{
    const DownloadLocationOperationStage stage = GetOperationStage( );
    return stage != DownloadLocationOperationStage::Idle
        && stage != DownloadLocationOperationStage::DeleteComplete
        && stage != DownloadLocationOperationStage::DeleteCancelled;
}

void ImageScraper::SettingsPanel::LoadState( )
{
    if( !m_AppConfig )
    {
        return;
    }

    if( !m_AppConfig->GetValue<bool>( s_CheckUpdatesOnStartupConfigKey, m_CheckUpdatesOnStartup ) )
    {
        m_CheckUpdatesOnStartup = false;
        m_AppConfig->SetValue<bool>( s_CheckUpdatesOnStartupConfigKey, m_CheckUpdatesOnStartup );
        ( void )m_AppConfig->Serialise( );
    }

    if( !m_AppConfig->GetValue<std::string>( s_LastUpdatePromptedVersionConfigKey, m_LastUpdatePromptedVersion ) )
    {
        m_LastUpdatePromptedVersion.clear( );
        m_AppConfig->SetValue<std::string>( s_LastUpdatePromptedVersionConfigKey, m_LastUpdatePromptedVersion );
        ( void )m_AppConfig->Serialise( );
    }

    if( !m_AppConfig->GetValue<std::string>( s_SkippedUpdateVersionConfigKey, m_SkippedUpdateVersion ) )
    {
        m_SkippedUpdateVersion.clear( );
        m_AppConfig->SetValue<std::string>( s_SkippedUpdateVersionConfigKey, m_SkippedUpdateVersion );
        ( void )m_AppConfig->Serialise( );
    }

#ifdef _DEBUG
    if( !m_AppConfig->GetValue<bool>( s_CheckDevelopmentVersionConfigKey, m_CheckDevelopmentVersion ) )
    {
        m_CheckDevelopmentVersion = false;
        m_AppConfig->SetValue<bool>( s_CheckDevelopmentVersionConfigKey, m_CheckDevelopmentVersion );
        ( void )m_AppConfig->Serialise( );
    }
#endif
}

void ImageScraper::SettingsPanel::Update( )
{
    if( m_DownloadLocationResolved && !IsDownloadRootAvailableOrCreatable( m_DownloadRoot ) )
    {
        m_DownloadLocationResolved = false;
        m_ShowMissingDownloadLocationPopup = true;
        m_DownloadLocationError = "The saved download location could not be found.";
    }

    if( !m_StartupCheckHandled )
    {
        m_StartupCheckHandled = true;
        if( m_CheckUpdatesOnStartup )
        {
            StartUpdateCheck( false );
        }
    }

    ImGui::SetNextWindowSize( ImVec2( 420, 220 ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "Settings", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    ImGui::Text( "Application" );
    ImGui::Separator( );
    ImGui::Text( "Current version: %s", VERSION_STRING );
    ImGui::Spacing( );

    DrawDownloadsSettings( );

    ImGui::Spacing( );
    ImGui::Separator( );
    ImGui::Spacing( );
    ImGui::Text( "Updates" );

    bool checkUpdatesOnStartup = m_CheckUpdatesOnStartup;
    if( ImGui::Checkbox( "Check for updates on startup", &checkUpdatesOnStartup ) )
    {
        m_CheckUpdatesOnStartup = checkUpdatesOnStartup;
        PersistCheckUpdatesOnStartup( );
    }

    const bool checkWasRunning = m_CheckRunning;
    if( checkWasRunning )
    {
        ImGui::BeginDisabled( );
    }
    if( ImGui::Button( "Check now" ) )
    {
        StartUpdateCheck( true );
    }
    if( checkWasRunning )
    {
        ImGui::EndDisabled( );
    }

    ImGui::SameLine( );
    DrawUpdateStatus( );

    if( m_UpdateStatus == SettingsUpdateStatus::UpdateAvailable && !m_LatestRelease.m_HtmlUrl.empty( ) )
    {
        if( ImGui::Button( "Open release page" ) )
        {
            if( OpenReleasePage( m_LatestRelease.m_HtmlUrl ) )
            {
                PersistStringValue( s_LastUpdatePromptedVersionConfigKey, m_LatestRelease.m_TagName );
            }
        }
    }

    if( m_UpdateStatus == SettingsUpdateStatus::Failed && !m_LastError.empty( ) )
    {
        ImGui::TextWrapped( "%s", m_LastError.c_str( ) );
    }

#ifdef _DEBUG
    ImGui::Spacing( );
    ImGui::Separator( );
    ImGui::Spacing( );
    ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.2f, 1.0f ), "Dev" );
    ImGui::SameLine( );
    bool checkDevelopmentVersion = m_CheckDevelopmentVersion;
    if( ImGui::Checkbox( "Allow update checks for dev version", &checkDevelopmentVersion ) )
    {
        m_CheckDevelopmentVersion = checkDevelopmentVersion;
        PersistCheckDevelopmentVersion( );
    }
    if( ImGui::IsItemHovered( ) )
    {
        ImGui::SetTooltip( "Treats the local dev fallback version as 0.0.0 so update notification UI can be tested.\nDebug builds only." );
    }
#endif

    DrawUpdatePopup( );
    DrawMissingDownloadLocationPopup( );
    DrawMoveDownloadsPopup( );
    DrawLocationOperationPopup( );

    ImGui::End( );
}

void ImageScraper::SettingsPanel::PersistCheckUpdatesOnStartup( )
{
    if( !m_AppConfig )
    {
        return;
    }

    m_AppConfig->SetValue<bool>( s_CheckUpdatesOnStartupConfigKey, m_CheckUpdatesOnStartup );
    if( !m_AppConfig->Serialise( ) )
    {
        LogError( "[%s] Failed to save update startup-check setting.", __FUNCTION__ );
    }
}

#ifdef _DEBUG
void ImageScraper::SettingsPanel::PersistCheckDevelopmentVersion( )
{
    if( !m_AppConfig )
    {
        return;
    }

    m_AppConfig->SetValue<bool>( s_CheckDevelopmentVersionConfigKey, m_CheckDevelopmentVersion );
    if( !m_AppConfig->Serialise( ) )
    {
        LogError( "[%s] Failed to save dev update-check setting.", __FUNCTION__ );
    }
}
#endif

void ImageScraper::SettingsPanel::PersistStringValue( const std::string& key, const std::string& value )
{
    if( !m_AppConfig )
    {
        return;
    }

    m_AppConfig->SetValue<std::string>( key, value );
    if( !m_AppConfig->Serialise( ) )
    {
        LogError( "[%s] Failed to save update setting: %s", __FUNCTION__, key.c_str( ) );
        return;
    }

    if( key == s_LastUpdatePromptedVersionConfigKey )
    {
        m_LastUpdatePromptedVersion = value;
    }
    else if( key == s_SkippedUpdateVersionConfigKey )
    {
        m_SkippedUpdateVersion = value;
    }
}

void ImageScraper::SettingsPanel::StartUpdateCheck( bool manual )
{
    if( m_CheckRunning )
    {
        return;
    }

    m_CheckRunning = true;
    m_UpdateStatus = SettingsUpdateStatus::Checking;
    m_LastError.clear( );

    UpdateCheckRequest request{ };
    request.m_CurrentVersion = VERSION_STRING;
    request.m_CaBundlePath = m_CaBundlePath;
    request.m_UserAgent = std::string( "ImageScraper/" ) + VERSION_STRING;
    request.m_TimeoutSeconds = 10;
#ifdef _DEBUG
    request.m_AllowDevelopmentVersion = m_CheckDevelopmentVersion;
#endif

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, request, manual ]( )
        {
            CurlHttpClient httpClient{ };
            UpdateCheckResult result = CheckForUpdates( httpClient, request );
            auto mainTask = TaskManager::Instance( ).SubmitMain( [ this, result, manual ]( )
                {
                    OnUpdateCheckComplete( result, manual );
                } );
            ( void )mainTask;
        } );
    ( void )task;
}

void ImageScraper::SettingsPanel::OnUpdateCheckComplete( const UpdateCheckResult& result, bool manual )
{
    m_CheckRunning = false;
    m_LatestRelease = result.m_LatestRelease;
    m_LastError = result.m_Error;

    switch( result.m_Status )
    {
    case UpdateCheckStatus::UpToDate:
        m_UpdateStatus = SettingsUpdateStatus::UpToDate;
        LogDebug( "[%s] ImageScraper is up to date.", __FUNCTION__ );
        break;
    case UpdateCheckStatus::UpdateAvailable:
        m_UpdateStatus = SettingsUpdateStatus::UpdateAvailable;
        LogDebug( "[%s] ImageScraper update available: %s", __FUNCTION__, result.m_LatestRelease.m_TagName.c_str( ) );
        if( manual || ShouldShowStartupPromptFor( result.m_LatestRelease ) )
        {
            m_ShowUpdatePopup = true;
        }
        break;
    case UpdateCheckStatus::Skipped:
        m_UpdateStatus = SettingsUpdateStatus::Skipped;
        LogDebug( "[%s] Update check skipped: %s", __FUNCTION__, result.m_Error.c_str( ) );
        break;
    case UpdateCheckStatus::Failed:
    default:
        m_UpdateStatus = SettingsUpdateStatus::Failed;
        LogError( "[%s] Update check failed: %s", __FUNCTION__, result.m_Error.c_str( ) );
        break;
    }
}

bool ImageScraper::SettingsPanel::ShouldShowStartupPromptFor( const GitHubReleaseInfo& release ) const
{
    return release.m_TagName != m_LastUpdatePromptedVersion
        && release.m_TagName != m_SkippedUpdateVersion;
}

void ImageScraper::SettingsPanel::DrawUpdateStatus( ) const
{
    switch( m_UpdateStatus )
    {
    case SettingsUpdateStatus::NotChecked:
        ImGui::TextUnformatted( "Not checked" );
        break;
    case SettingsUpdateStatus::Checking:
        ImGui::TextUnformatted( "Checking..." );
        break;
    case SettingsUpdateStatus::UpToDate:
        ImGui::TextUnformatted( "Up to date" );
        break;
    case SettingsUpdateStatus::UpdateAvailable:
        ImGui::Text( "Update available: %s", m_LatestRelease.m_TagName.c_str( ) );
        break;
    case SettingsUpdateStatus::Skipped:
        ImGui::TextUnformatted( "Dev update checks disabled" );
        break;
    case SettingsUpdateStatus::Failed:
        ImGui::TextUnformatted( "Update check failed" );
        break;
    }
}

void ImageScraper::SettingsPanel::DrawUpdatePopup( )
{
    if( m_ShowUpdatePopup )
    {
        ImGui::OpenPopup( "Update available" );
        m_ShowUpdatePopup = false;
    }

    if( ImGui::BeginPopupModal( "Update available", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        ImGui::Text( "ImageScraper %s is available.", m_LatestRelease.m_TagName.c_str( ) );
        ImGui::TextUnformatted( "Open the GitHub release page to download it." );

        if( ImGui::Button( "Open Release Page" ) )
        {
            if( OpenReleasePage( m_LatestRelease.m_HtmlUrl ) )
            {
                PersistStringValue( s_LastUpdatePromptedVersionConfigKey, m_LatestRelease.m_TagName );
            }
            ImGui::CloseCurrentPopup( );
        }

        ImGui::SameLine( );
        if( ImGui::Button( "Skip This Version" ) )
        {
            PersistStringValue( s_SkippedUpdateVersionConfigKey, m_LatestRelease.m_TagName );
            ImGui::CloseCurrentPopup( );
        }

        ImGui::SameLine( );
        if( ImGui::Button( "Later" ) )
        {
            ImGui::CloseCurrentPopup( );
        }

        ImGui::EndPopup( );
    }
}

bool ImageScraper::SettingsPanel::OpenReleasePage( const std::string& url ) const
{
#ifdef _WIN32
    const std::wstring wideUrl( url.begin( ), url.end( ) );
    constexpr intptr_t shellExecuteSuccessThreshold = 32;

    const HINSTANCE result = ::ShellExecuteW( nullptr, L"open", wideUrl.c_str( ), nullptr, nullptr, SW_SHOWNORMAL );
    if( reinterpret_cast<intptr_t>( result ) <= shellExecuteSuccessThreshold )
    {
        LogError( "[%s] Failed to open release page: %s", __FUNCTION__, url.c_str( ) );
        return false;
    }

    return true;
#else
    ( void )url;
    return false;
#endif
}

void ImageScraper::SettingsPanel::DrawDownloadsSettings( )
{
    ImGui::Text( "Downloads" );
    ImGui::Separator( );
    ImGui::TextUnformatted( "Location" );
    ImGui::SameLine( 110.0f );
    ImGui::PushTextWrapPos( ImGui::GetCursorPosX( ) + ImGui::GetContentRegionAvail( ).x );
    ImGui::TextDisabled( "%s", FormatPathForDisplay( m_DownloadRoot ).c_str( ) );
    ImGui::PopTextWrapPos( );

    if( !m_DownloadLocationResolved )
    {
        ImGui::TextColored( ImVec4( 1.0f, 0.55f, 0.15f, 1.0f ), "%s", m_DownloadLocationError.c_str( ) );
    }

    const bool locationOperationActive = IsLocationOperationActive( );
    if( m_Blocked || locationOperationActive )
    {
        ImGui::BeginDisabled( );
    }

    if( ImGui::Button( "Choose Location" ) )
    {
        ChooseDownloadLocation( );
    }

    ImGui::SameLine( );
    const bool usingDefault = DownloadLocationConfig::IsSamePath( m_DownloadRoot, m_DefaultDownloadRoot );
    if( usingDefault )
    {
        ImGui::BeginDisabled( );
    }
    if( ImGui::Button( "Use Default" ) )
    {
        UseDefaultDownloadLocation( );
    }
    if( usingDefault )
    {
        ImGui::EndDisabled( );
    }

    if( m_Blocked || locationOperationActive )
    {
        ImGui::EndDisabled( );
        if( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( locationOperationActive
                ? "Download location cannot be changed while a copy or delete operation is running."
                : "Download location cannot be changed while work is running." );
        }
    }
}

void ImageScraper::SettingsPanel::DrawMissingDownloadLocationPopup( )
{
    if( m_ShowMissingDownloadLocationPopup )
    {
        ImGui::OpenPopup( "Download location not found" );
        m_ShowMissingDownloadLocationPopup = false;
    }

    if( ImGui::BeginPopupModal( "Download location not found", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::TextWrapped( "The saved download location could not be found." );
        ImGui::Spacing( );
        ImGui::TextDisabled( "%s", FormatPathForDisplay( m_DownloadRoot ).c_str( ) );
        ImGui::Spacing( );

        ImGui::BeginDisabled( m_Blocked || IsLocationOperationActive( ) );
        if( ImGui::Button( "Choose Location", ImVec2( 130, 0 ) ) )
        {
            ChooseDownloadLocation( );
            if( m_DownloadLocationResolved || m_ShowMoveDownloadsPopup )
            {
                ImGui::CloseCurrentPopup( );
            }
        }

        ImGui::SameLine( );
        if( ImGui::Button( "Use Default", ImVec2( 110, 0 ) ) )
        {
            UseDefaultDownloadLocation( );
            ImGui::CloseCurrentPopup( );
        }
        ImGui::EndDisabled( );

        ImGui::SameLine( );
        if( ImGui::Button( "Cancel", ImVec2( 90, 0 ) ) )
        {
            ImGui::CloseCurrentPopup( );
        }

        ImGui::EndPopup( );
    }
}

void ImageScraper::SettingsPanel::DrawMoveDownloadsPopup( )
{
    if( m_ShowMoveDownloadsPopup )
    {
        ImGui::OpenPopup( "Copy existing downloads" );
        m_ShowMoveDownloadsPopup = false;
    }

    if( ImGui::BeginPopupModal( "Copy existing downloads", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::TextWrapped( "Copy existing provider downloads to the new location?" );
        ImGui::Spacing( );
        ImGui::TextDisabled( "From: %s", FormatPathForDisplay( m_DownloadRoot ).c_str( ) );
        ImGui::TextDisabled( "To:   %s", FormatPathForDisplay( m_PendingDownloadRoot ).c_str( ) );
        ImGui::Spacing( );

        if( ImGui::Button( "Copy", ImVec2( 90, 0 ) ) )
        {
            const std::filesystem::path previousRoot = m_DownloadRoot;
            const std::filesystem::path nextRoot = m_PendingDownloadRoot;
            m_PendingDownloadRoot.clear( );
            BeginCopyDownloadLocation( previousRoot, nextRoot );
            ImGui::CloseCurrentPopup( );
        }

        ImGui::SameLine( );
        if( ImGui::Button( "Leave", ImVec2( 90, 0 ) ) )
        {
            ApplyDownloadRoot( m_PendingDownloadRoot );
            m_PendingDownloadRoot.clear( );
            ImGui::CloseCurrentPopup( );
        }

        ImGui::SameLine( );
        if( ImGui::Button( "Cancel", ImVec2( 90, 0 ) ) )
        {
            m_PendingDownloadRoot.clear( );
            ImGui::CloseCurrentPopup( );
        }

        ImGui::EndPopup( );
    }
}

void ImageScraper::SettingsPanel::DrawLocationOperationPopup( )
{
    const DownloadLocationOperationStage stage = GetOperationStage( );
    if( stage == DownloadLocationOperationStage::Idle )
    {
        return;
    }

    ImGui::OpenPopup( "Download location update" );
    if( !ImGui::BeginPopupModal( "Download location update", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        return;
    }

    const LocationOperationProgress progress = GetOperationProgressSnapshot( );

    switch( stage )
    {
    case DownloadLocationOperationStage::Preparing:
    case DownloadLocationOperationStage::CheckingSpace:
    case DownloadLocationOperationStage::Copying:
    case DownloadLocationOperationStage::Deleting:
    {
        ImGui::TextWrapped( "%s", progress.m_Message.c_str( ) );
        ImGui::Spacing( );
        const float fraction = CalculateProgressFraction( progress );
        ImGui::ProgressBar( fraction, ImVec2( 420.0f, 0.0f ) );
        ImGui::TextDisabled(
            "%s / %s, %i / %i files",
            FormatBytes( progress.m_ProcessedBytes ).c_str( ),
            FormatBytes( progress.m_TotalBytes ).c_str( ),
            progress.m_ProcessedFiles,
            progress.m_TotalFiles );
        if( progress.m_SkippedFiles > 0 )
        {
            ImGui::TextDisabled( "Skipped: %i", progress.m_SkippedFiles );
        }
        if( !progress.m_CurrentPath.empty( ) )
        {
            ImGui::PushTextWrapPos( ImGui::GetCursorPosX( ) + 420.0f );
            ImGui::TextDisabled( "%s", progress.m_CurrentPath.c_str( ) );
            ImGui::PopTextWrapPos( );
        }

        ImGui::Spacing( );
        if( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) )
        {
            RequestLocationOperationCancel( );
        }
        break;
    }

    case DownloadLocationOperationStage::AwaitingUnknownSpaceConfirmation:
        ImGui::TextWrapped( "Available space at the new location could not be verified." );
        ImGui::TextDisabled( "Copy size: %s", FormatBytes( progress.m_TotalBytes ).c_str( ) );
        ImGui::TextDisabled( "Required with buffer: %s", FormatBytes( progress.m_RequiredBytes ).c_str( ) );
        ImGui::Spacing( );
        if( ImGui::Button( "Continue Copy", ImVec2( 120, 0 ) ) )
        {
            StartCopyWorker( true );
        }
        ImGui::SameLine( );
        if( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) )
        {
            SetOperationStage( DownloadLocationOperationStage::CopyCancelled, "Copy cancelled. The old download location is still active." );
        }
        break;

    case DownloadLocationOperationStage::CopyComplete:
        DrawCopyCompletePopup( progress );
        break;

    case DownloadLocationOperationStage::CopyCancelled:
        ImGui::TextWrapped( "%s", progress.m_Message.c_str( ) );
        ImGui::TextDisabled( "The active download location was not changed." );
        if( progress.m_ProcessedBytes > 0 || progress.m_ProcessedFiles > 0 )
        {
            ImGui::TextDisabled( "Partial copies may exist at the destination." );
        }
        ImGui::Spacing( );
        if( ImGui::Button( "OK", ImVec2( 100, 0 ) ) )
        {
            ResetLocationOperation( );
            ImGui::CloseCurrentPopup( );
        }
        break;

    case DownloadLocationOperationStage::DeleteComplete:
    case DownloadLocationOperationStage::DeleteCancelled:
        ImGui::TextWrapped( "%s", progress.m_Message.c_str( ) );
        ImGui::TextDisabled( "Current download location: %s", FormatPathForDisplay( m_DownloadRoot ).c_str( ) );
        ImGui::Spacing( );
        if( ImGui::Button( "OK", ImVec2( 100, 0 ) ) )
        {
            ResetLocationOperation( );
            ImGui::CloseCurrentPopup( );
        }
        break;

    case DownloadLocationOperationStage::Idle:
        break;
    }

    ImGui::EndPopup( );
}

void ImageScraper::SettingsPanel::DrawCopyCompletePopup( const LocationOperationProgress& progress )
{
    ImGui::TextWrapped( "Downloads were copied to the new location." );
    ImGui::Spacing( );
    ImGui::TextDisabled(
        "Copied: %i files, %s",
        progress.m_ProcessedFiles,
        FormatBytes( progress.m_ProcessedBytes ).c_str( ) );
    if( progress.m_SkippedFiles > 0 )
    {
        ImGui::TextDisabled( "Skipped because destination already existed: %i", progress.m_SkippedFiles );
    }
    if( progress.m_FailedFiles > 0 )
    {
        ImGui::TextDisabled( "Failed: %i", progress.m_FailedFiles );
    }
    ImGui::Spacing( );

    if( ImGui::Button( "Keep Originals", ImVec2( 120, 0 ) ) )
    {
        CompleteLocationChangeKeepingOriginals( );
        ImGui::CloseCurrentPopup( );
    }

    ImGui::SameLine( );
    const bool canDeleteOriginals = progress.m_SkippedFiles == 0 && progress.m_FailedFiles == 0;
    if( !canDeleteOriginals )
    {
        ImGui::BeginDisabled( );
    }
    if( ImGui::Button( "Delete Originals", ImVec2( 130, 0 ) ) )
    {
        ApplyDownloadRoot( m_OperationNextRoot );
        StartDeleteOriginalsWorker( );
    }
    if( !canDeleteOriginals )
    {
        ImGui::EndDisabled( );
        if( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( "Originals cannot be deleted because some files were skipped or failed." );
        }
    }
}

void ImageScraper::SettingsPanel::ChooseDownloadLocation( )
{
    if( m_Blocked || IsLocationOperationActive( ) )
    {
        return;
    }

    const std::optional<std::filesystem::path> selectedPath = PickFolder( m_DownloadRoot );
    if( !selectedPath.has_value( ) )
    {
        return;
    }

    const std::filesystem::path nextRoot = DownloadLocationConfig::NormalisePath( *selectedPath );
    RequestDownloadRootChange( nextRoot );
}

void ImageScraper::SettingsPanel::UseDefaultDownloadLocation( )
{
    RequestDownloadRootChange( m_DefaultDownloadRoot );
}

void ImageScraper::SettingsPanel::RequestDownloadRootChange( const std::filesystem::path& nextRoot )
{
    if( m_Blocked || IsLocationOperationActive( ) )
    {
        return;
    }

    const std::filesystem::path normalisedNextRoot = DownloadLocationConfig::NormalisePath( nextRoot );
    if( DownloadLocationConfig::IsSamePath( normalisedNextRoot, m_DownloadRoot ) )
    {
        return;
    }

    if( ShouldPromptToMoveDownloads( m_DownloadRoot, normalisedNextRoot ) )
    {
        m_PendingDownloadRoot = normalisedNextRoot;
        m_ShowMoveDownloadsPopup = true;
        return;
    }

    ApplyDownloadRoot( normalisedNextRoot );
}

void ImageScraper::SettingsPanel::ApplyDownloadRoot( const std::filesystem::path& downloadRoot )
{
    const std::filesystem::path normalisedRoot = DownloadLocationConfig::NormalisePath( downloadRoot );
    PersistDownloadRoot( normalisedRoot );
    m_DownloadRoot = normalisedRoot;
    m_DownloadLocationResolved = IsDownloadRootAvailableOrCreatable( m_DownloadRoot );
    m_DownloadLocationError = m_DownloadLocationResolved ? std::string{ } : "The saved download location could not be found.";

    if( m_OnDownloadRootChanged )
    {
        m_OnDownloadRootChanged( m_DownloadRoot );
    }
}

void ImageScraper::SettingsPanel::PersistDownloadRoot( const std::filesystem::path& downloadRoot )
{
    if( !m_AppConfig )
    {
        return;
    }

    if( DownloadLocationConfig::IsSamePath( downloadRoot, m_DefaultDownloadRoot ) )
    {
        m_AppConfig->RemoveValue( DownloadLocationConfig::s_DownloadLocationConfigKey );
    }
    else
    {
        m_AppConfig->SetValue<std::string>( DownloadLocationConfig::s_DownloadLocationConfigKey, downloadRoot.string( ) );
    }

    if( !m_AppConfig->Serialise( ) )
    {
        LogError( "[%s] Failed to save download location setting.", __FUNCTION__ );
    }
}

bool ImageScraper::SettingsPanel::ShouldPromptToMoveDownloads( const std::filesystem::path& previousRoot, const std::filesystem::path& nextRoot ) const
{
    if( previousRoot.empty( ) || nextRoot.empty( ) || DownloadLocationConfig::IsSamePath( previousRoot, nextRoot ) )
    {
        return false;
    }

    return DirectoryHasManagedDownloadEntries( previousRoot );
}

void ImageScraper::SettingsPanel::BeginCopyDownloadLocation( const std::filesystem::path& previousRoot, const std::filesystem::path& nextRoot )
{
    if( previousRoot.empty( ) || nextRoot.empty( ) || DownloadLocationConfig::IsSamePath( previousRoot, nextRoot ) )
    {
        ApplyDownloadRoot( nextRoot );
        return;
    }

    if( DownloadLocationConfig::IsPathWithinRoot( nextRoot, previousRoot ) )
    {
        LogError( "[%s] Refusing to copy downloads into a child of the current downloads folder: %s", __FUNCTION__, nextRoot.string( ).c_str( ) );
        return;
    }

    if( m_OnDownloadRootChanged )
    {
        m_OnDownloadRootChanged( m_DownloadRoot );
    }

    m_OperationPreviousRoot = previousRoot;
    m_OperationNextRoot = nextRoot;
    m_CopyPlan.clear( );
    m_CancelLocationOperation.store( false );
    UpdateOperationProgress( LocationOperationProgress{ } );
    SetOperationStage( DownloadLocationOperationStage::Preparing, "Preparing copy..." );
    StartCopyWorker( false );
}

void ImageScraper::SettingsPanel::StartCopyWorker( bool skipFreeSpaceCheck )
{
    m_CancelLocationOperation.store( false );
    m_OperationFuture = std::async( std::launch::async, [ this, skipFreeSpaceCheck ]( )
    {
        LocationOperationProgress progress = GetOperationProgressSnapshot( );
        std::vector<CopyPlanEntry> copyPlan;
        auto stageStartedAt = std::chrono::steady_clock::now( );

        if( skipFreeSpaceCheck )
        {
            std::lock_guard<std::mutex> lock( m_OperationMutex );
            copyPlan = m_CopyPlan;
        }
        else
        {
            stageStartedAt = std::chrono::steady_clock::now( );
            SetOperationStage( DownloadLocationOperationStage::Preparing, "Preparing copy..." );
            ScanCopyPlan( m_OperationPreviousRoot, m_OperationNextRoot, copyPlan, progress );
            WaitForMinimumOperationStep( stageStartedAt );
            if( m_CancelLocationOperation.load( ) || progress.m_FailedFiles > 0 )
            {
                SetOperationStage( DownloadLocationOperationStage::CopyCancelled, "Copy cancelled. The old download location is still active." );
                return;
            }

            {
                std::lock_guard<std::mutex> lock( m_OperationMutex );
                m_CopyPlan = copyPlan;
            }

            stageStartedAt = std::chrono::steady_clock::now( );
            SetOperationStage( DownloadLocationOperationStage::CheckingSpace, "Checking free space..." );
            if( !CheckFreeSpaceForCopy( m_OperationNextRoot, progress ) )
            {
                WaitForMinimumOperationStep( stageStartedAt );
                SetOperationStage( DownloadLocationOperationStage::CopyCancelled, progress.m_Message );
                return;
            }
            WaitForMinimumOperationStep( stageStartedAt );

            if( progress.m_Message == "Unknown free space" )
            {
                SetOperationStage( DownloadLocationOperationStage::AwaitingUnknownSpaceConfirmation, "Available space could not be verified." );
                return;
            }
        }

        stageStartedAt = std::chrono::steady_clock::now( );
        SetOperationStage( DownloadLocationOperationStage::Copying, "Copying downloads..." );
        CopyPlannedDownloads( copyPlan, progress );
        WaitForMinimumOperationStep( stageStartedAt );
        if( m_CancelLocationOperation.load( ) || progress.m_FailedFiles > 0 )
        {
            SetOperationStage( DownloadLocationOperationStage::CopyCancelled, "Copy cancelled. The old download location is still active." );
            return;
        }

        SetOperationStage( DownloadLocationOperationStage::CopyComplete, "Copy complete." );
    } );
}

void ImageScraper::SettingsPanel::StartDeleteOriginalsWorker( )
{
    m_CancelLocationOperation.store( false );
    UpdateOperationProgress( LocationOperationProgress{ } );
    SetOperationStage( DownloadLocationOperationStage::Deleting, "Deleting original downloads..." );
    m_OperationFuture = std::async( std::launch::async, [ this ]( )
    {
        const auto deleteStartedAt = std::chrono::steady_clock::now( );
        LocationOperationProgress progress{ };
        std::vector<DeletePlanEntry> deletePlan;
        ScanDeletePlan( m_OperationPreviousRoot, deletePlan, progress );
        WaitForMinimumOperationStep( deleteStartedAt );
        if( m_CancelLocationOperation.load( ) || progress.m_FailedFiles > 0 )
        {
            SetOperationStage( DownloadLocationOperationStage::DeleteCancelled, "Original deletion stopped." );
            return;
        }

        DeletePlannedOriginals( deletePlan, progress );
        WaitForMinimumOperationStep( deleteStartedAt );
        if( m_CancelLocationOperation.load( ) || progress.m_FailedFiles > 0 )
        {
            SetOperationStage( DownloadLocationOperationStage::DeleteCancelled, "Original deletion stopped." );
            return;
        }

        SetOperationStage( DownloadLocationOperationStage::DeleteComplete, "Original downloads deleted." );
    } );
}

void ImageScraper::SettingsPanel::RequestLocationOperationCancel( )
{
    m_CancelLocationOperation.store( true );
}

void ImageScraper::SettingsPanel::CompleteLocationChangeKeepingOriginals( )
{
    ApplyDownloadRoot( m_OperationNextRoot );
    ResetLocationOperation( );
}

void ImageScraper::SettingsPanel::ResetLocationOperation( )
{
    if( m_OperationFuture.valid( ) )
    {
        m_OperationFuture.wait( );
    }

    m_CopyPlan.clear( );
    m_OperationPreviousRoot.clear( );
    m_OperationNextRoot.clear( );
    m_CancelLocationOperation.store( false );
    UpdateOperationProgress( LocationOperationProgress{ } );
    SetOperationStage( DownloadLocationOperationStage::Idle );
}

void ImageScraper::SettingsPanel::ScanCopyPlan(
    const std::filesystem::path& previousRoot,
    const std::filesystem::path& nextRoot,
    std::vector<CopyPlanEntry>& copyPlan,
    LocationOperationProgress& progress )
{
    std::error_code ec;
    std::filesystem::create_directories( nextRoot, ec );
    if( ec )
    {
        ++progress.m_FailedFiles;
        progress.m_Message = "Failed to create the new download location.";
        UpdateOperationProgress( progress );
        LogError( "[%s] Failed to create new download location %s: %s", __FUNCTION__, nextRoot.string( ).c_str( ), ec.message( ).c_str( ) );
        return;
    }

    for( const std::filesystem::path& providerRoot : GetManagedProviderRoots( previousRoot ) )
    {
        if( m_CancelLocationOperation.load( ) )
        {
            return;
        }

        ec.clear( );
        if( !std::filesystem::exists( providerRoot, ec ) || ec )
        {
            continue;
        }

        if( !std::filesystem::is_directory( providerRoot, ec ) || ec )
        {
            WarningLog( "[%s] Skipped unmanaged provider path because it is not a directory: %s", __FUNCTION__, providerRoot.string( ).c_str( ) );
            continue;
        }

        const std::filesystem::path providerDestinationRoot = nextRoot / providerRoot.lexically_relative( previousRoot );
        std::filesystem::create_directories( providerDestinationRoot, ec );
        if( ec )
        {
            ++progress.m_FailedFiles;
            progress.m_Message = "Failed while preparing destination folders.";
            LogError( "[%s] Failed to create provider destination folder %s: %s", __FUNCTION__, providerDestinationRoot.string( ).c_str( ), ec.message( ).c_str( ) );
            return;
        }

        for( std::filesystem::recursive_directory_iterator it{ providerRoot, std::filesystem::directory_options::skip_permission_denied, ec }, end;
             !ec && it != end && !m_CancelLocationOperation.load( );
             it.increment( ec ) )
        {
            const std::filesystem::path sourcePath = it->path( );
            const std::filesystem::path relativePath = sourcePath.lexically_relative( previousRoot );
            const std::filesystem::path destinationPath = nextRoot / relativePath;
            progress.m_CurrentPath = sourcePath.string( );

            if( it->is_directory( ec ) && !ec )
            {
                std::filesystem::create_directories( destinationPath, ec );
                if( ec )
                {
                    ++progress.m_FailedFiles;
                    progress.m_Message = "Failed while preparing destination folders.";
                    LogError( "[%s] Failed to create destination folder %s: %s", __FUNCTION__, destinationPath.string( ).c_str( ), ec.message( ).c_str( ) );
                    return;
                }
                continue;
            }

            if( !it->is_regular_file( ec ) || ec )
            {
                continue;
            }

            if( std::filesystem::exists( destinationPath, ec ) && !ec )
            {
                ++progress.m_SkippedFiles;
                WarningLog( "[%s] Skipped copying download because destination already exists: %s", __FUNCTION__, destinationPath.string( ).c_str( ) );
                UpdateOperationProgress( progress );
                continue;
            }

            const uintmax_t fileSize = it->file_size( ec );
            if( ec )
            {
                ++progress.m_FailedFiles;
                progress.m_Message = "Failed while reading source file sizes.";
                LogError( "[%s] Failed to read source file size %s: %s", __FUNCTION__, sourcePath.string( ).c_str( ), ec.message( ).c_str( ) );
                return;
            }

            copyPlan.push_back( { sourcePath, destinationPath, fileSize } );
            progress.m_TotalBytes += fileSize;
            ++progress.m_TotalFiles;
            UpdateOperationProgress( progress );
        }

        if( ec )
        {
            ++progress.m_FailedFiles;
            progress.m_Message = "Failed while scanning existing downloads.";
            LogError( "[%s] Failed while scanning existing downloads in %s: %s", __FUNCTION__, providerRoot.string( ).c_str( ), ec.message( ).c_str( ) );
            UpdateOperationProgress( progress );
            return;
        }
    }
}

void ImageScraper::SettingsPanel::CopyPlannedDownloads(
    const std::vector<CopyPlanEntry>& copyPlan,
    LocationOperationProgress& progress )
{
    progress.m_ProcessedBytes = 0;
    progress.m_ProcessedFiles = 0;
    UpdateOperationProgress( progress );

    for( const CopyPlanEntry& entry : copyPlan )
    {
        if( m_CancelLocationOperation.load( ) )
        {
            return;
        }

        if( !CopyFileWithProgress( entry, progress ) )
        {
            ++progress.m_FailedFiles;
            UpdateOperationProgress( progress );
            return;
        }

        ++progress.m_ProcessedFiles;
        UpdateOperationProgress( progress );
    }
}

void ImageScraper::SettingsPanel::ScanDeletePlan(
    const std::filesystem::path& root,
    std::vector<DeletePlanEntry>& deletePlan,
    LocationOperationProgress& progress )
{
    std::error_code ec;
    for( const std::filesystem::path& providerRoot : GetManagedProviderRoots( root ) )
    {
        if( m_CancelLocationOperation.load( ) )
        {
            return;
        }

        ec.clear( );
        if( !std::filesystem::exists( providerRoot, ec ) || ec )
        {
            continue;
        }

        if( !std::filesystem::is_directory( providerRoot, ec ) || ec )
        {
            WarningLog( "[%s] Skipped unmanaged provider path because it is not a directory: %s", __FUNCTION__, providerRoot.string( ).c_str( ) );
            continue;
        }

        std::vector<DeletePlanEntry> providerDeletePlan{ };
        providerDeletePlan.push_back( { providerRoot, 0, true } );
        for( std::filesystem::recursive_directory_iterator it{ providerRoot, std::filesystem::directory_options::skip_permission_denied, ec }, end;
             !ec && it != end && !m_CancelLocationOperation.load( );
             it.increment( ec ) )
        {
            const std::filesystem::path path = it->path( );
            progress.m_CurrentPath = path.string( );

            const bool isDirectory = it->is_directory( ec ) && !ec;
            uintmax_t sizeBytes = 0;
            if( !isDirectory && it->is_regular_file( ec ) && !ec )
            {
                sizeBytes = it->file_size( ec );
                if( ec )
                {
                    sizeBytes = 0;
                }
                progress.m_TotalBytes += sizeBytes;
                ++progress.m_TotalFiles;
            }

            providerDeletePlan.push_back( { path, sizeBytes, isDirectory } );
            UpdateOperationProgress( progress );
        }

        if( ec )
        {
            ++progress.m_FailedFiles;
            progress.m_Message = "Failed while scanning originals for deletion.";
            LogError( "[%s] Failed while scanning originals in %s: %s", __FUNCTION__, providerRoot.string( ).c_str( ), ec.message( ).c_str( ) );
            UpdateOperationProgress( progress );
            return;
        }

        std::reverse( providerDeletePlan.begin( ), providerDeletePlan.end( ) );
        deletePlan.insert( deletePlan.end( ), providerDeletePlan.begin( ), providerDeletePlan.end( ) );
    }
}

void ImageScraper::SettingsPanel::DeletePlannedOriginals(
    const std::vector<DeletePlanEntry>& deletePlan,
    LocationOperationProgress& progress )
{
    for( const DeletePlanEntry& entry : deletePlan )
    {
        if( m_CancelLocationOperation.load( ) )
        {
            return;
        }

        if( !DeleteFileWithProgress( entry, progress ) )
        {
            ++progress.m_FailedFiles;
            UpdateOperationProgress( progress );
            return;
        }
    }
}

bool ImageScraper::SettingsPanel::CheckFreeSpaceForCopy( const std::filesystem::path& nextRoot, LocationOperationProgress& progress )
{
    const uintmax_t bufferBytes = (std::max<uintmax_t>)( progress.m_TotalBytes / 10, k_MinFreeSpaceBufferBytes );
    const uintmax_t maxBytes = (std::numeric_limits<uintmax_t>::max)( );
    progress.m_RequiredBytes = progress.m_TotalBytes > maxBytes - bufferBytes ? maxBytes : progress.m_TotalBytes + bufferBytes;

    std::error_code ec;
    const std::filesystem::space_info spaceInfo = std::filesystem::space( nextRoot, ec );
    if( ec || spaceInfo.available == static_cast<uintmax_t>( -1 ) )
    {
        progress.m_Message = "Unknown free space";
        UpdateOperationProgress( progress );
        return true;
    }

    progress.m_AvailableBytes = spaceInfo.available;
    UpdateOperationProgress( progress );
    if( spaceInfo.available < progress.m_RequiredBytes )
    {
        std::ostringstream stream;
        stream << "Insufficient free space. Required " << FormatBytes( progress.m_RequiredBytes )
               << ", available " << FormatBytes( spaceInfo.available ) << ".";
        progress.m_Message = stream.str( );
        UpdateOperationProgress( progress );
        return false;
    }

    return true;
}

bool ImageScraper::SettingsPanel::CopyFileWithProgress( const CopyPlanEntry& entry, LocationOperationProgress& progress )
{
    std::error_code ec;
    std::filesystem::create_directories( entry.m_DestinationPath.parent_path( ), ec );
    if( ec )
    {
        progress.m_Message = "Failed to create destination folders.";
        LogError( "[%s] Failed to create destination folder %s: %s", __FUNCTION__, entry.m_DestinationPath.parent_path( ).string( ).c_str( ), ec.message( ).c_str( ) );
        return false;
    }

    std::ifstream input{ entry.m_SourcePath, std::ios::binary };
    if( !input.is_open( ) )
    {
        progress.m_Message = "Failed to open source file.";
        LogError( "[%s] Failed to open source file: %s", __FUNCTION__, entry.m_SourcePath.string( ).c_str( ) );
        return false;
    }

    std::ofstream output{ entry.m_DestinationPath, std::ios::binary | std::ios::trunc };
    if( !output.is_open( ) )
    {
        progress.m_Message = "Failed to open destination file.";
        LogError( "[%s] Failed to open destination file: %s", __FUNCTION__, entry.m_DestinationPath.string( ).c_str( ) );
        return false;
    }

    std::vector<char> buffer( static_cast<size_t>( k_CopyBufferBytes ) );
    uintmax_t copiedForFile = 0;
    progress.m_CurrentPath = entry.m_SourcePath.string( );
    UpdateOperationProgress( progress );

    while( !m_CancelLocationOperation.load( ) && input.good( ) )
    {
        input.read( buffer.data( ), static_cast<std::streamsize>( buffer.size( ) ) );
        const std::streamsize bytesRead = input.gcount( );
        if( bytesRead <= 0 )
        {
            break;
        }

        output.write( buffer.data( ), bytesRead );
        if( !output.good( ) )
        {
            progress.m_Message = "Failed while writing destination file.";
            LogError( "[%s] Failed while writing destination file: %s", __FUNCTION__, entry.m_DestinationPath.string( ).c_str( ) );
            return false;
        }

        copiedForFile += static_cast<uintmax_t>( bytesRead );
        progress.m_ProcessedBytes += static_cast<uintmax_t>( bytesRead );
        UpdateOperationProgress( progress );
    }

    if( m_CancelLocationOperation.load( ) )
    {
        return true;
    }

    if( copiedForFile != entry.m_SizeBytes )
    {
        progress.m_Message = "Failed while reading source file.";
        LogError( "[%s] Failed while reading source file: %s", __FUNCTION__, entry.m_SourcePath.string( ).c_str( ) );
        return false;
    }

    return true;
}

bool ImageScraper::SettingsPanel::DeleteFileWithProgress( const DeletePlanEntry& entry, LocationOperationProgress& progress )
{
    progress.m_CurrentPath = entry.m_Path.string( );
    UpdateOperationProgress( progress );

    std::error_code ec;
    std::filesystem::remove( entry.m_Path, ec );
    if( ec )
    {
        progress.m_Message = "Failed while deleting originals.";
        LogError( "[%s] Failed to delete original path %s: %s", __FUNCTION__, entry.m_Path.string( ).c_str( ), ec.message( ).c_str( ) );
        return false;
    }

    if( !entry.m_IsDirectory )
    {
        progress.m_ProcessedBytes += entry.m_SizeBytes;
        ++progress.m_ProcessedFiles;
        UpdateOperationProgress( progress );
    }

    return true;
}

void ImageScraper::SettingsPanel::SetOperationStage( DownloadLocationOperationStage stage, const std::string& message )
{
    std::lock_guard<std::mutex> lock( m_OperationMutex );
    m_LocationOperationStage = stage;
    if( !message.empty( ) )
    {
        m_LocationOperationProgress.m_Message = message;
    }
}

void ImageScraper::SettingsPanel::WaitForMinimumOperationStep( std::chrono::steady_clock::time_point startedAt )
{
    const auto visibleUntil = startedAt + std::chrono::milliseconds( k_MinOperationStageMs );
    while( !m_CancelLocationOperation.load( ) && std::chrono::steady_clock::now( ) < visibleUntil )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 25 ) );
    }
}

ImageScraper::SettingsPanel::DownloadLocationOperationStage ImageScraper::SettingsPanel::GetOperationStage( ) const
{
    std::lock_guard<std::mutex> lock( m_OperationMutex );
    return m_LocationOperationStage;
}

ImageScraper::SettingsPanel::LocationOperationProgress ImageScraper::SettingsPanel::GetOperationProgressSnapshot( ) const
{
    std::lock_guard<std::mutex> lock( m_OperationMutex );
    return m_LocationOperationProgress;
}

void ImageScraper::SettingsPanel::UpdateOperationProgress( const LocationOperationProgress& progress )
{
    std::lock_guard<std::mutex> lock( m_OperationMutex );
    m_LocationOperationProgress = progress;
}

std::string ImageScraper::SettingsPanel::FormatBytes( uintmax_t bytes ) const
{
    std::ostringstream stream;
    if( bytes < 1024 )
    {
        stream << bytes << " B";
    }
    else if( bytes < 1024ull * 1024ull )
    {
        stream << ( bytes / 1024ull ) << " KB";
    }
    else if( bytes < 1024ull * 1024ull * 1024ull )
    {
        stream << ( bytes / ( 1024ull * 1024ull ) ) << " MB";
    }
    else
    {
        stream << ( bytes / ( 1024ull * 1024ull * 1024ull ) ) << " GB";
    }

    return stream.str( );
}

float ImageScraper::SettingsPanel::CalculateProgressFraction( const LocationOperationProgress& progress ) const
{
    if( progress.m_TotalBytes == 0 )
    {
        return progress.m_TotalFiles == 0 ? 0.0f : static_cast<float>( progress.m_ProcessedFiles ) / static_cast<float>( progress.m_TotalFiles );
    }

    return (std::min)( 1.0f, static_cast<float>( static_cast<double>( progress.m_ProcessedBytes ) / static_cast<double>( progress.m_TotalBytes ) ) );
}

bool ImageScraper::SettingsPanel::IsDownloadRootAvailableOrCreatable( const std::filesystem::path& downloadRoot ) const
{
    return DownloadLocationConfig::IsRootDriveAvailable( downloadRoot );
}

std::string ImageScraper::SettingsPanel::FormatPathForDisplay( const std::filesystem::path& path ) const
{
    return path.empty( ) ? std::string{ "(not set)" } : path.string( );
}

std::optional<std::filesystem::path> ImageScraper::SettingsPanel::PickFolder( const std::filesystem::path& initialPath )
{
#ifdef _WIN32
    HRESULT initResult = ::CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
    const bool shouldUninitialize = SUCCEEDED( initResult );
    if( FAILED( initResult ) && initResult != RPC_E_CHANGED_MODE )
    {
        LogError( "[%s] Failed to initialise COM for folder picker.", __FUNCTION__ );
        return std::nullopt;
    }

    IFileOpenDialog* dialog = nullptr;
    HRESULT result = ::CoCreateInstance( CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &dialog ) );
    if( FAILED( result ) || dialog == nullptr )
    {
        if( shouldUninitialize )
        {
            ::CoUninitialize( );
        }
        LogError( "[%s] Failed to create folder picker.", __FUNCTION__ );
        return std::nullopt;
    }

    DWORD options = 0;
    if( SUCCEEDED( dialog->GetOptions( &options ) ) )
    {
        dialog->SetOptions( options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST );
    }
    dialog->SetTitle( L"Choose download location" );

    if( !initialPath.empty( ) && std::filesystem::exists( initialPath ) )
    {
        IShellItem* initialFolder = nullptr;
        result = ::SHCreateItemFromParsingName( initialPath.wstring( ).c_str( ), nullptr, IID_PPV_ARGS( &initialFolder ) );
        if( SUCCEEDED( result ) && initialFolder != nullptr )
        {
            dialog->SetFolder( initialFolder );
            initialFolder->Release( );
        }
    }

    std::optional<std::filesystem::path> selectedPath;
    result = dialog->Show( nullptr );
    if( SUCCEEDED( result ) )
    {
        IShellItem* item = nullptr;
        result = dialog->GetResult( &item );
        if( SUCCEEDED( result ) && item != nullptr )
        {
            PWSTR path = nullptr;
            result = item->GetDisplayName( SIGDN_FILESYSPATH, &path );
            if( SUCCEEDED( result ) && path != nullptr )
            {
                selectedPath = std::filesystem::path{ path };
                ::CoTaskMemFree( path );
            }
            item->Release( );
        }
    }

    dialog->Release( );
    if( shouldUninitialize )
    {
        ::CoUninitialize( );
    }

    return selectedPath;
#else
    ( void )initialPath;
    return std::nullopt;
#endif
}
