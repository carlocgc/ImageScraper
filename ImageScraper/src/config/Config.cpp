#include "config/Config.h"
#include "log/Logger.h"
#include <fstream>
#include <filesystem>

const std::string ImageScraper::Config::UserAgent( ) const
{
    return GetValue<std::string>( "UserAgent" );
}

const std::string ImageScraper::Config::CaBundle( ) const
{
    const std::string bundleName = GetValue<std::string>( "CaBundle" );
    const std::filesystem::path root = std::filesystem::current_path( );
    const std::filesystem::path bundlePath = root / bundleName.c_str( );
    return bundlePath.generic_string( );
}

bool ImageScraper::Config::ReadFromFile( const std::string& filename )
{
    std::filesystem::path configPath = std::filesystem::current_path( ) / filename;
    const std::string filepath = configPath.generic_string( );
    if( !std::filesystem::exists( configPath ) )
    {
        ErrorAssert( "[%s] Read failed, file not found: %s", __FUNCTION__, filepath.c_str( ) );
        return false;
    };

    std::ifstream file;
    file.open( filepath );
    if( !file.is_open( ) )
    {
        ErrorLog( "[%s] Read failed, Could not open file: %s", __FUNCTION__, filepath.c_str( ) );
        return false;
    }

    m_Json = json::parse( file );

    InfoLog( "[%s] Config Loaded successfully.", __FUNCTION__ );
    return false;
}
