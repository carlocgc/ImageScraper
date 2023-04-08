#pragma once
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <cassert>

enum class LogLevel
{
    Info,
    Warning,
    Error
};

std::string TimeStamp( );

void PrintLog( LogLevel type, const char* message, ... );

template<typename... Args>
void AssertLog( const char* message, Args... args )
{
    PrintLog( LogLevel::Error, message, args... );
    assert( ( message, false ) );
}

#define DebugLog( logLevel, message, ... ) PrintLog( logLevel, message, __VA_ARGS__ );
#define InfoLog( message, ... ) PrintLog( LogLevel::Info, message, __VA_ARGS__ );
#define WarningLog( message, ... ) PrintLog( LogLevel::Warning, message, __VA_ARGS__ );
#define ErrorLog( message, ... ) PrintLog( LogLevel::Error, message, __VA_ARGS__ );
#define ErrorAssert( message, ... ) AssertLog( message, __VA_ARGS__ );