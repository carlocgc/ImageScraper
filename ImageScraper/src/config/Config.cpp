#include "config/Config.h"
#include "log/Logger.h"
#include "utils/FilesystemUtils.h"
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
    const std::filesystem::path bundlePath = root / FilesystemUtils::PathFromUtf8( bundleName );
    return FilesystemUtils::PathToUtf8Generic( bundlePath );
}

bool ImageScraper::Config::ReadFromFile( const std::string& filename )
{
    std::filesystem::path configPath = std::filesystem::current_path( ) / FilesystemUtils::PathFromUtf8( filename );
    const std::string filepath = FilesystemUtils::PathToUtf8Generic( configPath );
    if( !std::filesystem::exists( configPath ) )
    {
        LogError( "[%s] Read failed, file not found: %s", __FUNCTION__, filepath.c_str( ) );
        return false;
    };

    std::ifstream file;
    file.open( configPath );
    if( !file.is_open( ) )
    {
        LogError( "[%s] Read failed, Could not open file: %s", __FUNCTION__, filepath.c_str( ) );
        return false;
    }

    m_Json = json::parse( file );

    SuccessLog( "[%s] Config Loaded successfully.", __FUNCTION__ );
    return true;
}
