#include "ui/ProviderSearchInput.h"

void ImageScraper::ProviderSearchInput::Draw( const ProviderSearchInputConfig& config, std::string& value, const SearchHistory& history )
{
    DownloadOptionSearchConfig optionConfig{ };
    optionConfig.m_RowId = config.m_ChildId;
    optionConfig.m_InputId = config.m_InputId;
    optionConfig.m_HistoryButtonId = config.m_HistoryButtonId;
    optionConfig.m_HistoryPopupId = config.m_HistoryPopupId;
    optionConfig.m_Label = config.m_Label;
    optionConfig.m_InputFlags = config.m_InputFlags;
    DownloadOptionControls::DrawSearchInput( optionConfig, value, history );
}
