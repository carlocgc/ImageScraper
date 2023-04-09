#pragma once
#include <map>
#include <thread>
#include <queue>
#include <mutex>
#include <future>
#include <functional>

namespace ImageScraper
{
    using ThreadContext = uint16_t;

    class ThreadPool
    {
    public:
        ThreadPool( int numThreads );
        ~ThreadPool( );

        template<class F, class... Args>
        auto Submit( ThreadContext context, F&& f, Args&&... args ) -> std::future<decltype( f( args... ) )>
        {
            using ReturnT = decltype( f( args... ) );

            // Create a new packaged_task object that will wrap the function to be executed
            auto task = std::make_shared<std::packaged_task<ReturnT( )>>(
                // Bind the function f to its arguments (if any)
                std::bind( std::forward<F>( f ), std::forward<Args>( args )... )
                );

            std::future< ReturnT > res = task->get_future( );

            {
                std::unique_lock<std::mutex> lock( m_QueueMutexes[ context ] );
                m_TaskQueues[ context ].emplace( [ task ]( ) { ( *task )( ); } );
            }

            m_Conditions[ context ].notify_one( );
            return res;
        }

        template<class F, class... Args>
        auto SubmitMain( F&& f, Args&&... args ) -> std::future<decltype( f( args... ) )>
        {
            using ReturnT = decltype( f( args... ) );

            // Create a new packaged_task object that will wrap the function to be executed
            auto task = std::make_shared<std::packaged_task<ReturnT( )>>(
                // Bind the function f to its arguments (if any)
                std::bind( std::forward<F>( f ), std::forward<Args>( args )... )
                );

            std::future< ReturnT > res = task->get_future( );

            {
                std::unique_lock<std::mutex> lock( m_MainMutex );
                m_MainQueue.emplace( [ task ]( ) { ( *task )( ); } );
            }

            m_MainCondtion.notify_one( );
            return res;
        }

        void InvokeMainTaskQueue( );

    private:
        std::vector<std::thread> m_Threads{ };
        std::map<ThreadContext, std::queue<std::function<void( )>>> m_TaskQueues{ };
        std::map<ThreadContext, std::mutex> m_QueueMutexes{ };
        std::map<ThreadContext, std::condition_variable> m_Conditions{ };

        std::queue<std::function<void( )>> m_MainQueue{ };
        std::mutex m_MainMutex{ };
        std::condition_variable m_MainCondtion{ };

        std::mutex m_StopMutex{ };
        bool m_Stop{ false };
    };
}


