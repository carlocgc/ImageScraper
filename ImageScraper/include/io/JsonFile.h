#pragma once

#include "nlohmann/json.hpp"

#include <string>
#include <string_view>

namespace ImageScraper
{
    using Json = nlohmann::json;

    class JsonFile
    {
    public:
        JsonFile( const std::string& filepath ) : m_FilePath{ filepath } { };
        bool Deserialise( );
        bool Serialise( );

        template<typename T>
        const bool GetValue( const std::string& key, T& outValue ) const
        {
            if( !m_Json.contains( key ) )
            {
                return false;
            }

            outValue = m_Json[ key ].get<T>( );
            return true;
        }

        template<typename T>
        void SetValue( const std::string& key, const T& value )
        {
            m_Json[ key ] = value;
        }

        const Json& GetJson( ) const { return m_Json; };
        const std::string& GetFilePath( ) const { return m_FilePath; }


    private:
        std::string m_FilePath;
        Json m_Json;
    };
}

