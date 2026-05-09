#include "CppUnitTest.h"
#include "ui/LogPanel.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    namespace
    {
        struct TempFile
        {
            std::string path;

            explicit TempFile( const std::string& suffix )
            {
                path = ( std::filesystem::temp_directory_path( ) / suffix ).string( );
            }

            ~TempFile( )
            {
                std::error_code ec;
                std::filesystem::remove( path, ec );
            }
        };
    }


    TEST_CLASS(LogPanelTests)
    {
    public:
    TEST_METHOD(LoadWordWrapEnabledFromConfig_Defaults_To_True_And_Persists_The_Missing_Key)
    {
        TempFile tmp( "imagescraper_log_panel_default.json" );
        std::shared_ptr<JsonFile> appConfig = std::make_shared<JsonFile>( tmp.path );
    
        Assert::IsTrue(  appConfig->Deserialise( ) == true );
        Assert::IsTrue(  LogPanel::LoadWordWrapEnabledFromConfig( appConfig ) == true );
    
        bool storedWordWrap = false;
        Assert::IsTrue(  appConfig->GetValue<bool>( LogPanel::s_WordWrapConfigKey, storedWordWrap ) == true );
        Assert::IsTrue(  storedWordWrap == true );
    
        JsonFile reloaded( tmp.path );
        Assert::IsTrue(  reloaded.Deserialise( ) == true );
        storedWordWrap = false;
        Assert::IsTrue(  reloaded.GetValue<bool>( LogPanel::s_WordWrapConfigKey, storedWordWrap ) == true );
        Assert::IsTrue(  storedWordWrap == true );
    }
    
    TEST_METHOD(LoadWordWrapEnabledFromConfig_Respects_A_Saved_Disabled_Value)
    {
        TempFile tmp( "imagescraper_log_panel_disabled.json" );
        std::shared_ptr<JsonFile> appConfig = std::make_shared<JsonFile>( tmp.path );
    
        Assert::IsTrue(  appConfig->Deserialise( ) == true );
        Assert::IsTrue(  LogPanel::SaveWordWrapEnabledToConfig( appConfig, false ) == true );
        Assert::IsTrue(  LogPanel::LoadWordWrapEnabledFromConfig( appConfig ) == false );
    
        JsonFile reloaded( tmp.path );
        Assert::IsTrue(  reloaded.Deserialise( ) == true );
    
        bool storedWordWrap = true;
        Assert::IsTrue(  reloaded.GetValue<bool>( LogPanel::s_WordWrapConfigKey, storedWordWrap ) == true );
        Assert::IsTrue(  storedWordWrap == false );
    }
    
    };
}
