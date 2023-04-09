#pragma once
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <cassert>

namespace ImageScraper
{
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
}

#define DebugLog( logLevel, message, ... ) ImageScraper::PrintLog( logLevel, message, __VA_ARGS__ );
#define InfoLog( message, ... ) ImageScraper::PrintLog( LogLevel::Info, message, __VA_ARGS__ );
#define WarningLog( message, ... ) ImageScraper::PrintLog( LogLevel::Warning, message, __VA_ARGS__ );
#define ErrorLog( message, ... ) ImageScraper::PrintLog( LogLevel::Error, message, __VA_ARGS__ );
#define ErrorAssert( message, ... ) ImageScraper::AssertLog( message, __VA_ARGS__ );