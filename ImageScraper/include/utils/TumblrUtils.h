#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>

namespace ImageScraper::TumblrUtils
{
    using Json = nlohmann::json;

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
        }

        return mediaUrls;
    }
}
