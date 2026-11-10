#include "CppUnitTest.h"
#include "config/AppConfigLocation.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{
    using namespace ImageScraper;

    namespace
    {
        // Creates a scratch directory with a legacy and a new base folder, removed on destruction.
        struct AppConfigSandbox
        {
            std::filesystem::path root;
            std::filesystem::path legacyPath;
            std::filesystem::path targetPath;

            AppConfigSandbox( )
            {
                root = std::filesystem::temp_directory_path( ) / "AppConfigLocationTest";
                std::error_code ec;
                std::filesystem::remove_all( root, ec );
                legacyPath = AppConfigLocation::GetAppConfigPath( root / "Temp" );
                targetPath = AppConfigLocation::GetAppConfigPath( root / "LocalAppData" );
            }

            ~AppConfigSandbox( )
            {
                std::error_code ec;
                std::filesystem::remove_all( root, ec );
            }

            static void WriteFile( const std::filesystem::path& path, const std::string& contents )
            {
                std::filesystem::create_directories( path.parent_path( ) );
                std::ofstream file{ path, std::ios::trunc };
                file << contents;
            }

            static std::string ReadFile( const std::filesystem::path& path )
            {
                std::ifstream file{ path };
                return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
            }
        };
    }

    TEST_CLASS(AppConfigLocationTests)
    {
    public:
        TEST_METHOD(GetAppConfigPath_Appends_App_Folder_And_File_Name)
        {
            const std::filesystem::path result = AppConfigLocation::GetAppConfigPath( L"C:\\Base" );
            Assert::AreEqual( std::wstring{ L"C:\\Base\\ImageScraper\\config.json" }, result.wstring( ) );
        }

        TEST_METHOD(Migrate_Copies_Legacy_Config_When_Target_Missing)
        {
            AppConfigSandbox sandbox;
            AppConfigSandbox::WriteFile( sandbox.legacyPath, R"({"download_location":"D:/Pics"})" );

            const auto result = AppConfigLocation::MigrateLegacyAppConfig( sandbox.legacyPath, sandbox.targetPath );

            Assert::IsTrue( result == AppConfigLocation::MigrationResult::Migrated );
            Assert::AreEqual( std::string{ R"({"download_location":"D:/Pics"})" }, AppConfigSandbox::ReadFile( sandbox.targetPath ) );
            Assert::IsTrue( std::filesystem::exists( sandbox.legacyPath ), L"Legacy file should be left in place" );
        }

        TEST_METHOD(Migrate_Does_Not_Overwrite_Existing_Target)
        {
            AppConfigSandbox sandbox;
            AppConfigSandbox::WriteFile( sandbox.legacyPath, R"({"download_location":"old"})" );
            AppConfigSandbox::WriteFile( sandbox.targetPath, R"({"download_location":"new"})" );

            const auto result = AppConfigLocation::MigrateLegacyAppConfig( sandbox.legacyPath, sandbox.targetPath );

            Assert::IsTrue( result == AppConfigLocation::MigrationResult::NotNeeded );
            Assert::AreEqual( std::string{ R"({"download_location":"new"})" }, AppConfigSandbox::ReadFile( sandbox.targetPath ) );
        }

        TEST_METHOD(Migrate_Does_Nothing_When_Legacy_Config_Missing)
        {
            AppConfigSandbox sandbox;

            const auto result = AppConfigLocation::MigrateLegacyAppConfig( sandbox.legacyPath, sandbox.targetPath );

            Assert::IsTrue( result == AppConfigLocation::MigrationResult::NotNeeded );
            Assert::IsFalse( std::filesystem::exists( sandbox.targetPath ) );
        }

        TEST_METHOD(Migrate_Handles_Non_Ansi_Paths)
        {
            AppConfigSandbox sandbox;
            const std::filesystem::path legacy = AppConfigLocation::GetAppConfigPath( sandbox.root / L"\u4E34\u65F6" );
            const std::filesystem::path target = AppConfigLocation::GetAppConfigPath( sandbox.root / L"\u0414\u0430\u043D\u043D\u044B\u0435" );
            AppConfigSandbox::WriteFile( legacy, "{}" );

            const auto result = AppConfigLocation::MigrateLegacyAppConfig( legacy, target );

            Assert::IsTrue( result == AppConfigLocation::MigrationResult::Migrated );
            Assert::IsTrue( std::filesystem::exists( target ) );
        }
    };
}
