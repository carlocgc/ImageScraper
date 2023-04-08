#pragma once
#include <string>

class Service
{
public:
    virtual ~Service( ) { };
    virtual bool HandleUrl( const Config& config, const std::string& url ) = 0;
};