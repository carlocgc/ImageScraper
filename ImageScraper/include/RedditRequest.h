#pragma once
#include "Request.h"
#include "Config.h"

class RedditRequest final : public Request
{
public:
    RedditRequest( const Config& config, const std::string& endpoint );
    bool Perform( ) override;

protected:
    bool Configure( const Config& config ) override;
};

