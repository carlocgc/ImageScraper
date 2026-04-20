#pragma once
#include "requests/DownloadRequestTypes.h"
#include "requests/RequestTypes.h"
#include "services/IServiceSink.h"

#include <fstream>

namespace ImageScraper
{
    class DownloadRequest
    {
    public:
        DownloadRequest( std::shared_ptr<IServiceSink> sink );
        RequestResult Perform( const DownloadOptions& options );
        size_t WriteCallback( char* contents, size_t size, size_t nmemb );
        int ProgressCallback( double dltotal, double dlnow, double ultotal, double ulnow );

    private:
        bool IsFloatZero( float value, float epsilon = 1e-6 );
        void CleanupPartialOutputFile( );

        std::vector<char>* m_BufferPtr{ nullptr };
        std::ofstream m_OutputFile{ };
        std::filesystem::path m_OutputFilePath{ };
        size_t m_BytesWritten{ 0 };
        RequestResult m_Result{ };
        std::shared_ptr<IServiceSink> m_Sink{ nullptr };
    };
}
