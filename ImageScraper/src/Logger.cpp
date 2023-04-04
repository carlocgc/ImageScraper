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

    const int len = static_cast< int >( strlen( output ) );
    output[ len - 1 ] = '\n';
    output[ len ] = '\0';

    std::cout << output;
    OutputDebugStringA( output );
}
