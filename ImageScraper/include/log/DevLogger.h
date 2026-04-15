#pragma once
#include "Log/LoggerBase.h"
#include <windows.h>

namespace ImageScraper
{
    class DevLogger final : public LoggerBase
    {
    public:
        void Log( const LogLine& line ) override
        {
            OutputDebugStringA( line.m_String.c_str() );
        }
    };
}
