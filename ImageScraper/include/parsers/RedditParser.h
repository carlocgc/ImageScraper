#pragma once
#include <vector>
#include <string>
#include "nlohmann/json.hpp"

using namespace nlohmann;

namespace RedditParser
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

        const std::string& url = data[ "url" ];
        if( url.find( ".jpg" ) == std::string::npos && url.find( ".png" ) == std::string::npos )
        {
            return false;
        }

        out.push_back( url );
        return true;
    }
};