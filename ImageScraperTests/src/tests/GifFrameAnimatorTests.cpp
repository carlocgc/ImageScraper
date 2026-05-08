#include "catch2/catch_amalgamated.hpp"
#include "ui/GifFrameAnimator.h"

using namespace ImageScraper;

// ----------------------------------------------------------------------------
// Initialise
// ----------------------------------------------------------------------------

TEST_CASE( "GifFrameAnimator starts at frame 0 after Initialise", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    REQUIRE( anim.CurrentFrame( ) == 0 );
}

TEST_CASE( "GifFrameAnimator FrameCount returns the initialised frame count", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 200, 150 }, 3 );
    REQUIRE( anim.FrameCount( ) == 3 );
}

// ----------------------------------------------------------------------------
// Advance
// ----------------------------------------------------------------------------

TEST_CASE( "GifFrameAnimator does not advance when delta is less than delay", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    anim.Advance( 50.0f );
    REQUIRE( anim.CurrentFrame( ) == 0 );
}

TEST_CASE( "GifFrameAnimator advances frame when accumulated delta exceeds delay", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    anim.Advance( 110.0f );
    REQUIRE( anim.CurrentFrame( ) == 1 );
}

TEST_CASE( "GifFrameAnimator wraps around past the last frame", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    anim.Advance( 350.0f );  // advances 3 full frames (3 x 100ms = 300ms consumed, 50ms left)
    REQUIRE( anim.CurrentFrame( ) == 0 );
}

TEST_CASE( "GifFrameAnimator does not advance when frame count is 1", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100 }, 1 );
    anim.Advance( 1000.0f );
    REQUIRE( anim.CurrentFrame( ) == 0 );
}

TEST_CASE( "GifFrameAnimator uses default 100ms delay when delays vector is empty", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { }, 3 );
    anim.Advance( 150.0f );  // should advance once at 100ms default
    REQUIRE( anim.CurrentFrame( ) == 1 );
}

TEST_CASE( "GifFrameAnimator respects per-frame delay differences", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 200, 50, 50 }, 3 );
    anim.Advance( 210.0f );  // crosses first frame boundary only (200ms)
    REQUIRE( anim.CurrentFrame( ) == 1 );
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------

TEST_CASE( "GifFrameAnimator Reset returns to frame 0 and clears accumulator", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    anim.Advance( 250.0f );
    anim.Reset( );
    REQUIRE( anim.CurrentFrame( ) == 0 );

    // accumulator must be cleared — a small delta must not advance
    anim.Advance( 10.0f );
    REQUIRE( anim.CurrentFrame( ) == 0 );
}

// ----------------------------------------------------------------------------
// SetFrame
// ----------------------------------------------------------------------------

TEST_CASE( "GifFrameAnimator SetFrame jumps to the target frame", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    anim.SetFrame( 2 );
    REQUIRE( anim.CurrentFrame( ) == 2 );
}

TEST_CASE( "GifFrameAnimator SetFrame clears the accumulator", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    anim.Advance( 250.0f );  // lands on frame 2 with 50ms left in accum
    anim.SetFrame( 1 );

    REQUIRE( anim.CurrentFrame( ) == 1 );

    // accumulator must be zero — a small delta should not advance
    anim.Advance( 10.0f );
    REQUIRE( anim.CurrentFrame( ) == 1 );
}

TEST_CASE( "GifFrameAnimator SetFrame clamps to valid range", "[GifFrameAnimator]" )
{
    GifFrameAnimator anim;
    anim.Initialise( { 100, 100, 100 }, 3 );
    anim.SetFrame( 99 );
    REQUIRE( anim.CurrentFrame( ) == 2 );
}
