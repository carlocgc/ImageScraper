#pragma once

#include "io\JsonFile.h"
#include "ui\IUiPanel.h"
#include "updater\UpdateChecker.h"

#include <memory>
#include <string>

namespace ImageScraper
{
    enum class SettingsUpdateStatus
    {
        NotChecked,
        Checking,
        UpToDate,
        UpdateAvailable,
        Skipped,
        Failed,
    };

    class SettingsPanel : public IUiPanel
    {
    public:
        static constexpr const char* s_CheckUpdatesOnStartupConfigKey = "check_updates_on_startup";
        static constexpr const char* s_LastUpdatePromptedVersionConfigKey = "last_update_prompted_version";
        static constexpr const char* s_SkippedUpdateVersionConfigKey = "skipped_update_version";
#ifdef _DEBUG
        static constexpr const char* s_CheckDevelopmentVersionConfigKey = "check_development_version_updates";
#endif

        SettingsPanel( std::shared_ptr<JsonFile> appConfig, std::string caBundlePath );

        void Update( ) override;

        bool IsCheckUpdatesOnStartupEnabled( ) const { return m_CheckUpdatesOnStartup; }
        SettingsUpdateStatus GetUpdateStatus( ) const { return m_UpdateStatus; }

    private:
        void LoadState( );
        void PersistCheckUpdatesOnStartup( );
#ifdef _DEBUG
        void PersistCheckDevelopmentVersion( );
#endif
        void PersistStringValue( const std::string& key, const std::string& value );

        void StartUpdateCheck( bool manual );
        void OnUpdateCheckComplete( const UpdateCheckResult& result, bool manual );
        bool ShouldShowStartupPromptFor( const GitHubReleaseInfo& release ) const;

        void DrawUpdateStatus( ) const;
        void DrawUpdatePopup( );
        bool OpenReleasePage( const std::string& url ) const;

        std::shared_ptr<JsonFile> m_AppConfig{ };
        std::string m_CaBundlePath{ };
        bool m_CheckUpdatesOnStartup{ false };
        bool m_StartupCheckHandled{ false };
        bool m_CheckRunning{ false };
        bool m_ShowUpdatePopup{ false };
        SettingsUpdateStatus m_UpdateStatus{ SettingsUpdateStatus::NotChecked };
        GitHubReleaseInfo m_LatestRelease{ };
        std::string m_LastError{ };
        std::string m_LastUpdatePromptedVersion{ };
        std::string m_SkippedUpdateVersion{ };

#ifdef _DEBUG
        bool m_CheckDevelopmentVersion{ false };
#endif
    };
}
