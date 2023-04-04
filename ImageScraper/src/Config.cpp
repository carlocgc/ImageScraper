#include "Config.h"
#include "json/json.h"
#include <fstream>
#include <filesystem>

bool Config::ReadFromFile( const std::string& filepath )
{
    std::filesystem::path configPath = filepath;
    if( !std::filesystem::exists( configPath ) )
    {
        return false;
    };

    std::ofstream file;
    file.open( filepath );
    if( !file.is_open( ) )
    {
        return false;
    }



    return false;
}
