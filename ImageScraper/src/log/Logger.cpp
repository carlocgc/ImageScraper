#include "log/Logger.h"
#include "log/LoggerBase.h"

#include <chrono>

#define LOG_MAX_SIZE 1024

std::vector<std::shared_ptr<ImageScraper::LoggerBase>> ImageScraper::Logger::m_Loggers;

std::string ImageScraper::Logger::TimeStamp( )
{
    std::chrono::system_clock::time_point timePoint = std::chrono::system_clock::now( );
    std::time_t timeT = std::chrono::system_clock::to_time_t( timePoint );
    std::tm gmt{ };
    gmtime_s( &gmt, &timeT );
    std::chrono::duration<double> fractionalSeconds = ( timePoint - std::chrono::system_clock::from_time_t( timeT ) ) + std::chrono::seconds( gmt.tm_sec );

    std::string buffer( "[year/mo/dy hr:mn:sc.xxxxxx]" );
    sprintf_s( &buffer.front( ), buffer.size( ) + 1, "[%04d/%02d/%02d %02d:%02d:%09.6f]", gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday, gmt.tm_hour, gmt.tm_min, fractionalSeconds.count( ) );
    return buffer;
}

void ImageScraper::Logger::Log( LogLevel logLevel, const char* message, ... )
{
    char output[ LOG_MAX_SIZE ];

    const std::string timeStamp = TimeStamp( );

    strncpy_s( output, timeStamp.c_str( ), LOG_MAX_SIZE );

    std::string levelTag = "";

    switch( logLevel )
    {
    case LogLevel::Warning:
        levelTag = " [WARN] ";
        break;
    case LogLevel::Error:
        levelTag = " [ERRO] ";
        break;
    case LogLevel::Debug:
        levelTag = " [DBUG] ";
        break;
    default:
        levelTag = " [INFO] ";
        break;
    }

    strcat_s( output, LOG_MAX_SIZE, levelTag.c_str( ) );

    const uint64_t preLen = static_cast< uint64_t >( strlen( output ) );

    va_list ap;
    va_start( ap, message );
    vsnprintf( output + preLen, LOG_MAX_SIZE - preLen, message, ap );
    output[ LOG_MAX_SIZE - 1 ] = '\0';
    va_end( ap );

    const uint64_t postLen = static_cast< uint64_t >( strlen( output ) );

    if( postLen >= LOG_MAX_SIZE - 1 )
    {
        output[ LOG_MAX_SIZE - 2 ] = '\n';
        output[ LOG_MAX_SIZE - 1 ] = '\0';
    }
    else
    {
        strcat_s( output, "\n" );
    }

    for( auto logger : m_Loggers )
    {
        LogLine line{ logLevel, output };
        logger->Log( line );
    }
}
