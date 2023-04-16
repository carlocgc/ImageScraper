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

        std::string contentKey{ };

        if( data.contains( "url" ) )
        {
            contentKey = "url";
        }
        else if( data.contains( "url_overridden_by_dest" ) )
        {
            contentKey = "url_overridden_by_dest";
        }
        else
        {
            return;
        }

        const std::vector<std::string> targetExts{ ".jpg", ".jpeg", ".png", ".webm", ".webp", ".gif", ".gifv", ".mp4" };

        const std::string& url = data[ contentKey ];

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