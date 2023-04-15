#pragma once

#include "log/Logger.h"
#include "requests/RequestTypes.h"
#include "nlohmann/json.hpp"

#include <string>
#include <algorithm>
#include <filesystem>

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

    static std::string RedirectToPreferredFileTypeUrl( const std::string& url )
    {
        const std::string ext = ExtractExtFromFile( url );

        // TODO Add a map of unwanted types and preferred types
        if( ext != "gifv" )
        {
            return "";
        }

        const std::size_t dotPos = url.find_last_of( "." );
        if( dotPos == std::string::npos )
        {
            DebugLog( "[%s] %s did not contain a file name and ext", __FUNCTION__, url.c_str( ) );
            return "";
        }

        // TODO add mapping for preferred file ext
        const std::string newUrl = url.substr( 0, dotPos + 1 ) + "mp4";
        return newUrl;
    }

    static bool CreateDir( const std::string& dir )
    {
        if( !std::filesystem::exists( dir ) )
        {
            if( !std::filesystem::create_directories( dir ) )
            {
                ErrorLog( "[%s] Failed to create download folder, invalid path: %s", __FUNCTION__, dir );
                return false;
            }
        }

        InfoLog( "[%s] Created download folder: %s", __FUNCTION__, dir.c_str() );
        return true;
    }

    static bool IsResponseError( RequestResult& result )
    {
        using Json = nlohmann::json;

        try
        {
            Json payload = Json::parse( result.m_Response );

            if( payload.contains( "error" ) )
            {
                // TODO Parse error properly
                result.m_Error.m_ErrorCode = ResponseErrorCode::Unknown;
                result.m_Error.m_ErrorString = "Response Error!";
                return true;
            }
        }
        catch( const Json::exception& ex )
        {
            result.m_Error.m_ErrorCode = ResponseErrorCode::Unknown;
            result.m_Error.m_ErrorString = ex.what( );
            return true;
        }

        return false;
    }
}