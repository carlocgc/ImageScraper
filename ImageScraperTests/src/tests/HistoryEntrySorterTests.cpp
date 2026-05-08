#include "catch2/catch_amalgamated.hpp"
#include "ui/HistoryEntrySorter.h"

#include <algorithm>
#include <vector>

using namespace ImageScraper;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static HistorySortEntry MakeFile(
    const std::string& label,
    uintmax_t sizeBytes = 0,
    const std::string& typeLabel = "",
    unsigned long long creationTicks = 0,
    const std::string& pathString = "" )
{
    HistorySortEntry e;
    e.m_IsDirectory   = false;
    e.m_Label         = label;
    e.m_SizeBytes     = sizeBytes;
    e.m_TypeLabel     = typeLabel;
    e.m_CreationTicks = creationTicks;
    e.m_PathString    = pathString.empty( ) ? label : pathString;
    return e;
}

static HistorySortEntry MakeDir( const std::string& label )
{
    HistorySortEntry e;
    e.m_IsDirectory   = true;
    e.m_Label         = label;
    e.m_SizeBytes     = 0;
    e.m_CreationTicks = 0;
    e.m_PathString    = label;
    return e;
}

// ---------------------------------------------------------------------------
// Directory-first invariant
// ---------------------------------------------------------------------------

TEST_CASE( "MakeHistoryComparator sorts directories before files (Name/asc)", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Name, true );

    const HistorySortEntry file = MakeFile( "alpha.jpg" );
    const HistorySortEntry dir  = MakeDir(  "zebra" );

    REQUIRE( cmp( dir, file ) == true );   // dir sorts before file
    REQUIRE( cmp( file, dir ) == false );  // file does NOT sort before dir
}

TEST_CASE( "MakeHistoryComparator sorts directories before files (Name/desc)", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Name, false );

    const HistorySortEntry file = MakeFile( "alpha.jpg" );
    const HistorySortEntry dir  = MakeDir(  "zebra" );

    // Directories still come first even in descending order
    REQUIRE( cmp( dir, file ) == true );
    REQUIRE( cmp( file, dir ) == false );
}

// ---------------------------------------------------------------------------
// Name column
// ---------------------------------------------------------------------------

TEST_CASE( "MakeHistoryComparator Name ascending orders alphabetically", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Name, true );

    std::vector<HistorySortEntry> v = { MakeFile( "charlie" ), MakeFile( "alpha" ), MakeFile( "bravo" ) };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_Label == "alpha" );
    REQUIRE( v[ 1 ].m_Label == "bravo" );
    REQUIRE( v[ 2 ].m_Label == "charlie" );
}

TEST_CASE( "MakeHistoryComparator Name descending reverses alphabetical order", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Name, false );

    std::vector<HistorySortEntry> v = { MakeFile( "alpha" ), MakeFile( "charlie" ), MakeFile( "bravo" ) };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_Label == "charlie" );
    REQUIRE( v[ 1 ].m_Label == "bravo" );
    REQUIRE( v[ 2 ].m_Label == "alpha" );
}

TEST_CASE( "MakeHistoryComparator Name comparison is case-insensitive", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Name, true );

    std::vector<HistorySortEntry> v = { MakeFile( "Beta" ), MakeFile( "alpha" ), MakeFile( "CHARLIE" ) };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_Label == "alpha" );
    REQUIRE( v[ 1 ].m_Label == "Beta" );
    REQUIRE( v[ 2 ].m_Label == "CHARLIE" );
}

// ---------------------------------------------------------------------------
// Size column
// ---------------------------------------------------------------------------

TEST_CASE( "MakeHistoryComparator Size ascending orders smallest first", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Size, true );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "c", 3000 ),
        MakeFile( "a", 1000 ),
        MakeFile( "b", 2000 ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_SizeBytes == 1000 );
    REQUIRE( v[ 1 ].m_SizeBytes == 2000 );
    REQUIRE( v[ 2 ].m_SizeBytes == 3000 );
}

TEST_CASE( "MakeHistoryComparator Size descending orders largest first", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Size, false );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "a", 100 ),
        MakeFile( "b", 300 ),
        MakeFile( "c", 200 ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_SizeBytes == 300 );
    REQUIRE( v[ 1 ].m_SizeBytes == 200 );
    REQUIRE( v[ 2 ].m_SizeBytes == 100 );
}

