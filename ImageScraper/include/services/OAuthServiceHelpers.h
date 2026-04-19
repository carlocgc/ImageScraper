#pragma once

#include "services/Service.h"

#include <functional>
#include <memory>
#include <string>

namespace ImageScraper
{
    class OAuthClient;

    namespace OAuthServiceHelpers
    {
        bool OpenExternalAuth( OAuthClient& client );

        bool HandleExternalAuth( ContentProvider provider,
                                 const std::string& providerName,
                                 const std::shared_ptr<IServiceSink>& sink,
                                 OAuthClient& client,
                                 const std::string& response,
                                 std::function<void( )> onAuthSuccess );

        void Authenticate( ContentProvider provider,
                           const std::string& providerName,
                           OAuthClient& client,
                           Service::AuthenticateCallback callback,
                           std::function<void( )> onAuthenticated );

        void SignOut( const std::string& providerName,
                      OAuthClient& client,
                      std::function<void( )> onSignedOut );
    }
}
