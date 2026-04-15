#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace ImageScraper
{
    using json = nlohmann::json;

    class Config final
    {
    public:
        Config( ) = default;
        ~Config( ) = default;

        bool ReadFromFile( const std::string& filepath );

        template<typename T>
        const T GetValue( const std::string& key ) const
        {
            return m_Json[ key ].get<T>( );
        }
        const std::string UserAgent( ) const;
        const std::string CaBundle( ) const;

    private:
        json m_Json{ };
    };

}