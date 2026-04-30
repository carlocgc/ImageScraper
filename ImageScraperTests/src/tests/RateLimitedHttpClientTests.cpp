#include "catch2/catch_amalgamated.hpp"
#include "mocks/MockHttpClient.h"

#include "network/RateLimitedHttpClient.h"
#include "network/RateLimitTypes.h"
#include "services/IServiceSink.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using namespace ImageScraper;

namespace
{
    HttpRequest MakeRequest( )
    {
        HttpRequest req{ };
        req.m_Url = "https://example.com/test";
        req.m_UserAgent = "TestAgent/1.0";
        return req;
    }

    HttpResponse MakeSuccess( )
    {
        HttpResponse r{ };
        r.m_Success    = true;
        r.m_StatusCode = 200;
        return r;
    }

    // Minimal sink that records the rate-limit callbacks the limiter fires.
    class RecordingSink : public IServiceSink
    {
    public:
        bool IsCancelled( ) override                                            { return m_Cancelled.load( ); }
        void OnRunComplete( ) override                                          { }
        void OnCurrentDownloadProgress( float ) override                        { }
        void OnTotalDownloadProgress( int, int ) override                       { }
        int  GetSigningInProvider( ) override                                   { return INVALID_CONTENT_PROVIDER; }
        void OnSignInComplete( ContentProvider ) override                       { }
        void OnFileDownloaded( const std::string&, const std::string& ) override { }

        void OnRateLimitWait( const std::string&, const std::string&, int waitSeconds ) override
        {
            m_WaitCalls.fetch_add( 1 );
            m_LastWaitSeconds.store( waitSeconds );
        }

        void OnRateLimitResume( const std::string&, const std::string& ) override
        {
            m_ResumeCalls.fetch_add( 1 );
        }

        std::atomic<bool> m_Cancelled{ false };
        std::atomic<int>  m_WaitCalls{ 0 };
        std::atomic<int>  m_ResumeCalls{ 0 };
        std::atomic<int>  m_LastWaitSeconds{ 0 };
    };
}

TEST_CASE( "RateLimitedHttpClient admits up to N requests and waits for the next slot", "[ratelimit]" )
{
    auto inner = std::make_shared<MockHttpClient>( );
    inner->m_Response = MakeSuccess( );

    RateLimitTable table{ };
    table[ "fast" ] = { { 3, 1 } };  // 3 requests per 1 second sliding window

    auto sink = std::make_shared<RecordingSink>( );

    RateLimitedHttpClient client(
        inner,
        table,
        [ sink ]( ) { return sink->IsCancelled( ); },
        sink,
        "TestService" );

    HttpRequest req = MakeRequest( );

    const auto start = std::chrono::steady_clock::now( );
    REQUIRE( client.Get( req, "fast" ).m_Success );
    REQUIRE( client.Get( req, "fast" ).m_Success );
    REQUIRE( client.Get( req, "fast" ).m_Success );

    // The 4th call must block until at least ~1s after the first one.
    REQUIRE( client.Get( req, "fast" ).m_Success );
    const auto elapsed = std::chrono::steady_clock::now( ) - start;
    REQUIRE( elapsed >= std::chrono::milliseconds( 900 ) );

    REQUIRE( inner->m_CallCount == 4 );
    REQUIRE( inner->m_LastRateLimitKey == "fast" );
}

TEST_CASE( "RateLimitedHttpClient with no spec under any key passes through unrestricted", "[ratelimit]" )
{
    auto inner = std::make_shared<MockHttpClient>( );
    inner->m_Response = MakeSuccess( );

    RateLimitTable empty{ };  // no entries at all - not even __default__

    auto sink = std::make_shared<RecordingSink>( );

    RateLimitedHttpClient client(
        inner,
        empty,
        [ sink ]( ) { return sink->IsCancelled( ); },
        sink,
        "TestService" );

    HttpRequest req = MakeRequest( );

    const auto start = std::chrono::steady_clock::now( );
    for( int i = 0; i < 20; ++i )
    {
        REQUIRE( client.Get( req, "anything" ).m_Success );
    }
    const auto elapsed = std::chrono::steady_clock::now( ) - start;
    REQUIRE( elapsed < std::chrono::milliseconds( 500 ) );
    REQUIRE( inner->m_CallCount == 20 );
}

TEST_CASE( "RateLimitedHttpClient empty key falls back to __default__ bucket", "[ratelimit]" )
{
    auto inner = std::make_shared<MockHttpClient>( );
    inner->m_Response = MakeSuccess( );

    RateLimitTable table{ };
    table[ s_DefaultRateLimitKey ] = { { 2, 1 } };

    auto sink = std::make_shared<RecordingSink>( );

    RateLimitedHttpClient client(
        inner,
        table,
        [ sink ]( ) { return sink->IsCancelled( ); },
        sink,
        "TestService" );

    HttpRequest req = MakeRequest( );

    const auto start = std::chrono::steady_clock::now( );
    REQUIRE( client.Get( req, "" ).m_Success );
    REQUIRE( client.Get( req, "" ).m_Success );
    // Third call must wait for the window to expire.
    REQUIRE( client.Get( req, "" ).m_Success );
    const auto elapsed = std::chrono::steady_clock::now( ) - start;
    REQUIRE( elapsed >= std::chrono::milliseconds( 900 ) );
}

