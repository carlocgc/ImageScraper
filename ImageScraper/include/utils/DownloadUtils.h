#pragma once
#include <string>
#include <algorithm>
#include <filesystem>
#include "log/Logger.h"
#include "requests/RequestTypes.h"

namespace ImageScraper::DownloadHelpers
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

        InfoLog( "[%s] Successfully parsed url to safe string: %s", __FUNCTION__, safeString.c_str() );
        return safeString;
    }

    static std::string ExtractFileNameAndExtFromUrl( const std::string& url )
    {
        // TODO Remove query params and fragments

        const std::size_t slashPos = url.find_last_of( "/" );
        if( slashPos == std::string::npos )
        {
            DebugLog( "[%s] %s did not contain a file name and ext", __FUNCTION__, url.c_str() );
            return "";
        }

        const std::string filename = url.substr( slashPos + 1 );

        const std::size_t dotPos = filename.find_last_of( "." );
        if( dotPos == std::string::npos )
        {
            DebugLog( "[%s] %s did not contain a file name and ext", __FUNCTION__, url.c_str( ) );
            return "";
        }

        return filename;
    }

    static std::string ExtractExtFromFile( const std::string& file )
    {
        const std::size_t dotPos = file.find_last_of( "." );
        if( dotPos == std::string::npos )
        {
            DebugLog( "[%s] %s did not contain a file name and ext", __FUNCTION__, file .c_str( ) );
            return "";
        }

        const std::string ext = file.substr( dotPos + 1 );

        return ext;
    }


    static bool CreateDir( const std::string& dir )
    {
        if( !std::filesystem::exists( dir ) )
        {
            if( !std::filesystem::create_directories( dir ) )
            {
                ErrorLog( "[%s] Failed to create directory, invalid path: %s", __FUNCTION__, dir );
                return false;
            }
        }

        InfoLog( "[%s] Successfully created folder: %s", __FUNCTION__, dir.c_str() );
        return true;
    }

    static bool IsResponseError( RequestResult& result )
    {
        if( result.m_Response.find( "error" ) == std::string::npos )
        {
            result.m_Error.m_ErrorCode = ResponseErrorCode::None;
            return false;
        }

        // TODO Parse response for error code
        result.m_Error.m_ErrorCode = ResponseErrorCode::Unknown;
        return true;
    }
}