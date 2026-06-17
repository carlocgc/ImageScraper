#include "log/FrontEndLogger.h"
#include "ui/FrontEnd.h"

ImageScraper::FrontEndLogger::FrontEndLogger( std::shared_ptr<FrontEnd> frontEnd )
    : m_FrontEnd{ frontEnd }
{
}

void ImageScraper::FrontEndLogger::Log( const LogLine& line )
{
    if( line.m_Level > m_FrontEnd->GetLogLevel( ) )
    {
        return;
    }

    if( line.m_Level >= LogLevel::Debug )
    {
        m_FrontEnd->Log( line );
        return;
    }

    // Preserve m_Id (and any other future LogLine fields) - the panel keys
    // selection by id, so losing it collapses every panel line to id 0.
    LogLine noDebugLine = line;
    noDebugLine.m_String = RemoveDebugLogInfo( line.m_String );

    m_FrontEnd->Log( noDebugLine );
    return;
}

std::string ImageScraper::FrontEndLogger::RemoveDebugLogInfo( const std::string& line )
{
    std::string out = line;

    // TODO: A lot of str copies here, try optimize
    const std::size_t first = out.find( '[' );
    if( first == std::string::npos )
    {
        return out;
    }

    const std::size_t second = out.find( '[', first + 1 );
    if( second == std::string::npos )
    {
        return out;
    }

    const std::size_t start = out.find( '[', second + 1 );
    if( start == std::string::npos )
    {
        return out;
    }

    // Remove the function name and closing bracket
    std::size_t end = out.find( "] ", start );
    if( end == std::string::npos || end < start )
    {
        return out;
    }

    out.erase( start, end - start + 2 );

    return out;
}
