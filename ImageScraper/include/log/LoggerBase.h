#pragma once

namespace ImageScraper
{
    struct LogLine;

    class LoggerBase
    {
    public:
        virtual void Log( const LogLine& line ) = 0;
    };
}
