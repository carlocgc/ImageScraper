#include "CppUnitTest.h"
#include "async/ThreadPool.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    // ─── Start / Stop lifecycle ───────────────────────────────────────────────────


    TEST_CLASS(ThreadPoolTests)
    {
    public:
    TEST_METHOD(ThreadPool_Start_Tasks_Complete_On_All_Started_Contexts)
    {
        ThreadPool pool;
        pool.Start( 3 );
    
        std::atomic<int> count{ 0 };
        auto f0 = pool.Submit( 0, [ & ]( ) { count++; } );
        auto f1 = pool.Submit( 1, [ & ]( ) { count++; } );
        auto f2 = pool.Submit( 2, [ & ]( ) { count++; } );
    
        f0.get( ); f1.get( ); f2.get( );
        Assert::IsTrue(  count == 3 );
    }
    
    TEST_METHOD(ThreadPool_Start_Calling_Start_Again_While_Running_Is_A_No_Op)
    {
        ThreadPool pool;
        pool.Start( 2 );
        pool.Start( 2 );  // second call must not spawn extra threads or corrupt state
    
        std::atomic<int> count{ 0 };
        auto f0 = pool.Submit( 0, [ & ]( ) { count++; } );
        auto f1 = pool.Submit( 1, [ & ]( ) { count++; } );
    
        f0.get( ); f1.get( );
        Assert::IsTrue(  count == 2 );
    }
    
    TEST_METHOD(ThreadPool_Stop_Pool_Can_Be_Restarted_Cleanly_After_Stop)
    {
        ThreadPool pool;
        pool.Start( 1 );
    
        std::atomic<int> count{ 0 };
        auto f1 = pool.Submit( 0, [ & ]( ) { count++; } );
        f1.get( );
        Assert::IsTrue(  count == 1 );
    
        pool.Stop( );
        pool.Start( 1 );
    
        auto f2 = pool.Submit( 0, [ & ]( ) { count++; } );
        f2.get( );
        Assert::IsTrue(  count == 2 );
    }
    
    TEST_METHOD(ThreadPool_Destructor_Completes_Without_Hanging)
    {
        {
            ThreadPool pool;
            pool.Start( 2 );
            auto f0 = pool.Submit( 0, [ ]( ) { } );
            auto f1 = pool.Submit( 1, [ ]( ) { } );
            f0.get( ); f1.get( );
        }  // destructor called here - test hangs if it deadlocks
        Assert::IsTrue( true );
    }
    
    // ─── Submit (worker thread) ───────────────────────────────────────────────────
    
    TEST_METHOD(ThreadPool_Submit_Future_Resolves_To_The_Correct_Return_Value)
    {
        ThreadPool pool;
        pool.Start( 1 );
    
        auto future = pool.Submit( 0, [ ]( ) { return 42; } );
        Assert::IsTrue(  future.get( ) == 42 );
    }
    
    TEST_METHOD(ThreadPool_Submit_Void_Returning_Task_Completes_Without_Error)
    {
        ThreadPool pool;
        pool.Start( 1 );
    
        std::atomic<bool> ran{ false };
        auto future = pool.Submit( 0, [ & ]( ) { ran = true; } );
        future.get( );
        Assert::IsTrue(  ran );
    }
    
    TEST_METHOD(ThreadPool_Submit_Tasks_On_The_Same_Context_Execute_In_FIFO_Order)
    {
        ThreadPool pool;
        pool.Start( 1 );
    
        std::vector<int> order;
        std::mutex orderMutex;
    
        auto f1 = pool.Submit( 0, [ & ]( ) { std::lock_guard<std::mutex> lk( orderMutex ); order.push_back( 1 ); } );
        auto f2 = pool.Submit( 0, [ & ]( ) { std::lock_guard<std::mutex> lk( orderMutex ); order.push_back( 2 ); } );
        auto f3 = pool.Submit( 0, [ & ]( ) { std::lock_guard<std::mutex> lk( orderMutex ); order.push_back( 3 ); } );
    
        f1.get( ); f2.get( ); f3.get( );
        Assert::IsTrue(  order == std::vector<int>{ 1, 2, 3 } );
    }
    
    TEST_METHOD(ThreadPool_Submit_Tasks_On_Different_Contexts_Execute_Concurrently)
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
        Assert::IsTrue(  signal );
    }
    
    // ─── SubmitMain (main-thread queue) ──────────────────────────────────────────
    
    TEST_METHOD(ThreadPool_SubmitMain_Task_Does_Not_Execute_Until_Update_Is_Called)
    {
        ThreadPool pool;
        pool.Start( 1 );
    
        bool ran = false;
        pool.SubmitMain( [ & ]( ) { ran = true; } );
        Assert::IsTrue(  !ran );
    
        pool.Update( );
        Assert::IsTrue(  ran );
    }
    
    TEST_METHOD(ThreadPool_SubmitMain_Future_Resolves_After_Update_Is_Called)
    {
        ThreadPool pool;
        pool.Start( 1 );
    
        auto future = pool.SubmitMain( [ ]( ) { return 99; } );
        pool.Update( );
        Assert::IsTrue(  future.get( ) == 99 );
    }
    
    TEST_METHOD(ThreadPool_SubmitMain_Multiple_Tasks_Drain_In_A_Single_Update_Call_In_FIFO_Order)
    {
        ThreadPool pool;
        pool.Start( 1 );
    
        std::vector<int> order;
        pool.SubmitMain( [ & ]( ) { order.push_back( 1 ); } );
        pool.SubmitMain( [ & ]( ) { order.push_back( 2 ); } );
        pool.SubmitMain( [ & ]( ) { order.push_back( 3 ); } );
    
        Assert::IsTrue(  order.empty( ) );
    
        pool.Update( );
        Assert::IsTrue(  order == std::vector<int>{ 1, 2, 3 } );
    }
    
    TEST_METHOD(ThreadPool_SubmitMain_Tasks_Queued_By_A_Main_Thread_Callback_Also_Drain_In_The_Same_Update_Call)
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
        Assert::IsTrue(  order == std::vector<int>{ 1, 2 } );
    }
    
    // ─── Update ──────────────────────────────────────────────────────────────────
    
    TEST_METHOD(ThreadPool_Update_No_Op_On_An_Empty_Main_Queue)
    {
        ThreadPool pool;
        pool.Start( 1 );
        pool.Update( );
        Assert::IsTrue( true );
    }
    
    TEST_METHOD(ThreadPool_Update_No_Op_After_Stop)
    {
        ThreadPool pool;
        pool.Start( 1 );
        bool ran = false;
        pool.SubmitMain( [ & ]( ) { ran = true; } );
        pool.Stop( );
        pool.Update( );
        Assert::IsTrue(  !ran );
    }
    
    // ─── Stop edge cases ─────────────────────────────────────────────────────────
    
    TEST_METHOD(ThreadPool_Stop_No_Op_When_Pool_Has_Not_Been_Started)
    {
        ThreadPool pool;
        pool.Stop( );
        Assert::IsTrue( true );
    }
    
    TEST_METHOD(ThreadPool_Stop_Waits_For_In_Flight_Task_To_Complete_Before_Returning)
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
    
        Assert::IsTrue(  taskComplete );
    }
    
    TEST_METHOD(ThreadPool_Stop_Queued_Tasks_Not_Yet_Started_Are_Dropped)
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
    
        Assert::IsTrue(  extraTasksRan < 3 );
    }
    
    };
}
