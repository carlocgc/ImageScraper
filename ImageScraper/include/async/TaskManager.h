#pragma once
#pragma once
#include "traits/TypeTraits.h"
#include "async/ThreadPool.h"

namespace ImageScraper
{
    class TaskManager : public NonCopyMovable
    {
    public:
        static ThreadPool& Instance( )
        {
            static ThreadPool instance{ 2 };
            return instance;
        }

        static const ThreadContext s_UIContext = 0;
        static const ThreadContext s_NetworkContext = 1;

    private:
        TaskManager( ) = default;
        ~TaskManager( ) = default;
    };
}