#pragma once

#include <filesystem>
#include <string>

namespace ImageScraper
{
    struct DownloadOptions
    {
        std::string m_Url{ };
        std::string m_CaBundle{ };
        std::string m_UserAgent{ };
        std::filesystem::path m_OutputFilePath{ };
    };
}
