#pragma once
#include "requests/Request.h"
#include "requests/RequestTypes.h"
#include "network/IHttpClient.h"

#include <memory>
#include <string>

namespace ImageScraper::Reddit
{
    class RevokeAccessTokenRequest : public Request
    {
    public:
        RevokeAccessTokenRequest( );
        RevokeAccessTokenRequest( std::shared_ptr<IHttpClient> client );

        RequestResult Perform( const RequestOptions& options ) override;

    private:
        std::shared_ptr<IHttpClient> m_HttpClient{ };
        static const std::string s_RevokeUrl;
    };
}
