#pragma once
#include "requests/DownloadRequestTypes.h"
#include "requests/RequestTypes.h"

#include <fstream>

namespace ImageScraper
{
    class FrontEnd;

    class DownloadRequest
    {
    public:
        DownloadRequest( std::shared_ptr<FrontEnd> frontEnd );
        RequestResult Perform( const DownloadOptions& options );
        size_t WriteCallback( char* contents, size_t size, size_t nmemb );
        int ProgressCallback( double dltotal, double dlnow, double ultotal, double ulnow );

    private:
        bool IsFloatZero( float value, float epsilon = 1e-6 );

        std::vector<char>* m_BufferPtr{ nullptr };
        size_t m_BytesWritten{ 0 };
        RequestResult m_Result{ };
        std::shared_ptr<FrontEnd> m_FrontEnd{ nullptr };
    };
}