#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace ImageScraper::TumblrUtils
{
    using Json = nlohmann::json;

    // Extracts the value of a named HTML attribute from a tag string.
    // Handles both single-quoted and double-quoted values.
    // e.g. ExtractAttr( "<img src=\"foo.jpg\">", "src" ) -> "foo.jpg"
    inline std::string ExtractAttr( const std::string& tag, const std::string& attr )
    {
        for( char quote : { '"', '\'' } )
        {
            const std::string search = attr + "=" + quote;
            std::size_t start = tag.find( search );
            if( start == std::string::npos )
            {
                continue;
            }
            start += search.size( );
            const std::size_t end = tag.find( quote, start );
            if( end == std::string::npos )
            {
                continue;
            }
            return tag.substr( start, end - start );
        }
        return { };
    }

    // Returns the URL from the highest-resolution entry in an HTML srcset attribute value.
    // Tumblr srcset entries are ordered ascending by width, so this takes the last entry
    // and strips the trailing width descriptor (e.g. "1280w").
    inline std::string BestUrlFromSrcset( const std::string& srcset )
    {
        const std::size_t lastComma = srcset.rfind( ',' );
        std::string lastEntry = ( lastComma != std::string::npos )
            ? srcset.substr( lastComma + 1 )
            : srcset;

        // trim leading whitespace
        const std::size_t contentStart = lastEntry.find_first_not_of( " \t\r\n" );
        if( contentStart == std::string::npos )
        {
            return { };
        }
        lastEntry = lastEntry.substr( contentStart );

        // strip trailing width descriptor (e.g. " 1280w")
        const std::size_t spacePos = lastEntry.rfind( ' ' );
        if( spacePos != std::string::npos )
        {
            return lastEntry.substr( 0, spacePos );
        }

        return lastEntry;
    }

    // Extracts image URLs from HTML by scanning <img> tags.
    // Prefers the highest-resolution srcset entry over src when srcset is present.
    inline void ExtractImgUrlsFromHtml( const std::string& html, std::vector<std::string>& out )
    {
        std::size_t pos = 0;
        while( pos < html.size( ) )
        {
            const std::size_t tagStart = html.find( "<img", pos );
            if( tagStart == std::string::npos )
            {
                break;
            }

            const std::size_t tagEnd = html.find( '>', tagStart );
            if( tagEnd == std::string::npos )
            {
                break;
            }

            const std::string tag = html.substr( tagStart, tagEnd - tagStart + 1 );

            const std::string srcset = ExtractAttr( tag, "srcset" );
            if( !srcset.empty( ) )
            {
                const std::string best = BestUrlFromSrcset( srcset );
                if( !best.empty( ) )
                {
                    out.push_back( best );
                    pos = tagEnd + 1;
                    continue;
                }
            }

            const std::string src = ExtractAttr( tag, "src" );
            if( !src.empty( ) )
            {
                out.push_back( src );
            }

            pos = tagEnd + 1;
        }
    }

    // Extracts video URLs from HTML by scanning <video src="..."> and <source src="..."> tags.
    inline void ExtractVideoUrlsFromHtml( const std::string& html, std::vector<std::string>& out )
    {
        for( const std::string& tagPrefix : { std::string( "<video" ), std::string( "<source" ) } )
        {
            std::size_t pos = 0;
            while( pos < html.size( ) )
            {
                const std::size_t tagStart = html.find( tagPrefix, pos );
                if( tagStart == std::string::npos )
                {
                    break;
                }

                const std::size_t tagEnd = html.find( '>', tagStart );
                if( tagEnd == std::string::npos )
                {
                    break;
                }

                const std::string tag = html.substr( tagStart, tagEnd - tagStart + 1 );
                const std::string src = ExtractAttr( tag, "src" );
                if( !src.empty( ) )
                {
                    out.push_back( src );
                }

                pos = tagEnd + 1;
            }
        }
    }

    inline std::vector<std::string> GetMediaUrlsFromResponse( const Json& response, int maxItems )
    {
        std::vector<std::string> mediaUrls{ };

        for( const auto& post : response[ "response" ][ "posts" ] )
        {
            if( post[ "type" ] == "photo" )
            {
                for( const auto& photo : post[ "photos" ] )
                {
                    if( photo.contains( "original_size" ) )
                    {
                        mediaUrls.push_back( photo[ "original_size" ][ "url" ] );
                        if( static_cast<int>( mediaUrls.size( ) ) >= maxItems )
                        {
                            return mediaUrls;
                        }
                    }
                }
            }
            else if( post[ "type" ] == "video" )
            {
                if( post.contains( "video_url" ) )
                {
                    mediaUrls.push_back( post[ "video_url" ] );
                    if( static_cast<int>( mediaUrls.size( ) ) >= maxItems )
                    {
                        return mediaUrls;
                    }
                }
            }
            else if( post[ "type" ] == "text" )
            {
                if( post.contains( "body" ) )
                {
                    const std::string body = post[ "body" ].get<std::string>( );
                    std::vector<std::string> extracted;
                    ExtractImgUrlsFromHtml( body, extracted );
                    ExtractVideoUrlsFromHtml( body, extracted );

                    for( const auto& url : extracted )
                    {
                        mediaUrls.push_back( url );
                        if( static_cast<int>( mediaUrls.size( ) ) >= maxItems )
                        {
                            return mediaUrls;
                        }
                    }
                }
            }
        }

        return mediaUrls;
    }
}
