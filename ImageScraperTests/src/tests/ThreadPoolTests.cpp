#include "catch2/catch_amalgamated.hpp"
#include "async/ThreadPool.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

using namespace ImageScraper;

// ─── Start / Stop lifecycle ───────────────────────────────────────────────────

TEST_CASE( "ThreadPool::Start - tasks complete on all started contexts", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 3 );

    std::atomic<int> count{ 0 };
    auto f0 = pool.Submit( 0, [ & ]( ) { count++; } );
    auto f1 = pool.Submit( 1, [ & ]( ) { count++; } );
    auto f2 = pool.Submit( 2, [ & ]( ) { count++; } );

    f0.get( ); f1.get( ); f2.get( );
    REQUIRE( count == 3 );
}

TEST_CASE( "ThreadPool::Start - calling Start again while running is a no-op", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 2 );
    pool.Start( 2 );  // second call must not spawn extra threads or corrupt state

    std::atomic<int> count{ 0 };
    auto f0 = pool.Submit( 0, [ & ]( ) { count++; } );
    auto f1 = pool.Submit( 1, [ & ]( ) { count++; } );

    f0.get( ); f1.get( );
    REQUIRE( count == 2 );
}

TEST_CASE( "ThreadPool::Stop - pool can be restarted cleanly after Stop", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    std::atomic<int> count{ 0 };
    auto f1 = pool.Submit( 0, [ & ]( ) { count++; } );
    f1.get( );
    REQUIRE( count == 1 );

    pool.Stop( );
    pool.Start( 1 );

    auto f2 = pool.Submit( 0, [ & ]( ) { count++; } );
    f2.get( );
    REQUIRE( count == 2 );
}

TEST_CASE( "ThreadPool destructor - completes without hanging", "[ThreadPool]" )
{
    {
        ThreadPool pool;
        pool.Start( 2 );
        auto f0 = pool.Submit( 0, [ ]( ) { } );
        auto f1 = pool.Submit( 1, [ ]( ) { } );
        f0.get( ); f1.get( );
    }  // destructor called here - test hangs if it deadlocks
    SUCCEED( );
}

// ─── Submit (worker thread) ───────────────────────────────────────────────────

TEST_CASE( "ThreadPool::Submit - future resolves to the correct return value", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    auto future = pool.Submit( 0, [ ]( ) { return 42; } );
    REQUIRE( future.get( ) == 42 );
}

TEST_CASE( "ThreadPool::Submit - void-returning task completes without error", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    std::atomic<bool> ran{ false };
    auto future = pool.Submit( 0, [ & ]( ) { ran = true; } );
    future.get( );
    REQUIRE( ran );
}

TEST_CASE( "ThreadPool::Submit - tasks on the same context execute in FIFO order", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    std::vector<int> order;
    std::mutex orderMutex;

    auto f1 = pool.Submit( 0, [ & ]( ) { std::lock_guard<std::mutex> lk( orderMutex ); order.push_back( 1 ); } );
    auto f2 = pool.Submit( 0, [ & ]( ) { std::lock_guard<std::mutex> lk( orderMutex ); order.push_back( 2 ); } );
    auto f3 = pool.Submit( 0, [ & ]( ) { std::lock_guard<std::mutex> lk( orderMutex ); order.push_back( 3 ); } );

    f1.get( ); f2.get( ); f3.get( );
    REQUIRE( order == std::vector<int>{ 1, 2, 3 } );
}

TEST_CASE( "ThreadPool::Submit - tasks on different contexts execute concurrently", "[ThreadPool]" )
{
    // Context 1 sets a flag that unblocks context 0. If both ran on the same
    // thread this would deadlock, proving they must run in parallel.
    ThreadPool pool;
    pool.Start( 2 );

    std::atomic<bool> signal{ false };

    auto f0 = pool.Submit( 0, [ & ]( )
    {
        while( !signal.load( ) ) { std::this_thread::yield( ); }
    } );

    auto f1 = pool.Submit( 1, [ & ]( )
    {
        signal = true;
    } );

    f1.get( );
    f0.get( );
    REQUIRE( signal );
}

// ─── SubmitMain (main-thread queue) ──────────────────────────────────────────

