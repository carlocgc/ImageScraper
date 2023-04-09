#pragma once
#include "DownloadRequestTypes.h"
#include <fstream>

namespace ImageScraper
{
    class DownloadRequest
    {
    public:
        DownloadResult Perform( const DownloadOptions& options );
        size_t WriteCallback( char* contents, size_t size, size_t nmemb );

    private:
        std::vector<char>* m_BufferPtr{ nullptr };
        size_t m_BytesWritten{ 0 };
        DownloadResult m_Result{ };
    };
}