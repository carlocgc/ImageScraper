#include "async/ThreadPool.h"

ImageScraper::ThreadPool::ThreadPool( int numThreads )
{
    for( int i = 0; i < numThreads; ++i )
    {
        m_Threads.emplace_back( [ this, i ]
            {
                while( true )
                {
                    std::function<void( )> task;

                    {
                        std::unique_lock<std::mutex> lock( m_QueueMutexes[ i ] );
                        m_Conditions[ i ].wait( lock, [ this, i ]
                            {
                                return m_Stop || !m_TaskQueues[ i ].empty( );
                            } );

                        if( m_Stop || m_TaskQueues[ i ].empty( ) )
                        {
                            return;
                        }

                        task = std::move( m_TaskQueues[ i ].front( ) );

                        m_TaskQueues[ i ].pop( );
                    }

                    task( );
                }
            } );
    }
}

ImageScraper::ThreadPool::~ThreadPool( )
{
    {
        std::unique_lock<std::mutex> lock( m_StopMutex );
        m_Stop = true;
    }

    for( auto& [key, cond] : m_Conditions )
    {
        cond.notify_all( );
    }

    for( auto& thread : m_Threads )
    {
        thread.join( );
    }
}

void ImageScraper::ThreadPool::InvokeMainTaskQueue( )
{
    std::function<void( )> task;

    {
        std::unique_lock<std::mutex> lock( m_MainMutex );

        if( m_Stop || m_MainQueue.empty( ) )
        {
            return;
        }

        task = std::move( m_MainQueue.front( ) );

        m_MainQueue.pop( );
    }

    task( );
}