TEST_CASE( "ThreadPool::SubmitMain - task does not execute until Update() is called", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    bool ran = false;
    pool.SubmitMain( [ & ]( ) { ran = true; } );
    REQUIRE( !ran );

    pool.Update( );
    REQUIRE( ran );
}

TEST_CASE( "ThreadPool::SubmitMain - future resolves after Update() is called", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    auto future = pool.SubmitMain( [ ]( ) { return 99; } );
    pool.Update( );
    REQUIRE( future.get( ) == 99 );
}

TEST_CASE( "ThreadPool::SubmitMain - multiple tasks drain in a single Update() call in FIFO order", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    std::vector<int> order;
    pool.SubmitMain( [ & ]( ) { order.push_back( 1 ); } );
    pool.SubmitMain( [ & ]( ) { order.push_back( 2 ); } );
    pool.SubmitMain( [ & ]( ) { order.push_back( 3 ); } );

    REQUIRE( order.empty( ) );

    pool.Update( );
    REQUIRE( order == std::vector<int>{ 1, 2, 3 } );
}

TEST_CASE( "ThreadPool::SubmitMain - tasks queued by a main-thread callback also drain in the same Update() call", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    std::vector<int> order;
    pool.SubmitMain( [ & ]( )
    {
        order.push_back( 1 );
        pool.SubmitMain( [ & ]( ) { order.push_back( 2 ); } );
    } );

    pool.Update( );
    REQUIRE( order == std::vector<int>{ 1, 2 } );
}

// ─── Update ──────────────────────────────────────────────────────────────────

TEST_CASE( "ThreadPool::Update - no-op on an empty main queue", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );
    REQUIRE_NOTHROW( pool.Update( ) );
}

TEST_CASE( "ThreadPool::Update - no-op after Stop", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );
    bool ran = false;
    pool.SubmitMain( [ & ]( ) { ran = true; } );
    pool.Stop( );
    REQUIRE_NOTHROW( pool.Update( ) );
    REQUIRE( !ran );
}

// ─── Stop edge cases ─────────────────────────────────────────────────────────

TEST_CASE( "ThreadPool::Stop - no-op when pool has not been started", "[ThreadPool]" )
{
    ThreadPool pool;
    REQUIRE_NOTHROW( pool.Stop( ) );
}

TEST_CASE( "ThreadPool::Stop - waits for in-flight task to complete before returning", "[ThreadPool]" )
{
    ThreadPool pool;
    pool.Start( 1 );

    std::atomic<bool> taskStarted{ false };
    std::atomic<bool> taskComplete{ false };

    pool.Submit( 0, [ & ]( )
    {
        taskStarted = true;
        std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
        taskComplete = true;
    } );

    while( !taskStarted ) { std::this_thread::yield( ); }
    pool.Stop( );

    REQUIRE( taskComplete );
}

TEST_CASE( "ThreadPool::Stop - queued tasks not yet started are dropped", "[ThreadPool]" )
{
    // Documented behaviour: once m_Stopping is set the worker exits after its
    // current task and does not drain remaining queued tasks. The exact number
    // dropped is scheduling-dependent; this test verifies Stop() completes
    // without hanging and that the queued tasks do not all run.
    ThreadPool pool;
    pool.Start( 1 );

    std::atomic<bool> workerBlocked{ false };
    std::atomic<bool> releaseWorker{ false };
    std::atomic<int>  extraTasksRan{ 0 };

    // Occupy the single worker thread
    pool.Submit( 0, [ & ]( )
    {
        workerBlocked = true;
        while( !releaseWorker ) { std::this_thread::yield( ); }
    } );

    while( !workerBlocked ) { std::this_thread::yield( ); }

    // Queue tasks while the worker is occupied - these should be dropped
    pool.Submit( 0, [ & ]( ) { extraTasksRan++; } );
    pool.Submit( 0, [ & ]( ) { extraTasksRan++; } );
    pool.Submit( 0, [ & ]( ) { extraTasksRan++; } );

    // Release the blocker then stop - m_Stopping races with the worker picking
    // up the next task, so some tasks may slip through, but not all three
    releaseWorker = true;
    pool.Stop( );

    REQUIRE( extraTasksRan < 3 );
}
