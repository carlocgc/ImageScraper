#include "CppUnitTest.h"
#include "collections/RingBuffer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    // ---------------------------------------------------------------------------
    // Construction
    // ---------------------------------------------------------------------------


    TEST_CLASS(RingBufferTests)
    {
    public:
    TEST_METHOD(RingBuffer_Is_Empty_On_Construction)
    {
        RingBuffer<int> rb( 4 );
        Assert::IsTrue(  rb.IsEmpty( ) );
        Assert::IsTrue(  !rb.IsFull( ) );
        Assert::IsTrue(  rb.GetSize( ) == 0 );
        Assert::IsTrue(  rb.GetCapacity( ) == 4 );
    }
    
    // ---------------------------------------------------------------------------
    // Push / Size
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Push_Increases_Size)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 10 );
        Assert::IsTrue(  rb.GetSize( ) == 1 );
        rb.Push( 20 );
        Assert::IsTrue(  rb.GetSize( ) == 2 );
    }
    
    TEST_METHOD(Buffer_Is_Full_When_Capacity_Is_Reached)
    {
        RingBuffer<int> rb( 3 );
        rb.Push( 1 );
        rb.Push( 2 );
        Assert::IsTrue(  !rb.IsFull( ) );
        rb.Push( 3 );
        Assert::IsTrue(  rb.IsFull( ) );
        Assert::IsTrue(  rb.GetSize( ) == 3 );
    }
    
    // ---------------------------------------------------------------------------
    // Pop
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Pop_Decreases_Size)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Pop( );
        Assert::IsTrue(  rb.GetSize( ) == 1 );
    }
    
    TEST_METHOD(Pop_On_Empty_Buffer_Is_A_No_Op)
    {
        RingBuffer<int> rb( 4 );
        rb.Pop( );
        Assert::IsTrue(  rb.IsEmpty( ) );
        Assert::IsTrue(  rb.GetSize( ) == 0 );
    }
    
    TEST_METHOD(Buffer_Is_Empty_After_Popping_All_Elements)
    {
        RingBuffer<int> rb( 3 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Push( 3 );
        rb.Pop( );
        rb.Pop( );
        rb.Pop( );
        Assert::IsTrue(  rb.IsEmpty( ) );
    }
    
    // ---------------------------------------------------------------------------
    // Front / Back
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Front_Returns_The_Oldest_Element)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 10 );
        rb.Push( 20 );
        rb.Push( 30 );
        Assert::IsTrue(  rb.Front( ) == 10 );
    }
    
    TEST_METHOD(Back_Returns_The_Newest_Element)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 10 );
        rb.Push( 20 );
        rb.Push( 30 );
        Assert::IsTrue(  rb.Back( ) == 30 );
    }
    
    TEST_METHOD(Front_Advances_After_Pop)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Push( 3 );
        rb.Pop( );
        Assert::IsTrue(  rb.Front( ) == 2 );
    }
    
    // ---------------------------------------------------------------------------
    // Wrap-around overflow — oldest element discarded
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Push_Beyond_Capacity_Discards_The_Oldest_Element)
    {
        RingBuffer<int> rb( 3 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Push( 3 ); // full
        rb.Push( 4 ); // overwrites 1
        Assert::IsTrue(  rb.GetSize( ) == 3 );
        Assert::IsTrue(  rb.Front( ) == 2 );
        Assert::IsTrue(  rb.Back( ) == 4 );
    }
    
    TEST_METHOD(Multiple_Overflow_Pushes_Keep_Only_The_Last_Capacity_Elements)
    {
        RingBuffer<int> rb( 3 );
        for( int i = 1; i <= 7; ++i )
        {
            rb.Push( i );
        }
        // Buffer should hold 5, 6, 7
        Assert::IsTrue(  rb.GetSize( ) == 3 );
        Assert::IsTrue(  rb.Front( ) == 5 );
        Assert::IsTrue(  rb.Back( ) == 7 );
    }
    
    // ---------------------------------------------------------------------------
    // operator[]
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Operator_Returns_Elements_In_Push_Order)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 10 );
        rb.Push( 20 );
        rb.Push( 30 );
        Assert::IsTrue(  rb[ 0 ] == 10 );
        Assert::IsTrue(  rb[ 1 ] == 20 );
        Assert::IsTrue(  rb[ 2 ] == 30 );
    }
    
    TEST_METHOD(Operator_Wraps_Correctly_After_Overflow)
    {
        RingBuffer<int> rb( 3 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Push( 3 );
        rb.Push( 4 ); // evicts 1; buffer holds 2, 3, 4
        Assert::IsTrue(  rb[ 0 ] == 2 );
        Assert::IsTrue(  rb[ 1 ] == 3 );
        Assert::IsTrue(  rb[ 2 ] == 4 );
    }
    
    TEST_METHOD(Operator_Throws_Out_Of_Range_For_Index_Equal_To_Size)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 1 );
        rb.Push( 2 );
        // GetSize() == 2, so index 2 is out of range
        Assert::ExpectException<std::out_of_range >( [&] { rb[ 2 ]; } );
    }
    
    TEST_METHOD(Operator_Throws_Out_Of_Range_For_Index_Beyond_Size)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 1 );
        Assert::ExpectException<std::out_of_range >( [&] { rb[ 5 ]; } );
    }
    
    // ---------------------------------------------------------------------------
    // Clear
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Clear_Resets_Size_And_Empty_State)
    {
        RingBuffer<int> rb( 4 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Clear( );
        Assert::IsTrue(  rb.IsEmpty( ) );
        Assert::IsTrue(  rb.GetSize( ) == 0 );
        Assert::IsTrue(  rb.GetCapacity( ) == 4 );
    }
    
    TEST_METHOD(Buffer_Is_Usable_After_Clear)
    {
        RingBuffer<int> rb( 3 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Push( 3 );
        rb.Clear( );
        rb.Push( 42 );
        Assert::IsTrue(  rb.GetSize( ) == 1 );
        Assert::IsTrue(  rb.Front( ) == 42 );
    }
    
    // ---------------------------------------------------------------------------
    // Iterators (non-wrapped — elements fit without overflow)
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Begin_End_On_Empty_Buffer)
    {
        RingBuffer<int> rb( 4 );
        Assert::IsTrue(  rb.begin( ) == rb.end( ) );
    }
    
    TEST_METHOD(Range_Based_For_Covers_All_Elements_When_Buffer_Has_Not_Wrapped)
    {
        RingBuffer<int> rb( 5 );
        rb.Push( 10 );
        rb.Push( 20 );
        rb.Push( 30 );
    
        std::vector<int> result;
        for( auto& v : rb )
        {
            result.push_back( v );
        }
    
        Assert::IsTrue(  result.size( ) == 3 );
        Assert::IsTrue(  result[ 0 ] == 10 );
        Assert::IsTrue(  result[ 1 ] == 20 );
        Assert::IsTrue(  result[ 2 ] == 30 );
    }
    
    // ---------------------------------------------------------------------------
    // RemoveAt
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(RemoveAt_Middle_Element_Shifts_Remaining_Elements_Correctly)
    {
        RingBuffer<int> rb( 8 );
        rb.Push( 0 );
        rb.Push( 1 );
        rb.Push( 2 );
        rb.Push( 3 );
        rb.Push( 4 );
    
        rb.RemoveAt( 2 ); // remove element with value 2
    
        Assert::IsTrue(  rb.GetSize( ) == 4 );
        Assert::IsTrue(  rb[ 0 ] == 0 );
        Assert::IsTrue(  rb[ 1 ] == 1 );
        Assert::IsTrue(  rb[ 2 ] == 3 );
        Assert::IsTrue(  rb[ 3 ] == 4 );
    }
    
    TEST_METHOD(RemoveAt_First_Element_Shifts_Remaining_Elements_Correctly)
    {
        RingBuffer<int> rb( 8 );
        rb.Push( 10 );
        rb.Push( 20 );
        rb.Push( 30 );
    
        rb.RemoveAt( 0 );
    
        Assert::IsTrue(  rb.GetSize( ) == 2 );
        Assert::IsTrue(  rb.Front( ) == 20 );
    }
    
    TEST_METHOD(RemoveAt_Last_Element_Decrements_Size_Without_Affecting_Earlier_Items)
    {
        RingBuffer<int> rb( 8 );
        rb.Push( 10 );
        rb.Push( 20 );
        rb.Push( 30 );
    
        rb.RemoveAt( 2 );
    
        Assert::IsTrue(  rb.GetSize( ) == 2 );
        Assert::IsTrue(  rb.Back( ) == 20 );
    }
    
    TEST_METHOD(RemoveAt_Out_Of_Range_Index_Is_A_No_Op)
    {
        RingBuffer<int> rb( 8 );
        rb.Push( 1 );
        rb.Push( 2 );
    
        rb.RemoveAt( 5 );
    
        Assert::IsTrue(  rb.GetSize( ) == 2 );
    }
    
    };
}
