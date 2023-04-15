#pragma once
#include <string>
#include <vector>

namespace ImageScraper
{
    struct DownloadOptions
    {
        std::string m_Url;
        std::string m_CaBundle;
        std::vector<char>* m_BufferPtr;
    };
}
