#pragma once

#include "traits/TypeTraits.h"
#include "async/ThreadPool.h"

#define THREAD_POOL_MAX_THREADS 3

namespace ImageScraper
{
    class TaskManager : public NonCopyMovable
    {
    public:
        static ThreadPool& Instance( )
        {
            static ThreadPool instance{ THREAD_POOL_MAX_THREADS };
            return instance;
        }

        static const ThreadContext s_UIContext = 0;
        static const ThreadContext s_NetworkContext = 1;
        static const ThreadContext s_ListenServer = 2;

    private:
        TaskManager( ) = default;
        ~TaskManager( ) = default;
    };
}