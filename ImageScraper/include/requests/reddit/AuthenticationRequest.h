#pragma once
#include "requests/Request.h"
#include "requests/RequestTypes.h"

#include <string>

namespace ImageScraper::Reddit
{
    class AuthenticationRequest : public Request
    {
    public:
        RequestResult Perform( const RequestOptions& options ) override;

    private:
        static const std::string s_AuthUrl;
        static const std::string s_AuthData;
    };
}

