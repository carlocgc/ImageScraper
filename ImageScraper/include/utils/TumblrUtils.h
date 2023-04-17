#pragma once

#include "nlohmann/json.hpp"

#include <vector>
#include <string>

namespace ImageScraper::Tumblr
{
    using Json = nlohmann::json;

    static std::vector<std::string> GetMediaUrlsFromResponse( const Json& response )
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
                    }
                }
            }
            else if( post[ "type" ] == "video" )
            {
                if( post.contains( "video_url" ) )
                {
                    mediaUrls.push_back( post[ "video_url" ] );
                }
            }
        }

        return mediaUrls;
    }
}