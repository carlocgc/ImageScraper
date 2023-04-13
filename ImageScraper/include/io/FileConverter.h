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

        bool TryConvert( const std::string& sourceFilePath, ConvertFlags convertFlags );
        bool convert_gifv_to_gif( const std::string& input_file, const std::string& output_file );

        static const std::map<std::string, std::string> s_ConversionMappings;
    };
}