// ---------------------------------------------------------------------------
// Type column
// ---------------------------------------------------------------------------

TEST_CASE( "MakeHistoryComparator Type ascending orders type labels alphabetically", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Type, true );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "c", 0, "Video file" ),
        MakeFile( "a", 0, "Image file" ),
        MakeFile( "b", 0, "Audio file" ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_TypeLabel == "Audio file" );
    REQUIRE( v[ 1 ].m_TypeLabel == "Image file" );
    REQUIRE( v[ 2 ].m_TypeLabel == "Video file" );
}

TEST_CASE( "MakeHistoryComparator Type descending reverses type label order", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Type, false );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "a", 0, "Audio file" ),
        MakeFile( "b", 0, "Video file" ),
        MakeFile( "c", 0, "Image file" ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_TypeLabel == "Video file" );
    REQUIRE( v[ 1 ].m_TypeLabel == "Image file" );
    REQUIRE( v[ 2 ].m_TypeLabel == "Audio file" );
}

// ---------------------------------------------------------------------------
// Created column
// ---------------------------------------------------------------------------

TEST_CASE( "MakeHistoryComparator Created ascending orders oldest first", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Created, true );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "c", 0, "", 3000 ),
        MakeFile( "a", 0, "", 1000 ),
        MakeFile( "b", 0, "", 2000 ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_CreationTicks == 1000 );
    REQUIRE( v[ 1 ].m_CreationTicks == 2000 );
    REQUIRE( v[ 2 ].m_CreationTicks == 3000 );
}

TEST_CASE( "MakeHistoryComparator Created descending orders newest first", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Created, false );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "a", 0, "", 100 ),
        MakeFile( "b", 0, "", 300 ),
        MakeFile( "c", 0, "", 200 ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_CreationTicks == 300 );
    REQUIRE( v[ 1 ].m_CreationTicks == 200 );
    REQUIRE( v[ 2 ].m_CreationTicks == 100 );
}

// ---------------------------------------------------------------------------
// Tiebreaker — equal primary key falls back to Name then PathString
// ---------------------------------------------------------------------------

TEST_CASE( "MakeHistoryComparator tiebreaks equal sizes by Name ascending", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Size, true );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "charlie", 500 ),
        MakeFile( "alpha",   500 ),
        MakeFile( "bravo",   500 ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_Label == "alpha" );
    REQUIRE( v[ 1 ].m_Label == "bravo" );
    REQUIRE( v[ 2 ].m_Label == "charlie" );
}

TEST_CASE( "MakeHistoryComparator tiebreaks equal name+size by PathString", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Size, true );

    // Same label and size — PathString is the final tiebreaker
    std::vector<HistorySortEntry> v =
    {
        MakeFile( "file", 500, "", 0, "C:\\z\\file" ),
        MakeFile( "file", 500, "", 0, "C:\\a\\file" ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    REQUIRE( v[ 0 ].m_PathString == "C:\\a\\file" );
    REQUIRE( v[ 1 ].m_PathString == "C:\\z\\file" );
}

// ---------------------------------------------------------------------------
// Mixed directories and files
// ---------------------------------------------------------------------------

TEST_CASE( "MakeHistoryComparator puts directories first then sorts each group", "[HistoryEntrySorter]" )
{
    auto cmp = MakeHistoryComparator( HistorySortColumn::Name, true );

    std::vector<HistorySortEntry> v =
    {
        MakeFile( "zebra.jpg" ),
        MakeDir(  "alpha_dir" ),
        MakeFile( "alpha.jpg" ),
        MakeDir(  "zebra_dir" ),
    };
    std::sort( v.begin( ), v.end( ), cmp );

    // Both directories come first, in name order
    REQUIRE( v[ 0 ].m_IsDirectory == true );
    REQUIRE( v[ 0 ].m_Label == "alpha_dir" );
    REQUIRE( v[ 1 ].m_IsDirectory == true );
    REQUIRE( v[ 1 ].m_Label == "zebra_dir" );

    // Files follow, in name order
    REQUIRE( v[ 2 ].m_IsDirectory == false );
    REQUIRE( v[ 2 ].m_Label == "alpha.jpg" );
    REQUIRE( v[ 3 ].m_IsDirectory == false );
    REQUIRE( v[ 3 ].m_Label == "zebra.jpg" );
}
