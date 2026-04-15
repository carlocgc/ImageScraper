#pragma once

#include <string>
#include <windows.h>
#include <Wincrypt.h>

#include "log/Logger.h"

namespace ImageScraper
{
    class EncryptionUtils
    {
    public:
        static std::string EncryptData( const std::string& data )
        {
            DATA_BLOB input;
            DATA_BLOB output;
            input.pbData = reinterpret_cast< BYTE* >( const_cast< char* >( data.c_str( ) ) );
            input.cbData = static_cast< DWORD >( data.length( ) );

            if( CryptProtectData( &input, NULL, NULL, NULL, NULL, 0, &output ) )
            {
                std::string encryptedData( reinterpret_cast< char* >( output.pbData ), output.cbData );
                LocalFree( output.pbData );

                InfoLog( "[%s] Data encrypted successfully!", __FUNCTION__ );
                return encryptedData;
            }

            ErrorLog( "[%s] Failed to encrypt data!", __FUNCTION__ );
            return "";
        }

        static std::string DecryptData( const std::string& data )
        {
            DATA_BLOB input;
            DATA_BLOB output;
            input.pbData = reinterpret_cast< BYTE* >( const_cast< char* >( data.c_str( ) ) );
            input.cbData = static_cast< DWORD >( data.length( ) );

            if( CryptUnprotectData( &input, NULL, NULL, NULL, NULL, 0, &output ) )
            {
                std::string decryptedData( reinterpret_cast< char* >( output.pbData ), output.cbData );
                LocalFree( output.pbData );

                InfoLog( "[%s] Data decrypted successfully!", __FUNCTION__ );
                return decryptedData;
            }

            ErrorLog( "[%s] Failed to decrypt data!", __FUNCTION__ );
            return "";
        }

    private:
        EncryptionUtils( ) = delete;
    };
}