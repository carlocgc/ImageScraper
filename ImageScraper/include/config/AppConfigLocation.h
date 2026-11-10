#pragma once

#include <filesystem>
#include <system_error>

namespace ImageScraper::AppConfigLocation
{
    // The app config (download location, OAuth refresh tokens, UI state) used to
    // live in %TEMP%, which Windows Storage Sense and Disk Cleanup empty, so the
    // settings were silently lost. It now lives in %LOCALAPPDATA%\ImageScraper.
    inline constexpr const wchar_t* s_AppFolderName = L"ImageScraper";
    inline constexpr const wchar_t* s_AppConfigFileName = L"config.json";

    // Returns <baseDir>/ImageScraper/config.json.
    inline std::filesystem::path GetAppConfigPath( const std::filesystem::path& baseDir )
    {
        return baseDir / s_AppFolderName / s_AppConfigFileName;
    }

    enum class MigrationResult
    {
        NotNeeded,  // Target already exists, or there is no legacy file to move.
        Migrated,   // Legacy file was copied to the target.
        Failed      // Legacy file exists but could not be copied.
    };

    // Copies the legacy config to the new location if the new one does not exist
    // yet. The legacy file is left in place; Windows clears %TEMP% on its own.
    // Never throws.
    inline MigrationResult MigrateLegacyAppConfig(
        const std::filesystem::path& legacyPath,
        const std::filesystem::path& targetPath )
    {
        std::error_code ec;
        if( std::filesystem::exists( targetPath, ec ) || ec )
        {
            return MigrationResult::NotNeeded;
        }

        if( !std::filesystem::is_regular_file( legacyPath, ec ) || ec )
        {
            return MigrationResult::NotNeeded;
        }

        std::filesystem::create_directories( targetPath.parent_path( ), ec );
        if( ec )
        {
            return MigrationResult::Failed;
        }

        std::filesystem::copy_file( legacyPath, targetPath, ec );
        return ec ? MigrationResult::Failed : MigrationResult::Migrated;
    }
}
