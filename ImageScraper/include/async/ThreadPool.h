#pragma once

#include "log/Logger.h"

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

        /// <summary>
        /// Submit callable to be executed on the given thread context
        /// </summary>
        /// <typeparam name="F"> Callable type </typeparam>
        /// <typeparam name="...Args"> Callable arge types </typeparam>
        /// <param name="context"> Thread context as integer, uniquely identifies the execution thread. </param>
        /// <param name="f"> Callable </param>
        /// <param name="...args"> Callable args </param>
        /// <returns></returns>
        template<class F, class... Args>
        auto Submit( ThreadContext context, F&& f, Args&&... args ) -> std::future<decltype( f( args... ) )>
        {
            if( context > m_Threads.size( ) )
            {
                AssertLog( "[%s] Invalid ThreadContext, max context count is set in ThreadPool ctor." );
            }

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

        /// <summary>
        /// Submit callable on main thread task queue, typically used for completion callbacks of worker thread tasks
        /// </summary>
        /// <typeparam name="F"> Callable type </typeparam>
        /// <typeparam name="...Args"> Callable arge types </typeparam>
        /// <param name="f"> Callable </param>
        /// <param name="...args"> Callable args </param>
        /// <returns></returns>
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

        /// <summary>
        /// Invokes tasks on the main thread task queue
        /// </summary>
        void Update( );

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


