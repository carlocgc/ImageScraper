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

    LogLine noDebugLine{ };
    noDebugLine.m_Level = line.m_Level;
    noDebugLine.m_String = RemoveDebugLogInfo( line.m_String );

    m_FrontEnd->Log( noDebugLine );
    return;
}

std::string ImageScraper::FrontEndLogger::RemoveDebugLogInfo( const std::string& line )
{
    std::string out = line;

    // TODO: A lot of str copies here, try optimize
    // Find the index of the 3rd occurrence of the '[' character
    std::size_t start = out.find_first_of( "[", out.find_first_of( "[", out.find_first_of( "[" ) + 1 ) + 1 );

    // Remove the function name and closing bracket
    std::size_t end = out.find( "] ", start );
    out.erase( start, end - start + 2 );

    return out;
}
