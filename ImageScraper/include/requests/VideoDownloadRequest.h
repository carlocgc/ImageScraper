#pragma once

#include "requests/DownloadRequestTypes.h"
#include "requests/RequestTypes.h"
#include "services/IServiceSink.h"

#include <filesystem>
#include <memory>
#include <vector>

struct AVFormatContext;
struct AVPacket;

namespace ImageScraper
{
    class VideoDownloadRequest
    {
    public:
        VideoDownloadRequest( std::shared_ptr<IServiceSink> sink );
        RequestResult Perform( const DownloadOptions& options );

    private:
        static int InterruptCallback( void* opaque );

        bool OpenInput( const DownloadOptions& options );
        bool CreateOutput( );
        bool IsCancelled( ) const;
        void UpdateProgress( const AVPacket& packet ) const;
        void CleanupPartialOutputFile( );
        void Close( );

        AVFormatContext* m_InputFormatCtx{ nullptr };
        AVFormatContext* m_OutputFormatCtx{ nullptr };
        AVPacket* m_Packet{ nullptr };
        std::vector<int> m_OutputStreamByInputStream{ };
        std::filesystem::path m_OutputFilePath{ };
        RequestResult m_Result{ };
        std::shared_ptr<IServiceSink> m_Sink{ nullptr };
        int m_VideoInputStream{ -1 };
        double m_TotalDurationSeconds{ 0.0 };
    };
}