TEST_CASE( "RateLimitedHttpClient unknown key falls back to __default__ when present", "[ratelimit]" )
{
    auto inner = std::make_shared<MockHttpClient>( );
    inner->m_Response = MakeSuccess( );

    RateLimitTable table{ };
    table[ s_DefaultRateLimitKey ] = { { 1, 1 } };

    auto sink = std::make_shared<RecordingSink>( );

    RateLimitedHttpClient client(
        inner,
        table,
        [ sink ]( ) { return sink->IsCancelled( ); },
        sink,
        "TestService" );

    HttpRequest req = MakeRequest( );
    REQUIRE( client.Get( req, "unregistered_key" ).m_Success );
    const auto start = std::chrono::steady_clock::now( );
    REQUIRE( client.Get( req, "unregistered_key" ).m_Success );
    const auto elapsed = std::chrono::steady_clock::now( ) - start;
    REQUIRE( elapsed >= std::chrono::milliseconds( 900 ) );
}

TEST_CASE( "RateLimitedHttpClient returns synthetic failure on cancellation", "[ratelimit]" )
{
    auto inner = std::make_shared<MockHttpClient>( );
    inner->m_Response = MakeSuccess( );

    RateLimitTable table{ };
    // Effectively zero-throughput: 1 request per 60 seconds.
    table[ "tight" ] = { { 1, 60 } };

    auto sink = std::make_shared<RecordingSink>( );

    RateLimitedHttpClient client(
        inner,
        table,
        [ sink ]( ) { return sink->IsCancelled( ); },
        sink,
        "TestService" );

    HttpRequest req = MakeRequest( );
    // First call admits immediately.
    REQUIRE( client.Get( req, "tight" ).m_Success );

    // Second call would have to wait ~60s. Cancel mid-wait.
    std::thread cancellor( [ & ]( )
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
            sink->m_Cancelled.store( true );
        } );

    const auto start = std::chrono::steady_clock::now( );
    HttpResponse response = client.Get( req, "tight" );
    const auto elapsed = std::chrono::steady_clock::now( ) - start;

    cancellor.join( );

    REQUIRE_FALSE( response.m_Success );
    REQUIRE( response.m_StatusCode == -1 );
    REQUIRE( response.m_Error == "cancelled" );
    REQUIRE( elapsed < std::chrono::milliseconds( 1500 ) );
    // Inner client should NOT have been called for the cancelled request.
    REQUIRE( inner->m_CallCount == 1 );
}

TEST_CASE( "RateLimitedHttpClient enforces stacked windows independently", "[ratelimit]" )
{
    auto inner = std::make_shared<MockHttpClient>( );
    inner->m_Response = MakeSuccess( );

    RateLimitTable table{ };
    // Two stacked windows: 5/sec AND 8/3sec.
    //   - The 5/sec rule is the binding one for calls 6-8 (the original burst ages out together).
    //   - The 8/3sec rule must bind on call 9: at that point the 5/sec window has only 3
    //     entries (room to spare), but the 8/3sec window is full and its oldest entry won't
    //     age out until ~3s after the burst started.
    table[ "stacked" ] = { { 5, 1 }, { 8, 3 } };

    auto sink = std::make_shared<RecordingSink>( );

    RateLimitedHttpClient client(
        inner,
        table,
        [ sink ]( ) { return sink->IsCancelled( ); },
        sink,
        "TestService" );

    HttpRequest req = MakeRequest( );
    const auto start = std::chrono::steady_clock::now( );

    // First 5 should fly through (5/sec window has room).
    for( int i = 0; i < 5; ++i )
    {
        REQUIRE( client.Get( req, "stacked" ).m_Success );
    }
    const auto afterFirst5 = std::chrono::steady_clock::now( ) - start;
    REQUIRE( afterFirst5 < std::chrono::milliseconds( 500 ) );

    // Calls 6, 7, 8 must wait for the 5/sec window to roll. After call 6 admits, calls 7 and 8
    // follow quickly because the entire original burst ages out of the 5/sec window together.
    for( int i = 0; i < 3; ++i )
    {
        REQUIRE( client.Get( req, "stacked" ).m_Success );
    }
    const auto afterFirst8 = std::chrono::steady_clock::now( ) - start;
    REQUIRE( afterFirst8 >= std::chrono::milliseconds( 900 ) );
    REQUIRE( afterFirst8 < std::chrono::milliseconds( 2500 ) );

    // Call 9 is the test of the 8/3sec window: the 5/sec window has 3 entries (room for 2),
    // but the 8/3sec window is full. Its oldest entry was admitted near t=0, so call 9 cannot
    // admit until t ~= 3s. If only the 5/sec rule were enforced, call 9 would admit at t ~= 1s.
    REQUIRE( client.Get( req, "stacked" ).m_Success );
    const auto afterFirst9 = std::chrono::steady_clock::now( ) - start;
    REQUIRE( afterFirst9 >= std::chrono::milliseconds( 2800 ) );

    REQUIRE( inner->m_CallCount == 9 );
}

TEST_CASE( "RateLimitedHttpClient fires sink callback when wait exceeds threshold", "[ratelimit]" )
{
    auto inner = std::make_shared<MockHttpClient>( );
    inner->m_Response = MakeSuccess( );

    RateLimitTable table{ };
    // Force a >2s wait: 1 per 3 seconds.
    table[ "slow" ] = { { 1, 3 } };

    auto sink = std::make_shared<RecordingSink>( );

    RateLimitedHttpClient client(
        inner,
        table,
        [ sink ]( ) { return sink->IsCancelled( ); },
        sink,
        "TestService" );

    HttpRequest req = MakeRequest( );
    REQUIRE( client.Get( req, "slow" ).m_Success );
    REQUIRE( sink->m_WaitCalls.load( ) == 0 );

    REQUIRE( client.Get( req, "slow" ).m_Success );

    REQUIRE( sink->m_WaitCalls.load( ) == 1 );
    REQUIRE( sink->m_ResumeCalls.load( ) == 1 );
    REQUIRE( sink->m_LastWaitSeconds.load( ) >= 2 );
}
