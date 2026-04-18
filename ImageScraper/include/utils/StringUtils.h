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

        // Extracts the value of a named parameter from an HTTP request line or query string.
        // Searches for "key=" and returns the substring up to the next '&' or ' '.
        // Returns an empty string if the key is not found.
        static std::string ExtractQueryParam( const std::string& str, const std::string& key )
        {
            const std::string search = key + "=";
            std::size_t start = str.find( search );
            if( start == std::string::npos )
            {
                return { };
            }
            start += search.length( );
            const std::size_t ampPos   = str.find( "&", start );
            const std::size_t spacePos = str.find( " ", start );
            const std::size_t end      = ( ampPos < spacePos ) ? ampPos : spacePos;
            if( end == std::string::npos )
            {
                return { };
            }
            return str.substr( start, end - start );
        }

        // Percent-encodes a string for use as a URI query parameter value (RFC 3986).
        // Unreserved characters (alpha, digit, '-', '_', '.', '~') are left as-is;
        // everything else is encoded as %XX.
        static std::string UrlEncode( const std::string& str )
        {
            std::string result;
            result.reserve( str.size( ) * 3 );
            for( unsigned char c : str )
            {
                if( std::isalnum( c ) || c == '-' || c == '_' || c == '.' || c == '~' )
                {
                    result += static_cast<char>( c );
                }
                else
                {
                    char hex[ 4 ];
                    snprintf( hex, sizeof( hex ), "%%%02X", c );
                    result += hex;
                }
            }
            return result;
        }

        // Decodes a percent-encoded URL string. '+' is treated as a space.
        static std::string UrlDecode( const std::string& str )
        {
            std::string result;
            result.reserve( str.size( ) );
            for( std::size_t i = 0; i < str.size( ); ++i )
            {
                if( str[ i ] == '+' )
                {
                    result += ' ';
                }
                else if( str[ i ] == '%' && i + 2 < str.size( ) )
                {
                    const char hexStr[ 3 ] = { str[ i + 1 ], str[ i + 2 ], '\0' };
                    result += static_cast<char>( std::strtol( hexStr, nullptr, 16 ) );
                    i += 2;
                }
                else
                {
                    result += str[ i ];
                }
            }
            return result;
        }

    private:
        StringUtils( ) = default;
    };
}