#pragma once
#include "requests/RequestTypes.h"
#include "config/Config.h"
#include <curlpp/Easy.hpp>
#include <curlpp/cURLpp.hpp>
#include <sstream>
#include <string>

namespace ImageScraper
{
    class Request
    {
    public:
        virtual ~Request( ) { };
        virtual RequestResult Perform( const RequestOptions& options ) = 0;
    };
}
