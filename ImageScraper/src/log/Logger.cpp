#include "log/Logger.h"

#include <windows.h>
#include <chrono>
#include <thread>
#include <mutex>

#define LOG_MAX_SIZE 1024

std::string ImageScraper::TimeStamp( )
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

void ImageScraper::PrintLog( LogLevel type, const char* message, ... )
{
    char output[ LOG_MAX_SIZE ];

    const std::string timeStamp = TimeStamp( );

    strncpy_s( output, timeStamp.c_str( ), LOG_MAX_SIZE );

    switch( type )
    {
    case LogLevel::Warning:
        strcat_s( output, LOG_MAX_SIZE, " [WARN] " );
        //strncpy_s( output, "[WARN] ", LOG_MAX_SIZE );
        break;
    case LogLevel::Error:
        strcat_s( output, LOG_MAX_SIZE, " [ERRO] " );
        //strncpy_s( output, "[ERRO] ", LOG_MAX_SIZE );
        break;
    default:
        strcat_s( output, LOG_MAX_SIZE, " [INFO] " );
        //strncpy_s( output, "[INFO] ", LOG_MAX_SIZE );
        break;
    }

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

    std::cout << output;

    OutputDebugStringA( output );
}
