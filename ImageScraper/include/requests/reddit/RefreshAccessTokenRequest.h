#pragma once

#include "requests/Request.h"
#include "network/IHttpClient.h"

#include <memory>

namespace ImageScraper::Reddit
{
    class RefreshAccessTokenRequest : public Request
    {
    public:
        RefreshAccessTokenRequest( );
        RefreshAccessTokenRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_AuthUrl;
        static const std::string s_AuthData;
    };
}
