#include "io/FileConverter.h"
#include "utils/DownloadUtils.h"

#include <cstdio>
#include <filesystem>

const std::map<std::string, std::string> ImageScraper::FileConverter::s_ConversionMappings =
{
    { "gifv", "gif" }
};

ImageScraper::FileConverter::FileConverter( )
{
}

bool ImageScraper::FileConverter::TryConvert( const std::string& sourceFilePath, ConvertFlags convertFlags )
{
    for( const auto& [source, target] : s_ConversionMappings )
    {
        const std::string ext = DownloadHelpers::ExtractExtFromFile( sourceFilePath );

        if( ext != source )
        {
            continue;
        }

        const std::size_t dotPos = sourceFilePath.find_last_of( '.' );
        const std::string newFilePath = sourceFilePath.substr( 0, dotPos + 1 ) + target;

        // TODO Do conversion

        ErrorLog( "[%s] NOT IMPLEMENTED Skipped conversion: %s", __FUNCTION__, sourceFilePath.c_str( ) );
        return false; // Debug remove

        //#pragma warning(suppress: 26812)
        if( static_cast< uint8_t >( convertFlags ) & static_cast< uint8_t >( ConvertFlags::DeleteSource ) )
        {
            const int deleteResult = std::remove( sourceFilePath.c_str( ) );

            if( deleteResult == 0 )
            {
                InfoLog( "[%s] Source file deleted: %s" __FUNCTION__, sourceFilePath.c_str( ) );
            }
            else
            {
                WarningLog( "[%s] Failed to delete source file: %s" __FUNCTION__, sourceFilePath.c_str( ) );
            }
        }

        return true;
    }

    return false;
}
