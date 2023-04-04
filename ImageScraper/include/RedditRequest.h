#pragma once
#include "Request.h"
#include "Config.h"

class RedditRequest : public Request
{
public:
	RedditRequest( const Config& config, const std::string& endpoint );
	void Perform( ) override;
	void Configure( const Config& config ) override;
};

