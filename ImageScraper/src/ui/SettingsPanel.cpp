#include "ui\SettingsPanel.h"

#include "async\TaskManager.h"
#include "log\Logger.h"
#include "network\CurlHttpClient.h"
#include "version\Version.h"

#include "imgui\imgui.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <shellapi.h>
#endif

#include <cstdint>
#include <utility>

ImageScraper::SettingsPanel::SettingsPanel( std::shared_ptr<JsonFile> appConfig, std::string caBundlePath )
    : m_AppConfig{ std::move( appConfig ) },
      m_CaBundlePath{ std::move( caBundlePath ) }
{
    LoadState( );
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
