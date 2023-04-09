#pragma once
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <cassert>
#include "traits/TypeTraits.h"
#include <vector>

namespace ImageScraper
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error
    };

    class LoggerBase;

    class Logger : public NonCopyMovable
    {
    public:
        static void AddLogger( std::shared_ptr<LoggerBase> logger )
        {
            m_Loggers.push_back( logger );
        }

        static void Update( );

        static void Log( LogLevel type, const char* message, ... );

        template<typename... Args>
        static void Assert( const char* message, Args... args )
        {
            Log( LogLevel::Error, message, args... );
            assert( ( message, false ) );
        }

    private:
        Logger( ) = default;

        static std::string TimeStamp( );
        static std::vector<std::shared_ptr<LoggerBase>> m_Loggers;
    };
}

#define DebugLog( logLevel, message, ... ) ImageScraper::Logger::Log( logLevel, message, __VA_ARGS__ );
#define InfoLog( message, ... ) ImageScraper::Logger::Log( LogLevel::Info, message, __VA_ARGS__ );
#define WarningLog( message, ... ) ImageScraper::Logger::Log( LogLevel::Warning, message, __VA_ARGS__ );
#define ErrorLog( message, ... ) ImageScraper::Logger::Log( LogLevel::Error, message, __VA_ARGS__ );
#define ErrorAssert( message, ... ) ImageScraper::Logger::Assert( message, __VA_ARGS__ );