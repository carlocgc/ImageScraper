#pragma once

#include "log/LoggerBase.h"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace ImageScraper
{
    // Writes every log line to a per-session file. The file path is
    // <logsDir>/ImageScraper_YYYYMMDD_HHMMSS.log; the directory is created if
    // it does not exist. After the new file opens, older sessions beyond
    // maxRetainedFiles are deleted (oldest first).
    class FileLogger final : public LoggerBase
    {
    public:
        FileLogger( const std::filesystem::path& logsDir, int maxRetainedFiles = 20 );
        ~FileLogger( );

        void Log( const LogLine& line ) override;

        const std::filesystem::path& GetLogFilePath( ) const { return m_FilePath; }
        bool IsOpen( ) const;

    private:
        static std::string MakeSessionFileName( );
        static void PruneOldSessions( const std::filesystem::path& logsDir, int maxRetainedFiles );

        std::filesystem::path m_FilePath;
        std::ofstream         m_Stream;
        std::mutex            m_Mutex;
    };
}
