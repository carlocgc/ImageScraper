#pragma once
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <cassert>

enum class LogType
{
    Info,
    Warning,
    Error
};

void PrintLog( LogType type, const char* message, ... );

template<typename... Args>
void AssertLog( const char* message, Args... args )
{
    PrintLog( LogType::Error, message, args... );
    assert( ( message, false ) );
}

#define DebugLog( LogType, message, ... ) PrintLog( LogType, message, __VA_ARGS__ );
#define InfoLog( message, ... ) PrintLog( LogType::Info, message, __VA_ARGS__ );
#define WarningLog( message, ... ) PrintLog( LogType::Warning, message, __VA_ARGS__ );
#define ErrorLog( message, ... ) PrintLog( LogType::Error, message, __VA_ARGS__ );
#define ErrorAssert( message, ... ) AssertLog( message, __VA_ARGS__ );