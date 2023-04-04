#pragma once
#include <cstdarg>
#include <cstdio>
#include <iostream>

enum class LogType
{
    Info,
    Warning,
    Error
};

void PrintLog( LogType type, const char* message, ... );

#define DebugLog( logType, message, ... ) PrintLog( logType, message, __VA_ARGS__ );
#define InfoLog( message, ... ) PrintLog( LogType::Info, message, __VA_ARGS__ );
#define WarningLog( message, ... ) PrintLog( LogType::Warning, message, __VA_ARGS__ );
#define ErrorLog( message, ... ) PrintLog( LogType::Error, message, __VA_ARGS__ );
