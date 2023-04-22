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
            if( str.empty( ) )
            {
                return std::wstring( );
            }

            int required_size = MultiByteToWideChar( CP_UTF8, 0, str.c_str( ), -1, NULL, 0 );
            std::wstring wide_str( nullTerminated ? required_size : required_size - 1, L'\0' );
            MultiByteToWideChar( CP_UTF8, 0, str.c_str( ), -1, &wide_str[ 0 ], required_size );
            return wide_str;
        }

        static std::string WideStringToUtf8String( const std::wstring& wstr, bool nullTerminated )
        {
            if( wstr.empty( ) )
            {
                return std::string( );
            }

            int required_size = WideCharToMultiByte( CP_UTF8, 0, wstr.c_str( ), -1, nullptr, 0, nullptr, nullptr );
            std::string utf8_string( nullTerminated ? required_size : required_size - 1, 0 );
            WideCharToMultiByte( CP_UTF8, 0, wstr.c_str( ), -1, &utf8_string[ 0 ], required_size, nullptr, nullptr );
            return utf8_string;
        }

    private:
        StringUtils( ) = default;
    };
}