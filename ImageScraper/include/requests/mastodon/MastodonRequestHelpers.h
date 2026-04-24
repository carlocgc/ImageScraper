#pragma once

#include "network/HttpClientTypes.h"
#include "requests/RequestTypes.h"

#include <string>
#include <vector>

namespace ImageScraper::Mastodon::RequestHelpers
{
    bool HasQueryParam( const std::vector<QueryParam>& params, const std::string& key );
    std::string CreateEncodedQueryParamString( const std::vector<QueryParam>& params );
    RequestResult MakeBadRequest( const std::string& message );
    RequestResult CreateResultFromResponse( const HttpResponse& response );
    void ApplyBearerAuthHeader( HttpRequest& request, const std::string& accessToken );
}
