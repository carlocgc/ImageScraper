#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>
#include <optional>

namespace ImageScraper::RedditUtils
{
    using Json = nlohmann::json;

    struct AccessTokenData
    {
        std::string m_Token{ };
        int m_ExpireSeconds{ 0 };
    };

    struct MediaUrlsData
    {
        std::vector<std::string> m_Urls{ };
        std::string m_AfterParam{ };
    };

    inline std::optional<AccessTokenData> ParseAccessToken( const Json& response )
    {
        if( !response.contains( "access_token" ) )
        {
            ErrorLog( "[%s] Response did not contain access token!", __FUNCTION__ );
            return std::nullopt;
        }

        if( !response.contains( "expires_in" ) )
        {
            ErrorLog( "[%s] Response did not contain token expire seconds!", __FUNCTION__ );
            return std::nullopt;
        }

        try
        {
            AccessTokenData data;
            data.m_Token = response[ "access_token" ];
            data.m_ExpireSeconds = response[ "expires_in" ];
            return data;
        }
        catch( const Json::exception& ex )
        {
            ErrorLog( "[%s] Could not parse access token response, error: %s", __FUNCTION__, ex.what( ) );
            return std::nullopt;
        }
    }

    inline std::optional<std::string> ParseRefreshToken( const Json& response )
    {
        if( !response.contains( "refresh_token" ) )
        {
            ErrorLog( "[%s] Response did not contain refresh token!", __FUNCTION__ );
            return std::nullopt;
        }

        try
        {
            std::string token = response[ "refresh_token" ];
            return token;
        }
        catch( const Json::exception& ex )
        {
            ErrorLog( "[%s] Could not parse refresh token response, error: %s", __FUNCTION__, ex.what( ) );
            return std::nullopt;
        }
    }

    inline MediaUrlsData GetMediaUrls( const Json& response )
    {
        MediaUrlsData result;

        if( !response.contains( "data" ) )
        {
            return result;
        }

        try
        {
            const Json& data = response[ "data" ];

            if( !data.contains( "children" ) )
            {
                return result;
            }

            const std::vector<Json>& children = data[ "children" ];

            const std::vector<std::string> targetExts{ ".jpg", ".jpeg", ".png", ".webm", ".webp", ".gif", ".gifv", ".mp4" };

            for( const auto& post : children )
            {
                const Json& postData = post[ "data" ];

                std::string contentKey{ };

                if( postData.contains( "url" ) )
                {
                    contentKey = "url";
                }
                else if( postData.contains( "url_overridden_by_dest" ) )
                {
                    contentKey = "url_overridden_by_dest";
                }
                else
                {
                    // Preserve original behaviour: bail on first post with no URL key
                    return result;
                }

                const std::string& url = postData[ contentKey ];

                for( const auto& ext : targetExts )
                {
                    if( url.find( ext ) != std::string::npos )
                    {
                        result.m_Urls.push_back( url );
                        break;
                    }
                }
            }

            if( data.contains( "after" ) && !data[ "after" ].is_null( ) )
            {
                result.m_AfterParam = data[ "after" ];
            }
        }
        catch( const Json::exception& e )
        {
            ErrorLog( "[%s] GetMediaUrls error parsing data, error: %s", __FUNCTION__, e.what( ) );
        }

        return result;
    }
}
