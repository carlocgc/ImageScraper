#pragma once
#include "asyncgc/ThreadPool.h"
#include "traits/TypeTraits.h"

namespace ImageScraper
{
    class ThreadPoolSingleton : public NonCopyMovable
    {
    public:
        static Asyncgc::ThreadPool& Instance( )
        {
            static Asyncgc::ThreadPool instance{ 2 };
            return instance;
        }

        static const Asyncgc::ThreadContext s_UIContext = 0;
        static const Asyncgc::ThreadContext s_NetworkContext = 1;

    private:
        ThreadPoolSingleton( ) = default;
        ~ThreadPoolSingleton( ) = default;
    };
}