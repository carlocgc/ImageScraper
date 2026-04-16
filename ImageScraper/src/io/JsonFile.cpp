#include "io/JsonFile.h"
#include "log/Logger.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iomanip>

bool ImageScraper::JsonFile::Deserialise( )
{
    LogDebug( "[%s] Start, m_FilePath: ", __FUNCTION__, m_FilePath.c_str( ) );

    if( !std::filesystem::exists( m_FilePath ) )
    {        
        CreateFile( );
    }

    std::ifstream file( m_FilePath );
    if( !file.is_open( ) )
    {
        LogError( "[%s] Deserialisation failed, Could not open file: %s", __FUNCTION__, m_FilePath.c_str( ) );
        return false;
    }

    m_Json = Json::parse( file );
    file.close( );

    LogDebug( "[%s] Success, file path: %s", __FUNCTION__, m_FilePath.c_str( ) );

    return true;
}

bool ImageScraper::JsonFile::Serialise( )
{
    LogDebug( "[%s] Start, file path: %s", __FUNCTION__, m_FilePath.c_str( ) );

    if( !std::filesystem::exists( m_FilePath ) )
    {
        CreateFile( );
    }

    std::ofstream file( m_FilePath, std::ios::trunc );
    if( !file.is_open( ) )
    {
        LogError( "[%s] Serialisation failed, Could not open file: %s", __FUNCTION__, m_FilePath.c_str( ) );
        return false;
    }

    file << m_Json.dump( 4, ' ' ) << std::endl;
    file.close( );

    LogDebug( "[%s] Success, file path: %s", __FUNCTION__, m_FilePath.c_str( ) );

    return true;
}

void ImageScraper::JsonFile::CreateFile( )
{
    if( std::filesystem::exists( m_FilePath ) )
    {
        return;
    }

    std::filesystem::path path( m_FilePath );
    std::filesystem::path parentPath = path.parent_path( );
    if( !std::filesystem::exists( parentPath ) )
    {        
        std::filesystem::create_directories( parentPath );
        LogDebug( "[%s] Directory created: %s", __FUNCTION__, parentPath.c_str( ) );
    }

    std::ofstream outFile{ m_FilePath, std::ios::app };
    Json data{ };
    outFile << data.dump( 4, ' ' ) << std::endl;
    outFile.close( );
    LogDebug( "[%s] File created: %s", __FUNCTION__, m_FilePath.c_str( ) );
}
