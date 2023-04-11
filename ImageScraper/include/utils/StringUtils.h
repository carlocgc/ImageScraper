#pragma once

#include <string>
#include <random>

namespace ImageScraper
{
    class StringUtils
    {
    public:
        static std::string CreateGuid( int length )
        {
            const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            std::random_device rand;
            std::mt19937 gen( rand( ) );
            std::uniform_int_distribution<int> dist( 0, static_cast< int >( charset.size( ) ) - 1 );

            std::string guid( length, ' ' );
            for( int i = 0; i < length; ++i )
            {
                guid[ i ] = charset[ dist( gen ) ];
            }

            return guid;
        }

    private:
        StringUtils( ) = default;
    };
}