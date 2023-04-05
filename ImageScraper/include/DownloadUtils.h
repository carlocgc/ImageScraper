#pragma once
#include <string>
#include <algorithm>
#include <filesystem>
#include "Logger.h"

namespace DownloadHelpers
{
    static std::string UrlToSafeString( const std::string& url )
    {
        // Remove scheme and colon
        std::string safeString = url;
        safeString.erase( 0, safeString.find( "://" ) + 3 );

        // Replace forward slashes with hyphens
        std::replace( safeString.begin( ), safeString.end( ), '/', '-' );

        // Remove query parameters and fragments, npos is returned for no match result
        std::size_t query_pos = safeString.find( '?' );
        if( query_pos != std::string::npos )
        {
            safeString.erase( query_pos );
        }

        std::size_t fragment_pos = safeString.find( '#' );
        if( fragment_pos != std::string::npos )
        {
            safeString.erase( fragment_pos );
        }

        InfoLog( "[%s] Successfully parsed url to safe string: %s", __FUNCTION__, safeString );
        return safeString;
    }

    static std::string ExtractFileNameAndExtFromUrl( const std::string& url )
    {
        // TODO Remove query params and fragments

        std::size_t slashPos = url.find_last_of( "/" );
        if( slashPos == std::string::npos )
        {
            InfoLog( "[%s] %s did not contain a file name and ext", __FUNCTION__, url );
            return "";
        }

        std::string filename = url.substr( slashPos + 1 );

        std::size_t dotPos = filename.find_last_of( "." );
        if( dotPos == std::string::npos )
        {
            InfoLog( "[%s] %s did not contain a file name and ext", __FUNCTION__, url );
            return "";
        }

        return filename;
    }

    static std::string CreateFolder( const std::string& folderName )
    {
        std::filesystem::path folderPath = std::filesystem::current_path( ) / folderName;
        std::string pathString = folderPath.generic_string( );

        if( !std::filesystem::exists( folderPath ) )
        {
            if( !std::filesystem::create_directory( folderPath ) )
            {
                ErrorLog( "[%s] Failed to create folder, invalid path: %s", __FUNCTION__, pathString );
                return "";
            }
        }

        InfoLog( "[%s] Successfully created folder: %s", __FUNCTION__, folderPath );
        return pathString;
    }
}