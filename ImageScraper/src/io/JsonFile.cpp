#include "io/JsonFile.h"
#include "log/Logger.h"
#include "nlohmann/json.hpp"
#include "utils/FilesystemUtils.h"
#include <filesystem>
#include <fstream>
#include <iomanip>

// m_FilePath is UTF-8. Every filesystem call must go through PathFromUtf8,
// because the narrow std::string overloads decode with the ANSI code page and
// would target a different (mangled) path for any non-ANSI character.

bool ImageScraper::JsonFile::Deserialise( )
{
    LogDebug( "[%s] Start, file path: %s", __FUNCTION__, m_FilePath.c_str( ) );

    const std::filesystem::path path = FilesystemUtils::PathFromUtf8( m_FilePath );
    if( !std::filesystem::exists( path ) )
    {
        CreateFile( );
    }

    std::ifstream file( path );
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

    const std::filesystem::path path = FilesystemUtils::PathFromUtf8( m_FilePath );
    if( !std::filesystem::exists( path ) )
    {
        CreateFile( );
    }

    std::ofstream file( path, std::ios::trunc );
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
    const std::filesystem::path path = FilesystemUtils::PathFromUtf8( m_FilePath );
    if( std::filesystem::exists( path ) )
    {
        return;
    }

    const std::filesystem::path parentPath = path.parent_path( );
    if( !parentPath.empty( ) && !std::filesystem::exists( parentPath ) )
    {
        std::filesystem::create_directories( parentPath );
        LogDebug( "[%s] Directory created: %s", __FUNCTION__, FilesystemUtils::PathToUtf8( parentPath ).c_str( ) );
    }

    std::ofstream outFile{ path, std::ios::app };
    Json data{ };
    outFile << data.dump( 4, ' ' ) << std::endl;
    outFile.close( );
    LogDebug( "[%s] File created: %s", __FUNCTION__, m_FilePath.c_str( ) );
}
