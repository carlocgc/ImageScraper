#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace ImageScraper
{
    // Column identifiers mirror the ImGuiID values assigned to each table column
    // in DownloadHistoryPanel, so they can be cast to/from ImGuiID safely.
    enum class HistorySortColumn : unsigned int
    {
        Name    = 0,
        Size    = 1,
        Type    = 2,
        Created = 3,
    };

    // The fields required to order a download history entry.
    // DownloadHistoryPanel::TreeNodeSnapshot inherits from this struct so that
    // MakeHistoryComparator can be applied directly to the sorted vector.
    struct HistorySortEntry
    {
        bool               m_IsDirectory{ false };
        std::string        m_Label{ };
        uintmax_t          m_SizeBytes{ 0 };
        std::string        m_TypeLabel{ };
        unsigned long long m_CreationTicks{ 0 };
        std::string        m_PathString{ };
    };

    // Returns a strict-weak-ordering comparator for HistorySortEntry objects.
    // Directories always sort before files. Within each group, entries are
    // ordered by the specified column; equal entries fall back to m_Label then
    // m_PathString. All string comparisons are case-insensitive.
    std::function<bool( const HistorySortEntry&, const HistorySortEntry& )>
        MakeHistoryComparator( HistorySortColumn column, bool ascending );
}
