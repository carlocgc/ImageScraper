#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"
#include "utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ImageScraper::DanbooruUtils
{
    using Json = nlohmann::json;

    enum class InputType
    {
        Invalid = 0,
        Query = 1,
        SinglePost = 2
    };

    struct NormalizedInput
    {
        InputType m_Type{ InputType::Invalid };
        std::string m_Query{ };
        std::string m_PostId{ };
        std::string m_Error{ };
    };

    struct PreparedDownload
    {
        std::string m_PostId{ };
        std::string m_SourceUrl{ };
        std::string m_FileName{ };
    };

    inline bool IsAsciiDigitString( const std::string& value )
    {
        return !value.empty( ) && std::all_of( value.begin( ), value.end( ), []( unsigned char c )
            {
                return std::isdigit( c ) != 0;
            } );
    }

    inline std::string ExtractExtensionFromUrl( const std::string& url )
    {
        const std::string normalizedUrl = StringUtils::StripUrlQueryAndFragment( url );
        const std::size_t slashPos = normalizedUrl.find_last_of( '/' );
        const std::string filename = slashPos == std::string::npos ? normalizedUrl : normalizedUrl.substr( slashPos + 1 );
        const std::size_t dotPos = filename.find_last_of( '.' );
        if( dotPos == std::string::npos || dotPos + 1 >= filename.size( ) )
        {
            return { };
        }

        return filename.substr( dotPos + 1 );
    }

    inline std::string NormalizeExtension( std::string extension )
    {
        if( !extension.empty( ) && extension.front( ) == '.' )
        {
            extension.erase( extension.begin( ) );
        }

        return StringUtils::ToLower( extension );
    }

    inline std::string BuildFileName( const std::string& postId, const std::string& extension )
    {
        if( postId.empty( ) || extension.empty( ) )
        {
            return { };
        }

        return postId + "." + extension;
    }

    inline std::string SanitizePathComponent( const std::string& value, const std::string& fallback = "query" )
    {
        std::string sanitized{ };
        sanitized.reserve( value.size( ) );

        bool lastWasUnderscore = false;
        for( unsigned char c : value )
        {
            const bool allowed = std::isalnum( c ) != 0 || c == '.' || c == '-' || c == '_';
            if( allowed )
            {
                sanitized.push_back( static_cast<char>( c ) );
                lastWasUnderscore = false;
                continue;
            }

            if( !lastWasUnderscore )
            {
                sanitized.push_back( '_' );
                lastWasUnderscore = true;
            }
        }

        while( !sanitized.empty( ) && ( sanitized.back( ) == '.' || sanitized.back( ) == '_' || sanitized.back( ) == ' ' ) )
        {
            sanitized.pop_back( );
        }

        return sanitized.empty( ) ? fallback : sanitized;
    }

    inline std::optional<std::string> ExtractPostIdFromUrl( const std::string& input )
    {
        const std::string withoutQuery = StringUtils::StripUrlQueryAndFragment( input );
        const std::string lower = StringUtils::ToLower( withoutQuery );
        const std::string marker = "/posts/";
        const std::size_t markerPos = lower.find( marker );
        if( markerPos == std::string::npos )
        {
            return std::nullopt;
        }

        std::size_t start = markerPos + marker.size( );
        std::size_t end = start;
        while( end < withoutQuery.size( ) && std::isdigit( static_cast<unsigned char>( withoutQuery[ end ] ) ) )
        {
            ++end;
        }

        if( end == start )
        {
            return std::nullopt;
        }

        return withoutQuery.substr( start, end - start );
    }

    inline std::string ExtractUrlQueryValue( const std::string& input, const std::string& key )
    {
        const std::string search = key + "=";
        const std::size_t queryStart = input.find( '?' );
        const std::size_t startSearch = queryStart == std::string::npos ? 0 : queryStart + 1;
        std::size_t start = input.find( search, startSearch );
        while( start != std::string::npos )
        {
            const bool startsParam = start == 0 || input[ start - 1 ] == '?' || input[ start - 1 ] == '&';
            if( startsParam )
            {
                start += search.size( );
                const std::size_t ampPos = input.find( '&', start );
                const std::size_t fragmentPos = input.find( '#', start );
                std::size_t end = input.size( );
                if( ampPos != std::string::npos )
                {
                    end = ( std::min )( end, ampPos );
                }
                if( fragmentPos != std::string::npos )
                {
                    end = ( std::min )( end, fragmentPos );
                }

                return input.substr( start, end - start );
            }

            start = input.find( search, start + search.size( ) );
        }

        return { };
    }
    inline NormalizedInput NormalizeInput( const std::string& rawInput )
    {
        const std::string input = StringUtils::Trim( rawInput );
        if( input.empty( ) )
        {
            return { InputType::Invalid, { }, { }, "Danbooru query cannot be empty." };
        }

        const std::string queryParam = ExtractUrlQueryValue( input, "q" );
        if( !queryParam.empty( ) )
        {
            return { InputType::Query, StringUtils::Trim( StringUtils::UrlDecode( queryParam ) ), { }, { } };
        }

        const std::string tagsParam = ExtractUrlQueryValue( input, "tags" );
        if( !tagsParam.empty( ) )
        {
            return { InputType::Query, StringUtils::Trim( StringUtils::UrlDecode( tagsParam ) ), { }, { } };
        }

        const std::optional<std::string> postId = ExtractPostIdFromUrl( input );
        if( postId.has_value( ) )
        {
            return { InputType::SinglePost, { }, *postId, { } };
        }

        if( input.find( "danbooru.donmai.us" ) != std::string::npos )
        {
            return { InputType::Invalid, { }, { }, "Danbooru URL must contain q=, tags=, or a /posts/<id> path." };
        }

        return { InputType::Query, input, { }, { } };
    }

    inline std::optional<PreparedDownload> PreparedDownloadFromPost( const Json& post )
    {
        if( !post.is_object( ) || !post.contains( "id" ) || !post.contains( "file_url" ) || !post[ "file_url" ].is_string( ) )
        {
            WarningLog( "[%s] Skipping Danbooru post with missing id or file_url.", __FUNCTION__ );
            return std::nullopt;
        }

        std::string postId{ };
        if( post[ "id" ].is_number_integer( ) )
        {
            postId = std::to_string( post[ "id" ].get<long long>( ) );
        }
        else if( post[ "id" ].is_string( ) )
        {
            postId = post[ "id" ].get<std::string>( );
        }

        const std::string sourceUrl = post[ "file_url" ].get<std::string>( );
        if( postId.empty( ) || sourceUrl.empty( ) )
        {
            WarningLog( "[%s] Skipping Danbooru post with empty id or file_url.", __FUNCTION__ );
            return std::nullopt;
        }

        std::string extension{ };
        if( post.contains( "file_ext" ) && post[ "file_ext" ].is_string( ) )
        {
            extension = NormalizeExtension( post[ "file_ext" ].get<std::string>( ) );
        }

        if( extension.empty( ) )
        {
            extension = NormalizeExtension( ExtractExtensionFromUrl( sourceUrl ) );
        }

        const std::string fileName = BuildFileName( postId, extension );
        if( fileName.empty( ) )
        {
            WarningLog( "[%s] Skipping Danbooru post %s with missing file extension.", __FUNCTION__, postId.c_str( ) );
            return std::nullopt;
        }

        return PreparedDownload{ postId, sourceUrl, fileName };
    }

    inline void AppendDownloadablePosts( const Json& response,
                                         int maxItems,
                                         std::unordered_set<std::string>& seenPostIds,
                                         std::vector<PreparedDownload>& downloads )
    {
        if( maxItems <= 0 || !response.is_array( ) )
        {
            return;
        }

        for( const Json& post : response )
        {
            if( static_cast<int>( downloads.size( ) ) >= maxItems )
            {
                return;
            }

            const std::optional<PreparedDownload> download = PreparedDownloadFromPost( post );
            if( !download.has_value( ) )
            {
                continue;
            }

            if( !seenPostIds.insert( download->m_PostId ).second )
            {
                continue;
            }

            downloads.push_back( *download );
        }
    }

    inline std::vector<PreparedDownload> GetDownloadablePostsFromResponse( const Json& response, int maxItems )
    {
        std::vector<PreparedDownload> downloads{ };
        std::unordered_set<std::string> seenPostIds{ };
        AppendDownloadablePosts( response, maxItems, seenPostIds, downloads );
        return downloads;
    }
}
