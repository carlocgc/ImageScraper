#include "io/JsonFile.h"
#include "log/Logger.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iomanip>

bool ImageScraper::JsonFile::Deserialise( )
{
    DebugLog( "[%s] Start, m_FilePath: ", __FUNCTION__, m_FilePath.c_str( ) );

    if( !std::filesystem::exists( m_FilePath ) )
    {
        std::ofstream outFile{ m_FilePath, std::ios::app };
        Json data{ };
        outFile << data.dump( 4, ' ' ) << std::endl;
        outFile.close( );
    }

    std::ifstream file( m_FilePath );
    if( !file.is_open( ) )
    {
        ErrorLog( "[%s] Deserialisation failed, Could not open file: %s", __FUNCTION__, m_FilePath.c_str( ) );
        return false;
    }

    m_Json = Json::parse( file );
    file.close( );

    DebugLog( "[%s] Success, file path: %s", __FUNCTION__, m_FilePath.c_str( ) );

    return true;
}

bool ImageScraper::JsonFile::Serialise( )
{
    DebugLog( "[%s] Start, file path: %s", __FUNCTION__, m_FilePath.c_str( ) );

    std::ofstream file( m_FilePath, std::ios::trunc );
    if( !file.is_open( ) )
    {
        ErrorLog( "[%s] Serialisation failed, Could not open file: %s", __FUNCTION__, m_FilePath.c_str( ) );
        return false;
    }

    file << m_Json.dump( 4, ' ' ) << std::endl;
    file.close( );

    DebugLog( "[%s] Success, file path: ", __FUNCTION__, m_FilePath.c_str( ) );

    return true;
}
