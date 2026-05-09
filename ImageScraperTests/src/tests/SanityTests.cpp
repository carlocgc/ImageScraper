#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{
    TEST_CLASS(SanityTests)
    {
    public:
        TEST_METHOD(SanityCheck)
        {
            Assert::AreEqual( 2, 1 + 1 );
        }
    };
}
