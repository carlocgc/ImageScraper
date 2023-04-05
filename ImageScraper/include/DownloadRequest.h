#pragma once
#include "Request.h"
#include <fstream>

static

class DownloadRequest : public Request
{
public:
    DownloadRequest( const Config& config, const std::string& endpoint );
    bool Perform( ) override;
    size_t WriteCallback( char* contents, size_t size, size_t nmemb );

protected:
    bool Configure( const Config& config ) override;

    std::string m_FilePath{ };
    std::ofstream m_File{ };
};

