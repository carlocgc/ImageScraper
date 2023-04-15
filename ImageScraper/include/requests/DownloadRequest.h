#pragma once
#include "requests/DownloadRequestTypes.h"
#include "requests/RequestTypes.h"

#include <fstream>

namespace ImageScraper
{
    class DownloadRequest
    {
    public:
        RequestResult Perform( const DownloadOptions& options );
        size_t WriteCallback( char* contents, size_t size, size_t nmemb );

    private:
        std::vector<char>* m_BufferPtr{ nullptr };
        size_t m_BytesWritten{ 0 };
        RequestResult m_Result{ };
    };
}