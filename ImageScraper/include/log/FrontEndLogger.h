#pragma once

#include "log/LoggerBase.h"

#include <string>
#include <memory>
#include "ui/FrontEnd.h"

namespace ImageScraper
{

    class FrontEndLogger : public LoggerBase
    {
    public:
        FrontEndLogger( std::shared_ptr<FrontEnd> frontEnd );
        void Log( const LogLine& line ) override;

    private:
        std::shared_ptr<FrontEnd> m_FrontEnd;
        std::string RemoveDebugLogInfo( const std::string& line );
    };
}

