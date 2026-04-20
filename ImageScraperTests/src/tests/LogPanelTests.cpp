#include "catch2/catch_amalgamated.hpp"
#include "ui/LogPanel.h"

#include <filesystem>
#include <memory>
#include <string>

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

TEST_CASE( "LoadWordWrapEnabledFromConfig defaults to true and persists the missing key", "[LogPanel]" )
{
    TempFile tmp( "imagescraper_log_panel_default.json" );
    std::shared_ptr<JsonFile> appConfig = std::make_shared<JsonFile>( tmp.path );

    REQUIRE( appConfig->Deserialise( ) == true );
    REQUIRE( LogPanel::LoadWordWrapEnabledFromConfig( appConfig ) == true );

    bool storedWordWrap = false;
    REQUIRE( appConfig->GetValue<bool>( LogPanel::s_WordWrapConfigKey, storedWordWrap ) == true );
    REQUIRE( storedWordWrap == true );

    JsonFile reloaded( tmp.path );
    REQUIRE( reloaded.Deserialise( ) == true );
    storedWordWrap = false;
    REQUIRE( reloaded.GetValue<bool>( LogPanel::s_WordWrapConfigKey, storedWordWrap ) == true );
    REQUIRE( storedWordWrap == true );
}

TEST_CASE( "LoadWordWrapEnabledFromConfig respects a saved disabled value", "[LogPanel]" )
{
    TempFile tmp( "imagescraper_log_panel_disabled.json" );
    std::shared_ptr<JsonFile> appConfig = std::make_shared<JsonFile>( tmp.path );

    REQUIRE( appConfig->Deserialise( ) == true );
    REQUIRE( LogPanel::SaveWordWrapEnabledToConfig( appConfig, false ) == true );
    REQUIRE( LogPanel::LoadWordWrapEnabledFromConfig( appConfig ) == false );

    JsonFile reloaded( tmp.path );
    REQUIRE( reloaded.Deserialise( ) == true );

    bool storedWordWrap = true;
    REQUIRE( reloaded.GetValue<bool>( LogPanel::s_WordWrapConfigKey, storedWordWrap ) == true );
    REQUIRE( storedWordWrap == false );
}
