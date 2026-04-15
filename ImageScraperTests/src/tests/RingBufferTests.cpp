#include "catch2/catch_amalgamated.hpp"
#include "collections/RingBuffer.h"

using namespace ImageScraper;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE( "RingBuffer is empty on construction", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    REQUIRE( rb.IsEmpty( ) );
    REQUIRE( !rb.IsFull( ) );
    REQUIRE( rb.GetSize( ) == 0 );
    REQUIRE( rb.GetCapacity( ) == 4 );
}

// ---------------------------------------------------------------------------
// Push / Size
// ---------------------------------------------------------------------------

TEST_CASE( "Push increases size", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 10 );
    REQUIRE( rb.GetSize( ) == 1 );
    rb.Push( 20 );
    REQUIRE( rb.GetSize( ) == 2 );
}

TEST_CASE( "Buffer is full when capacity is reached", "[RingBuffer]" )
{
    RingBuffer<int> rb( 3 );
    rb.Push( 1 );
    rb.Push( 2 );
    REQUIRE( !rb.IsFull( ) );
    rb.Push( 3 );
    REQUIRE( rb.IsFull( ) );
    REQUIRE( rb.GetSize( ) == 3 );
}

// ---------------------------------------------------------------------------
// Pop
// ---------------------------------------------------------------------------

TEST_CASE( "Pop decreases size", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 1 );
    rb.Push( 2 );
    rb.Pop( );
    REQUIRE( rb.GetSize( ) == 1 );
}

TEST_CASE( "Pop on empty buffer is a no-op", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Pop( );
    REQUIRE( rb.IsEmpty( ) );
    REQUIRE( rb.GetSize( ) == 0 );
}

TEST_CASE( "Buffer is empty after popping all elements", "[RingBuffer]" )
{
    RingBuffer<int> rb( 3 );
    rb.Push( 1 );
    rb.Push( 2 );
    rb.Push( 3 );
    rb.Pop( );
    rb.Pop( );
    rb.Pop( );
    REQUIRE( rb.IsEmpty( ) );
}

// ---------------------------------------------------------------------------
// Front / Back
// ---------------------------------------------------------------------------

TEST_CASE( "Front returns the oldest element", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 10 );
    rb.Push( 20 );
    rb.Push( 30 );
    REQUIRE( rb.Front( ) == 10 );
}

TEST_CASE( "Back returns the newest element", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 10 );
    rb.Push( 20 );
    rb.Push( 30 );
    REQUIRE( rb.Back( ) == 30 );
}

TEST_CASE( "Front advances after Pop", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 1 );
    rb.Push( 2 );
    rb.Push( 3 );
    rb.Pop( );
    REQUIRE( rb.Front( ) == 2 );
}

// ---------------------------------------------------------------------------
// Wrap-around overflow — oldest element discarded
// ---------------------------------------------------------------------------

TEST_CASE( "Push beyond capacity discards the oldest element", "[RingBuffer]" )
{
    RingBuffer<int> rb( 3 );
    rb.Push( 1 );
    rb.Push( 2 );
    rb.Push( 3 ); // full
    rb.Push( 4 ); // overwrites 1
    REQUIRE( rb.GetSize( ) == 3 );
    REQUIRE( rb.Front( ) == 2 );
    REQUIRE( rb.Back( ) == 4 );
}

TEST_CASE( "Multiple overflow pushes keep only the last capacity elements", "[RingBuffer]" )
{
    RingBuffer<int> rb( 3 );
    for( int i = 1; i <= 7; ++i )
    {
        rb.Push( i );
    }
    // Buffer should hold 5, 6, 7
    REQUIRE( rb.GetSize( ) == 3 );
    REQUIRE( rb.Front( ) == 5 );
    REQUIRE( rb.Back( ) == 7 );
}

// ---------------------------------------------------------------------------
// operator[]
// ---------------------------------------------------------------------------

TEST_CASE( "operator[] returns elements in push order", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 10 );
    rb.Push( 20 );
    rb.Push( 30 );
    REQUIRE( rb[ 0 ] == 10 );
    REQUIRE( rb[ 1 ] == 20 );
    REQUIRE( rb[ 2 ] == 30 );
}

TEST_CASE( "operator[] wraps correctly after overflow", "[RingBuffer]" )
{
    RingBuffer<int> rb( 3 );
    rb.Push( 1 );
    rb.Push( 2 );
    rb.Push( 3 );
    rb.Push( 4 ); // evicts 1; buffer holds 2, 3, 4
    REQUIRE( rb[ 0 ] == 2 );
    REQUIRE( rb[ 1 ] == 3 );
    REQUIRE( rb[ 2 ] == 4 );
}

TEST_CASE( "operator[] throws out_of_range for index equal to size", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 1 );
    rb.Push( 2 );
    // GetSize() == 2, so index 2 is out of range
    REQUIRE_THROWS_AS( rb[ 2 ], std::out_of_range );
}

TEST_CASE( "operator[] throws out_of_range for index beyond size", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 1 );
    REQUIRE_THROWS_AS( rb[ 5 ], std::out_of_range );
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST_CASE( "Clear resets size and empty state", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    rb.Push( 1 );
    rb.Push( 2 );
    rb.Clear( );
    REQUIRE( rb.IsEmpty( ) );
    REQUIRE( rb.GetSize( ) == 0 );
    REQUIRE( rb.GetCapacity( ) == 4 );
}

TEST_CASE( "Buffer is usable after Clear", "[RingBuffer]" )
{
    RingBuffer<int> rb( 3 );
    rb.Push( 1 );
    rb.Push( 2 );
    rb.Push( 3 );
    rb.Clear( );
    rb.Push( 42 );
    REQUIRE( rb.GetSize( ) == 1 );
    REQUIRE( rb.Front( ) == 42 );
}

// ---------------------------------------------------------------------------
// Iterators (non-wrapped — elements fit without overflow)
// ---------------------------------------------------------------------------

TEST_CASE( "begin == end on empty buffer", "[RingBuffer]" )
{
    RingBuffer<int> rb( 4 );
    REQUIRE( rb.begin( ) == rb.end( ) );
}

TEST_CASE( "Range-based for covers all elements when buffer has not wrapped", "[RingBuffer]" )
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

    REQUIRE( result.size( ) == 3 );
    REQUIRE( result[ 0 ] == 10 );
    REQUIRE( result[ 1 ] == 20 );
    REQUIRE( result[ 2 ] == 30 );
}
