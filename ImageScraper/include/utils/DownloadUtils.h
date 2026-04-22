#pragma once

#include "log/Logger.h"
#include "requests/RequestTypes.h"
#include "nlohmann/json.hpp"

#include <string>
#include <algorithm>
#include <filesystem>

namespace ImageScraper::DownloadHelpers
{
    using Json = nlohmann::json;

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
            LogDebug( "[%s] %s did not contain a file name and ext", __FUNCTION__, url.c_str() );
            return "";
        }

        const std::string filename = url.substr( slashPos + 1 );

        const std::size_t dotPos = filename.find_last_of( "." );
        if( dotPos == std::string::npos )
        {
            LogDebug( "[%s] %s did not contain a file name and ext", __FUNCTION__, url.c_str( ) );
            return "";
        }

        return filename;
    }

    static std::string ExtractExtFromFile( const std::string& file )
    {
        const std::size_t dotPos = file.find_last_of( "." );
        if( dotPos == std::string::npos )
        {
            LogDebug( "[%s] %s did not contain a file name and ext", __FUNCTION__, file .c_str( ) );
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
            LogDebug( "[%s] %s did not contain a file name and ext", __FUNCTION__, url.c_str( ) );
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
                LogError( "[%s] Failed to create download folder, invalid path: %s", __FUNCTION__, dir );
                return false;
            }
        }

        InfoLog( "[%s] Created download folder: %s", __FUNCTION__, dir.c_str() );
        return true;
    }

    static std::filesystem::path GetProviderRoot( const std::string& filepath )
    {
        std::filesystem::path result;
        bool foundDownloads = false;
        for( const auto& part : std::filesystem::path( filepath ) )
        {
            result /= part;
            if( foundDownloads )
            {
                return result;
            }

            if( part == "Downloads" )
            {
                foundDownloads = true;
            }
        }

        return { };
    }

    static std::string GetProviderName( const std::string& filepath )
    {
        return GetProviderRoot( filepath ).filename( ).string( );
    }

    static std::string GetSubfolderLabel( const std::string& filepath )
    {
        const auto providerRoot = GetProviderRoot( filepath );
        if( providerRoot.empty( ) )
        {
            return { };
        }

        const auto fileDir = std::filesystem::path( filepath ).parent_path( );
        std::error_code ec;
        const auto relativePath = std::filesystem::relative( fileDir, providerRoot, ec );
        if( ec || relativePath.empty( ) || relativePath == std::filesystem::path( "." ) )
        {
            return { };
        }

        const std::string subfolderName = relativePath.generic_string( );
        const std::string providerName = providerRoot.filename( ).string( );

        if( providerName == "4chan" )
        {
            return "/" + subfolderName + "/";
        }

        if( providerName == "Reddit" )
        {
            return "r/" + subfolderName;
        }

        if( providerName == "Tumblr" )
        {
            return "@" + subfolderName;
        }

        if( providerName == "Bluesky" )
        {
            return "@" + subfolderName;
        }

        return subfolderName;
    }

    static std::string CreateQueryParamString( const std::vector<QueryParam>& params )
    {
        std::string paramString{ };

        if( !params.empty( ) )
        {
            paramString += '?';
        }

        int paramCount = 0;

        for( const auto& param : params )
        {
            if( paramCount++ > 0 )
            {
                paramString += '&';
            }

            paramString += param.m_Key;
            paramString += '=';
            paramString += param.m_Value;
        }

        return paramString;
    }

    static bool IsRedditResponseError( RequestResult& result )
    {
        try
        {
            Json payload = Json::parse( result.m_Response );

            if( payload.contains( "error" ) )
            {
                const int errorCode = payload[ "error" ];
                result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( errorCode );
                return true;
            }

            if( payload.contains( "message" ) )
            {
                result.m_Error.m_ErrorString = payload[ "message" ];
            }
        }
        catch( const Json::exception& ex )
        {
            result.m_Error.m_ErrorCode = ResponseErrorCode::InternalServerError;
            result.m_Error.m_ErrorString = ex.what( );
            return true;
        }

        return false;
    }

    static bool IsTumblrResponseError( RequestResult& result )
    {
        try
        {
            Json payload = Json::parse( result.m_Response );

            if( payload.contains( "error" ) )
            {
                const int errorCode = payload[ "error" ];
                result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( errorCode );
                return true;
            }

            if( payload.contains( "message" ) )
            {
                result.m_Error.m_ErrorString = payload[ "message" ];
            }
        }
        catch( const Json::exception& ex )
        {
            result.m_Error.m_ErrorCode = ResponseErrorCode::InternalServerError;
            result.m_Error.m_ErrorString = ex.what( );
            return true;
        }

        return false;
    }

    static bool IsFourChanResponseError( RequestResult& result )
    {
        try
        {
            Json payload = Json::parse( result.m_Response );

            if( payload.contains( "error" ) )
            {
                const int errorCode = payload[ "error" ];
                result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( errorCode );
                return true;
            }

            if( payload.contains( "message" ) )
            {
                result.m_Error.m_ErrorString = payload[ "message" ];
            }
        }
        catch( const Json::exception& ex )
        {
            result.m_Error.m_ErrorCode = ResponseErrorCode::InternalServerError;
            result.m_Error.m_ErrorString = ex.what( );
            return true;
        }

        return false;
    }
}
