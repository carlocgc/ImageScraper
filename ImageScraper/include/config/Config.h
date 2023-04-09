#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace ImageScraper
{
    class Config final
    {
    public:
        Config( );

        template<typename T>
        const T GetValue( const std::string& key ) const
        {
            return m_ConfigData[ key ].get<T>( );
        }

        const std::string UserAgent( ) const;
        const std::string CaBundle( ) const;

    private:
        bool ReadFromFile( const std::string& filepath );

        nlohmann::json m_ConfigData{ };
    };

}