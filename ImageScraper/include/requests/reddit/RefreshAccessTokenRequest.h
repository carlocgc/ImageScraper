#pragma once

#include "requests/Request.h"

namespace ImageScraper::Reddit
{
    class RefreshAccessTokenRequest : public Request
    {
    public:
        RequestResult Perform( const RequestOptions& options ) override;

    private:
        static const std::string s_AuthUrl;
        static const std::string s_AuthData;
    };
}
