#include "CppUnitTest.h"
#include "ui/HistoryEntrySorter.h"

#include <algorithm>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

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


    TEST_CLASS(HistoryEntrySorterTests)
    {
    public:
    TEST_METHOD(MakeHistoryComparator_Sorts_Directories_Before_Files_Name_Asc)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Name, true );
    
        const HistorySortEntry file = MakeFile( "alpha.jpg" );
        const HistorySortEntry dir  = MakeDir(  "zebra" );
    
        Assert::IsTrue(  cmp( dir, file ) == true );   // dir sorts before file
        Assert::IsTrue(  cmp( file, dir ) == false );  // file does NOT sort before dir
    }
    
    TEST_METHOD(MakeHistoryComparator_Sorts_Directories_Before_Files_Name_Desc)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Name, false );
    
        const HistorySortEntry file = MakeFile( "alpha.jpg" );
        const HistorySortEntry dir  = MakeDir(  "zebra" );
    
        // Directories still come first even in descending order
        Assert::IsTrue(  cmp( dir, file ) == true );
        Assert::IsTrue(  cmp( file, dir ) == false );
    }
    
    // ---------------------------------------------------------------------------
    // Name column
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(MakeHistoryComparator_Name_Ascending_Orders_Alphabetically)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Name, true );
    
        std::vector<HistorySortEntry> v = { MakeFile( "charlie" ), MakeFile( "alpha" ), MakeFile( "bravo" ) };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_Label == "alpha" );
        Assert::IsTrue(  v[ 1 ].m_Label == "bravo" );
        Assert::IsTrue(  v[ 2 ].m_Label == "charlie" );
    }
    
    TEST_METHOD(MakeHistoryComparator_Name_Descending_Reverses_Alphabetical_Order)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Name, false );
    
        std::vector<HistorySortEntry> v = { MakeFile( "alpha" ), MakeFile( "charlie" ), MakeFile( "bravo" ) };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_Label == "charlie" );
        Assert::IsTrue(  v[ 1 ].m_Label == "bravo" );
        Assert::IsTrue(  v[ 2 ].m_Label == "alpha" );
    }
    
    TEST_METHOD(MakeHistoryComparator_Name_Comparison_Is_Case_Insensitive)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Name, true );
    
        std::vector<HistorySortEntry> v = { MakeFile( "Beta" ), MakeFile( "alpha" ), MakeFile( "CHARLIE" ) };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_Label == "alpha" );
        Assert::IsTrue(  v[ 1 ].m_Label == "Beta" );
        Assert::IsTrue(  v[ 2 ].m_Label == "CHARLIE" );
    }
    
    // ---------------------------------------------------------------------------
    // Size column
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(MakeHistoryComparator_Size_Ascending_Orders_Smallest_First)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Size, true );
    
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "c", 3000 ),
            MakeFile( "a", 1000 ),
            MakeFile( "b", 2000 ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_SizeBytes == 1000 );
        Assert::IsTrue(  v[ 1 ].m_SizeBytes == 2000 );
        Assert::IsTrue(  v[ 2 ].m_SizeBytes == 3000 );
    }
    
    TEST_METHOD(MakeHistoryComparator_Size_Descending_Orders_Largest_First)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Size, false );
    
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "a", 100 ),
            MakeFile( "b", 300 ),
            MakeFile( "c", 200 ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_SizeBytes == 300 );
        Assert::IsTrue(  v[ 1 ].m_SizeBytes == 200 );
        Assert::IsTrue(  v[ 2 ].m_SizeBytes == 100 );
    }
    
    // ---------------------------------------------------------------------------
    // Type column
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(MakeHistoryComparator_Type_Ascending_Orders_Type_Labels_Alphabetically)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Type, true );
    
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "c", 0, "Video file" ),
            MakeFile( "a", 0, "Image file" ),
            MakeFile( "b", 0, "Audio file" ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_TypeLabel == "Audio file" );
        Assert::IsTrue(  v[ 1 ].m_TypeLabel == "Image file" );
        Assert::IsTrue(  v[ 2 ].m_TypeLabel == "Video file" );
    }
    
    TEST_METHOD(MakeHistoryComparator_Type_Descending_Reverses_Type_Label_Order)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Type, false );
    
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "a", 0, "Audio file" ),
            MakeFile( "b", 0, "Video file" ),
            MakeFile( "c", 0, "Image file" ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_TypeLabel == "Video file" );
        Assert::IsTrue(  v[ 1 ].m_TypeLabel == "Image file" );
        Assert::IsTrue(  v[ 2 ].m_TypeLabel == "Audio file" );
    }
    
    // ---------------------------------------------------------------------------
    // Created column
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(MakeHistoryComparator_Created_Ascending_Orders_Oldest_First)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Created, true );
    
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "c", 0, "", 3000 ),
            MakeFile( "a", 0, "", 1000 ),
            MakeFile( "b", 0, "", 2000 ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_CreationTicks == 1000 );
        Assert::IsTrue(  v[ 1 ].m_CreationTicks == 2000 );
        Assert::IsTrue(  v[ 2 ].m_CreationTicks == 3000 );
    }
    
    TEST_METHOD(MakeHistoryComparator_Created_Descending_Orders_Newest_First)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Created, false );
    
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "a", 0, "", 100 ),
            MakeFile( "b", 0, "", 300 ),
            MakeFile( "c", 0, "", 200 ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_CreationTicks == 300 );
        Assert::IsTrue(  v[ 1 ].m_CreationTicks == 200 );
        Assert::IsTrue(  v[ 2 ].m_CreationTicks == 100 );
    }
    
    // ---------------------------------------------------------------------------
    // Tiebreaker — equal primary key falls back to Name then PathString
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(MakeHistoryComparator_Tiebreaks_Equal_Sizes_By_Name_Ascending)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Size, true );
    
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "charlie", 500 ),
            MakeFile( "alpha",   500 ),
            MakeFile( "bravo",   500 ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_Label == "alpha" );
        Assert::IsTrue(  v[ 1 ].m_Label == "bravo" );
        Assert::IsTrue(  v[ 2 ].m_Label == "charlie" );
    }
    
    TEST_METHOD(MakeHistoryComparator_Tiebreaks_Equal_Name_Size_By_PathString)
    {
        auto cmp = MakeHistoryComparator( HistorySortColumn::Size, true );
    
        // Same label and size — PathString is the final tiebreaker
        std::vector<HistorySortEntry> v =
        {
            MakeFile( "file", 500, "", 0, "C:\\z\\file" ),
            MakeFile( "file", 500, "", 0, "C:\\a\\file" ),
        };
        std::sort( v.begin( ), v.end( ), cmp );
    
        Assert::IsTrue(  v[ 0 ].m_PathString == "C:\\a\\file" );
        Assert::IsTrue(  v[ 1 ].m_PathString == "C:\\z\\file" );
    }
    
    // ---------------------------------------------------------------------------
    // Mixed directories and files
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(MakeHistoryComparator_Puts_Directories_First_Then_Sorts_Each_Group)
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
        Assert::IsTrue(  v[ 0 ].m_IsDirectory == true );
        Assert::IsTrue(  v[ 0 ].m_Label == "alpha_dir" );
        Assert::IsTrue(  v[ 1 ].m_IsDirectory == true );
        Assert::IsTrue(  v[ 1 ].m_Label == "zebra_dir" );
    
        // Files follow, in name order
        Assert::IsTrue(  v[ 2 ].m_IsDirectory == false );
        Assert::IsTrue(  v[ 2 ].m_Label == "alpha.jpg" );
        Assert::IsTrue(  v[ 3 ].m_IsDirectory == false );
        Assert::IsTrue(  v[ 3 ].m_Label == "zebra.jpg" );
    }
    
    };
}
