#pragma once
#include <vector>
#include <string>
#include "nlohmann/json.hpp"

using namespace nlohmann;

namespace ImageScraper::RedditParser
{
    static bool GetImageUrlFromRedditPost( const json& post, std::vector<std::string>& out )
    {
        if( !post.contains( "data" ) )
        {
            return false;
        }

        const json& data = post[ "data" ];
        if( !data.contains( "url" ) )
        {
            return false;
        }

        const std::vector<std::string> targetExts{ ".jpg", ".jpeg", ".png", ".webm", ".webp", ".gif", ".gifv", ".mp4" };

        const std::string& url = data[ "url" ];

        for( const auto& ext : targetExts )
        {
            const auto& pos = url.find( ext );
            if( pos != std::string::npos )
            {
                out.push_back( url );
                return true;
            }
        }

        return false;
    }

    static std::string GetAccessTokenFromResponse( const json& response )
    {
        if( !response.contains( "access_token" ) )
        {
            return "";
        }

        return response[ "access_token" ];;
    }
};