#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>

bool Config::ReadFromFile( const std::string& filepath )
{
    std::filesystem::path configPath = filepath;
    if( !std::filesystem::exists( configPath ) )
    {
        ErrorLog( "[%s] Read failed, file not found: %s", __FUNCTION__, filepath.c_str( ) );
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
