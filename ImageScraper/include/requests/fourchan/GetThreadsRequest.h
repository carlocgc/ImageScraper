#pragma once
#include "requests/Request.h"

namespace ImageScraper::FourChan
{
    class GetThreadsRequest : public Request
    {
    public:
        RequestResult Perform( const RequestOptions& options ) override;
    private:
        static const std::string s_BaseUrl;
    };
}