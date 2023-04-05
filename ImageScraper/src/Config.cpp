#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>

Config::Config( )
{
    const std::filesystem::path confPath = std::filesystem::current_path( ) / "config.json";
    const std::string pathStr = confPath.generic_string( );
    ReadFromFile( pathStr );
}

const std::string Config::UserAgent( ) const
{
    return GetValue<std::string>( "UserAgent" );
}

const std::string Config::CaBundle( ) const
{
    const std::string bundleName = GetValue<std::string>( "CaBundle" );
    const std::filesystem::path root = std::filesystem::current_path( );
    const std::filesystem::path bundlePath = root / bundleName.c_str( );
    return bundlePath.generic_string( );
}

bool Config::ReadFromFile( const std::string& filepath )
{
    std::filesystem::path configPath = filepath;
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

    m_ConfigData = nlohmann::json::parse( file );

    InfoLog( "[%s] Config Loaded successfully.", __FUNCTION__ );
    return false;
}
