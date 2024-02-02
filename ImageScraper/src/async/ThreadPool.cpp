#include "async/ThreadPool.h"

ImageScraper::ThreadPool::~ThreadPool( )
{
    Stop( );
}

void ImageScraper::ThreadPool::Start( int numThreads )
{
    if( m_IsRunning )
    {
        return;
    }

    if( m_Stopping.load( ) )
    {
        return;
    }

    m_IsRunning = true;

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
                                return m_Stopping.load( ) || !m_TaskQueues[ i ].empty( );
                            } );

                        if( m_Stopping.load( ) || m_TaskQueues[ i ].empty( ) )
                        {
                            lock.unlock( );
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

void ImageScraper::ThreadPool::Update( )
{
    std::function<void( )> task;

    {
        std::unique_lock<std::mutex> lock( m_MainMutex );

        if( m_Stopping.load( ) || m_MainQueue.empty( ) )
        {
            return;
        }

        task = std::move( m_MainQueue.front( ) );

        m_MainQueue.pop( );
    }

    task( );
}

void ImageScraper::ThreadPool::Stop( )
{
    if( !m_IsRunning )
    {
        return;
    }

    if( m_Stopping.load( ) )
    {
        return;
    }

    m_Stopping.store( true );    

    for( auto& [key, cond] : m_Conditions )
    {
        cond.notify_all( );        
    }

    for( auto& thread : m_Threads )
    {
        thread.join( );
    }

    m_IsRunning = false;
    m_Stopping.store( false );
}
