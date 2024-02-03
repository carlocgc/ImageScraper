#pragma once
#include "Log/LoggerBase.h"
#include <iostream>

namespace ImageScraper
{
    class ConsoleLogger final : public LoggerBase
    {
    public:
        void Log( const LogLine& line ) override
        {
            std::cout << line.m_String;
        }
    };
}