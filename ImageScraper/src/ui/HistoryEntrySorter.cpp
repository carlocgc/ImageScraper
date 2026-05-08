#include "ui/HistoryEntrySorter.h"
#include "utils/StringUtils.h"

namespace
{
    int CompareStringsCaseInsensitive( const std::string& lhs, const std::string& rhs )
    {
        const std::string lhsLower = ImageScraper::StringUtils::ToLower( lhs );
        const std::string rhsLower = ImageScraper::StringUtils::ToLower( rhs );

        if( lhsLower < rhsLower )
        {
            return -1;
        }

        if( lhsLower > rhsLower )
        {
            return 1;
        }

        return 0;
    }
}

std::function<bool( const ImageScraper::HistorySortEntry&, const ImageScraper::HistorySortEntry& )>
    ImageScraper::MakeHistoryComparator( HistorySortColumn column, bool ascending )
{
    return [ column, ascending ]( const HistorySortEntry& lhs, const HistorySortEntry& rhs )
    {
        if( lhs.m_IsDirectory != rhs.m_IsDirectory )
        {
            return lhs.m_IsDirectory > rhs.m_IsDirectory;
        }

        int comparison = 0;
        switch( column )
        {
            case HistorySortColumn::Name:
                comparison = CompareStringsCaseInsensitive( lhs.m_Label, rhs.m_Label );
                break;

            case HistorySortColumn::Size:
                comparison = ( lhs.m_SizeBytes < rhs.m_SizeBytes ) ? -1
                           : ( lhs.m_SizeBytes > rhs.m_SizeBytes ) ?  1 : 0;
                break;

            case HistorySortColumn::Type:
                comparison = CompareStringsCaseInsensitive( lhs.m_TypeLabel, rhs.m_TypeLabel );
                break;

            case HistorySortColumn::Created:
                comparison = ( lhs.m_CreationTicks < rhs.m_CreationTicks ) ? -1
                           : ( lhs.m_CreationTicks > rhs.m_CreationTicks ) ?  1 : 0;
                break;
        }

        if( !ascending )
        {
            comparison = -comparison;
        }

        if( comparison == 0 )
        {
            comparison = CompareStringsCaseInsensitive( lhs.m_Label, rhs.m_Label );
        }

        if( comparison == 0 )
        {
            comparison = CompareStringsCaseInsensitive( lhs.m_PathString, rhs.m_PathString );
        }

        return comparison < 0;
    };
}
