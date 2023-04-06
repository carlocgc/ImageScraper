#pragma once
#include "Request.h"
#include <fstream>

class DownloadRequest : public Request
{
public:
    RequestResult Perform( const RequestOptions& options ) override;
    size_t WriteCallback( char* contents, size_t size, size_t nmemb );

private:
    std::ofstream m_File{ };
};

