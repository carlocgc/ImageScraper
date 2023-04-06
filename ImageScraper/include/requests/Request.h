#pragma once
#include "Config.h"
#include <curlpp/Easy.hpp>
#include <curlpp/cURLpp.hpp>
#include <sstream>
#include <string>
#include "RequestTypes.h"

class Request
{
public:
    virtual ~Request( ) { };

    virtual RequestResult Perform( const RequestOptions& options ) = 0;
};

