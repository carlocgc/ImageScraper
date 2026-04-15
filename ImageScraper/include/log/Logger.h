#pragma once

#include "traits/TypeTraits.h"

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <cassert>
#include <vector>
#include <mutex>

namespace ImageScraper
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error,
        Debug
    };

    struct LogLine
    {
        LogLevel m_Level;
        std::string m_String;
    };

    class LoggerBase;

    class Logger : public NonCopyMovable
    {
    public:
        static void AddLogger( std::shared_ptr<LoggerBase> logger )
        {
            s_Loggers.push_back( logger );
        }

        static void Log( LogLevel logLevel, const char* message, ... );

        template<typename... Args>
        static void Assert( const char* message, Args... args )
        {
            Log( LogLevel::Error, message, args... );
            assert( ( message, false ) );
        }

    private:
        Logger( ) = default;

        static std::string TimeStamp( );
        static std::vector<std::shared_ptr<LoggerBase>> s_Loggers;
        static std::mutex s_LogMutex;
    };
}

#define InfoLog( message, ... ) ImageScraper::Logger::Log( LogLevel::Info, message, __VA_ARGS__ );
#define WarningLog( message, ... ) ImageScraper::Logger::Log( LogLevel::Warning, message, __VA_ARGS__ );
#define ErrorLog( message, ... ) ImageScraper::Logger::Log( LogLevel::Error, message, __VA_ARGS__ );
#define DebugLog( message, ... ) ImageScraper::Logger::Log( LogLevel::Debug, message, __VA_ARGS__ );

// TODO Add condition to assert
#define Assert( message, ... ) ImageScraper::Logger::Assert( message, __VA_ARGS__ );