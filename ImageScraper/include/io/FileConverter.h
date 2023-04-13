#pragma once
#include <string>
#include <map>

namespace ImageScraper
{
    class FileConverter
    {
    public:
        enum class ConvertFlags : uint8_t
        {
            None = 1,
            DeleteSource = 2
        };

        FileConverter( );

        bool TryConvert( const std::string& sourceFilePath, ConvertFlags convertFlags );

        static const std::map<std::string, std::string> s_ConversionMappings;
    };
}

