#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"
#include "utils/StringUtils.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ImageScraper::RedgifsUtils
{
    using Json = nlohmann::json;

    // True for redgifs URLs that need slug resolution (i.e. watch pages and
    // /ifr/ embeds, which are HTML pages, not media). Direct CDN URLs like
    // https://media.redgifs.com/<Slug>.mp4 return false - those are already
    // downloadable and must not be sent through the resolver.
    //
    // Examples that match:
    //   https://redgifs.com/watch/<slug>
    //   https://www.redgifs.com/watch/<slug>
    //   https://v3.redgifs.com/watch/<slug>
    //   https://www.redgifs.com/ifr/<slug>
    // Case-insensitive.
    inline bool IsRedgifsUrl( const std::string& url )
    {
        const std::string lowered = StringUtils::ToLower( url );
        if( lowered.find( "redgifs.com" ) == std::string::npos )
        {
            return false;
        }

        return lowered.find( "/watch/" ) != std::string::npos
            || lowered.find( "/ifr/" )   != std::string::npos;
    }

    // Pulls the lowercased gif slug from a redgifs URL. Returns empty
    // string if no slug can be parsed. Pure - no HTTP.
    inline std::string ExtractSlug( const std::string& url )
    {
        std::string trimmed = StringUtils::StripUrlQueryAndFragment( url );
        const std::string lowered = StringUtils::ToLower( trimmed );

        static const char* k_Markers[] = { "/watch/", "/ifr/" };
        std::size_t slugStart = std::string::npos;
        for( const char* marker : k_Markers )
        {
            const std::size_t pos = lowered.find( marker );
            if( pos != std::string::npos )
            {
                slugStart = pos + std::string{ marker }.size( );
                break;
            }
        }

        if( slugStart == std::string::npos || slugStart >= trimmed.size( ) )
        {
            return { };
        }

        std::string slug = trimmed.substr( slugStart );

        // Strip any trailing path segments and SEO descriptor tails. Redgifs
        // sometimes appends a slug with descriptive words separated by '-'
        // (e.g. /watch/<slug>-some-descriptive-text). The actual gif slug is
        // the part before the first '-'; sending the long form to the API
        // returns 405 because it doesn't match the /v2/gifs/<slug> route.
        const std::size_t cutPos = slug.find_first_of( "/-" );
        if( cutPos != std::string::npos )
        {
            slug.erase( cutPos );
        }

        return StringUtils::ToLower( slug );
    }

    // Picks the best available CDN URL from a redgifs `gif.urls` object.
    // Prefers HD over SD; returns empty string if neither is present.
    inline std::string PickMediaUrl( const Json& urls )
    {
        for( const char* key : { "hd", "sd" } )
        {
            if( urls.is_object( ) && urls.contains( key ) && urls[ key ].is_string( ) )
            {
                std::string mp4 = urls[ key ].get<std::string>( );
                if( !mp4.empty( ) )
                {
                    return mp4;
                }
            }
        }

        return { };
    }

    // Parses a /v2/auth/temporary response body. On success populates outToken
    // and outTtlSeconds (defaults to 3600 if the server didn't include one)
    // and returns true. On any parse failure or missing token, returns false
    // and leaves outputs unchanged.
    inline bool ExtractTokenFromAuthResponse( const std::string& body, std::string& outToken, int& outTtlSeconds )
    {
        try
        {
            const Json parsed = Json::parse( body );
            if( !parsed.contains( "token" ) || !parsed[ "token" ].is_string( ) )
            {
                LogError( "[%s] redgifs auth response missing token field", __FUNCTION__ );
                return false;
            }

            outToken = parsed[ "token" ].get<std::string>( );

            if( parsed.contains( "expiresIn" ) && parsed[ "expiresIn" ].is_number_integer( ) )
            {
                outTtlSeconds = parsed[ "expiresIn" ].get<int>( );
            }
            else
            {
                outTtlSeconds = 3600;
            }

            return true;
        }
        catch( const Json::exception& ex )
        {
            LogError( "[%s] redgifs auth parse error: %s", __FUNCTION__, ex.what( ) );
            return false;
        }
    }

    // Parses a /v2/gifs/<slug> response body and returns the best CDN URL,
    // or empty string on failure / missing fields.
    inline std::string ExtractMediaUrlFromGifResponse( const std::string& body )
    {
        try
        {
            const Json parsed = Json::parse( body );
            if( !parsed.contains( "gif" ) || !parsed[ "gif" ].is_object( ) )
            {
                return { };
            }

            const Json& gif = parsed[ "gif" ];
            if( !gif.contains( "urls" ) )
            {
                return { };
            }

            return PickMediaUrl( gif[ "urls" ] );
        }
        catch( const Json::exception& ex )
        {
            LogError( "[%s] redgifs gif response parse error: %s", __FUNCTION__, ex.what( ) );
            return { };
        }
    }

    // Parses a /v2/users/<name>/search response body. Returns the list of
    // best CDN URLs (one per gif entry that has any usable url), and
    // populates outTotalPages so the caller can stop paginating. Returns
    // empty list on parse failure.
    inline std::vector<std::string> ExtractMediaUrlsFromUserSearchResponse( const std::string& body, int& outTotalPages )
    {
        std::vector<std::string> urls{ };
        outTotalPages = 0;

        try
        {
            const Json parsed = Json::parse( body );

            if( parsed.contains( "pages" ) && parsed[ "pages" ].is_number_integer( ) )
            {
                outTotalPages = parsed[ "pages" ].get<int>( );
            }

            if( !parsed.contains( "gifs" ) || !parsed[ "gifs" ].is_array( ) )
            {
                return urls;
            }

            for( const Json& gif : parsed[ "gifs" ] )
            {
                if( !gif.is_object( ) || !gif.contains( "urls" ) )
                {
                    continue;
                }

                const std::string url = PickMediaUrl( gif[ "urls" ] );
                if( !url.empty( ) )
                {
                    urls.push_back( url );
                }
            }
        }
        catch( const Json::exception& ex )
        {
            LogError( "[%s] redgifs user search parse error: %s", __FUNCTION__, ex.what( ) );
            urls.clear( );
            outTotalPages = 0;
        }

        return urls;
    }
}
