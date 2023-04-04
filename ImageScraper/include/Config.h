#pragma once

#include <string>
#include <nlohmann/json.hpp>

class Config final
{
public:
    Config( const std::string filepath )
    {
        ReadFromFile( filepath );
    }

    template<typename T>
    T GetValue( const std::string& key )
    {
        return m_ConfigData[ key ].get<T>( );
    }

private:
    nlohmann::json m_ConfigData{ };

    bool ReadFromFile( const std::string& filepath );
};

