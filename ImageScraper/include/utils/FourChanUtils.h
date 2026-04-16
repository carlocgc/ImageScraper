#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>

namespace ImageScraper::FourChanUtils
{
    using Json = nlohmann::json;

    inline int GetPageCountForBoard( const std::string& boardId, const Json& response )
    {
        int pages = 0;

        if( !response.contains( "boards" ) )
        {
            return pages;
        }

        try
        {
            for( const auto& board : response[ "boards" ] )
            {
                if( !board.contains( "board" ) || board[ "board" ] != boardId )
                {
                    continue;
                }

                if( board.contains( "pages" ) )
                {
                    pages = board[ "pages" ];
                    break;
                }
            }
        }
        catch( const Json::exception& e )
        {
            LogError( "[%s] GetPageCountForBoard error parsing response, error: %s", __FUNCTION__, e.what( ) );
        }

        return pages;
    }

    inline std::vector<std::string> GetFileNamesFromResponse( const Json& response )
    {
        std::vector<std::string> filenames{ };

        if( !response.contains( "threads" ) )
        {
            return filenames;
        }

        try
        {
            for( const auto& thread : response[ "threads" ] )
            {
                if( !thread.contains( "posts" ) )
                {
                    continue;
                }

                for( const auto& post : thread[ "posts" ] )
                {
                    if( !post.contains( "tim" ) || !post.contains( "ext" ) )
                    {
                        continue;
                    }

                    const uint64_t time = post[ "tim" ].get<uint64_t>( );
                    const std::string ext = post[ "ext" ].get<std::string>( );

                    filenames.push_back( std::to_string( time ) + ext );
                }
            }
        }
        catch( const Json::exception& e )
        {
            LogError( "[%s] GetFileNamesFromResponse error parsing response, error: %s", __FUNCTION__, e.what( ) );
        }

        return filenames;
    }
}
