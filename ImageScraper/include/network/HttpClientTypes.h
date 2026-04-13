#pragma once

#include <string>
#include <vector>

namespace ImageScraper
{
    struct HttpRequest
    {
        std::string m_Url{ };
        std::vector<std::string> m_Headers{ };
        std::string m_Body{ };
        std::string m_CaBundle{ };
        std::string m_UserAgent{ };
        int m_TimeoutSeconds{ 30 };
    };

    struct HttpResponse
    {
        int m_StatusCode{ 0 };
        std::string m_Body{ };
        bool m_Success{ false };
        std::string m_Error{ };
    };
}
