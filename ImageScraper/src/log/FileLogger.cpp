#include "log/FileLogger.h"
#include "log/Logger.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <system_error>
#include <vector>

namespace
{
    constexpr const char* k_FilePrefix = "ImageScraper_";
    constexpr const char* k_FileExtension = ".log";
}

ImageScraper::FileLogger::FileLogger( const std::filesystem::path& logsDir, int maxRetainedFiles )
{
    std::error_code ec;
    std::filesystem::create_directories( logsDir, ec );

    m_FilePath = logsDir / MakeSessionFileName( );

    // Binary mode prevents CRLF translation. Lines already terminate with '\n'.
    m_Stream.open( m_FilePath, std::ios::out | std::ios::binary | std::ios::app );

    if( maxRetainedFiles > 0 )
    {
        PruneOldSessions( logsDir, maxRetainedFiles );
    }
}

ImageScraper::FileLogger::~FileLogger( )
{
    std::scoped_lock lock{ m_Mutex };
    if( m_Stream.is_open( ) )
    {
        m_Stream.flush( );
        m_Stream.close( );
    }
}

bool ImageScraper::FileLogger::IsOpen( ) const
{
    return m_Stream.is_open( );
}

void ImageScraper::FileLogger::Log( const LogLine& line )
{
    std::scoped_lock lock{ m_Mutex };
    if( !m_Stream.is_open( ) )
    {
        return;
    }

    m_Stream.write( line.m_String.data( ), static_cast<std::streamsize>( line.m_String.size( ) ) );
    m_Stream.flush( );
}

std::string ImageScraper::FileLogger::MakeSessionFileName( )
{
    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now( );
    const std::time_t timeT = std::chrono::system_clock::to_time_t( now );
    std::tm local{ };
    localtime_s( &local, &timeT );

    char buf[ 64 ];
    std::snprintf( buf, sizeof( buf ),
                   "%s%04d%02d%02d_%02d%02d%02d%s",
                   k_FilePrefix,
                   local.tm_year + 1900,
                   local.tm_mon + 1,
                   local.tm_mday,
                   local.tm_hour,
                   local.tm_min,
                   local.tm_sec,
                   k_FileExtension );
    return std::string{ buf };
}

void ImageScraper::FileLogger::PruneOldSessions( const std::filesystem::path& logsDir, int maxRetainedFiles )
{
    std::error_code ec;
    if( !std::filesystem::is_directory( logsDir, ec ) )
    {
        return;
    }

    std::vector<std::filesystem::path> sessionFiles;
    for( const auto& entry : std::filesystem::directory_iterator( logsDir, ec ) )
    {
        if( ec ) break;
        if( !entry.is_regular_file( ec ) ) continue;
        const std::string name = entry.path( ).filename( ).string( );
        if( name.rfind( k_FilePrefix, 0 ) != 0 ) continue;
        if( entry.path( ).extension( ).string( ) != k_FileExtension ) continue;
        sessionFiles.push_back( entry.path( ) );
    }

    if( static_cast<int>( sessionFiles.size( ) ) <= maxRetainedFiles )
    {
        return;
    }

    // Filenames embed a sortable timestamp, so lexicographic sort = chronological.
    std::sort( sessionFiles.begin( ), sessionFiles.end( ) );

    const int toDelete = static_cast<int>( sessionFiles.size( ) ) - maxRetainedFiles;
    for( int i = 0; i < toDelete; ++i )
    {
        std::error_code rmEc;
        std::filesystem::remove( sessionFiles[ i ], rmEc );
        // Best-effort; ignore failures (file in use by another process, etc.).
    }
}
