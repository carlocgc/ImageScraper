#pragma once

#include <vector>
#include <string>

using namespace nlohmann;

namespace ImageScraper::Reddit
{
    static void TryGetContentUrl( const json& post, std::vector<std::string>& out )
    {
        if( !post.contains( "data" ) )
        {
            return;
        }

        const json& data = post[ "data" ];
        if( !data.contains( "url" ) )
        {
            return;
        }

        const std::vector<std::string> targetExts{ ".jpg", ".jpeg", ".png", ".webm", ".webp", ".gif", ".gifv", ".mp4" };

        const std::string& url = data[ "url" ];

        for( const auto& ext : targetExts )
        {
            const auto& pos = url.find( ext );
            if( pos != std::string::npos )
            {
                out.push_back( url );
                return;
            }
        }
    }
};