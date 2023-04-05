#pragma once

#include <string>
#include <nlohmann/json.hpp>

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
    nlohmann::json m_ConfigData{ };

    bool ReadFromFile( const std::string& filepath );
};

