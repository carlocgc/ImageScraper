#include "Logger.h"
#include <windows.h>

#define LOGTYPE_CHAR_COUNT 7
#define LOG_MAX_SIZE 1024

void PrintLog( LogType type, const char* message, ... )
{
    char output[ LOG_MAX_SIZE ];

    switch( type )
    {
    case LogType::Warning:
        strncpy_s( output, "[WARN] ", LOG_MAX_SIZE );
        break;
    case LogType::Error:
        strncpy_s( output, "[ERRO] ", LOG_MAX_SIZE );
        break;
    default:
        strncpy_s( output, "[INFO] ", LOG_MAX_SIZE );
        break;
    }

    va_list ap;
    va_start( ap, message );
    vsnprintf( output + LOGTYPE_CHAR_COUNT, LOG_MAX_SIZE - LOGTYPE_CHAR_COUNT, message, ap );
    output[ LOG_MAX_SIZE - 1 ] = '\0';
    va_end( ap );

    uint64_t len = static_cast<uint64_t>( strlen( output ) );

    if( len >= LOG_MAX_SIZE - 1 )
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
