#pragma once
// NOTE: This request class is not currently called from RedditService::SignOut().
// Reddit's token revocation endpoint (/api/v1/revoke_token) imposes a server-side
// propagation delay of several minutes after revocation, during which re-authentication
// hangs — the OAuth page opens but never redirects. Sign-out is therefore local-only:
// tokens are cleared in memory and on disk without notifying Reddit. The class is
// retained in case this decision is revisited (e.g. if Reddit fixes the propagation
// delay, or for a future explicit "deauthorise all sessions" feature).
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
