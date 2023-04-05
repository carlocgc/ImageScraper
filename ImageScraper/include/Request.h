#pragma once
#include "Config.h"
#include <curlpp/Easy.hpp>
#include <curlpp/cURLpp.hpp>
#include <sstream>
#include <string>

class Request
{
public:
    Request( const Config& config, const std::string& endpoint )
        : m_UserAgent{ config.UserAgent() }
        , m_CaBundle{ config.CaBundle() }
        , m_EndPoint{ endpoint }
    {
    }
    virtual ~Request( ) { };
    virtual bool Perform( ) = 0;

    // Create a response obj and have perform return it?
    std::string Response( ) { return m_Response.str( ); };

protected:
    virtual bool Configure( const Config& config ) = 0;

    curlpp::Cleanup m_Cleanup;
    curlpp::Easy m_Easy;
    std::ostringstream m_Response;
    std::string m_UserAgent;
    std::string m_CaBundle;
    std::string m_EndPoint;
};

