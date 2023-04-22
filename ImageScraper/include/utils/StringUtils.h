#pragma once

#include <string>
#include <random>

#include <Windows.h>

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

        static std::wstring Utf8ToWideString( const std::string& str, bool nullTerminated )
        {
            int required_size = MultiByteToWideChar( CP_UTF8, 0, str.c_str( ), -1, NULL, 0 );
            std::wstring wide_str( nullTerminated ? required_size : required_size - 1, L'\0' );
            MultiByteToWideChar( CP_UTF8, 0, str.c_str( ), -1, &wide_str[ 0 ], required_size );
            return wide_str;
        }

    private:
        StringUtils( ) = default;
    };
}