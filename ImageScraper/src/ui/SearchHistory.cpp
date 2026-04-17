#include "ui/SearchHistory.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::SearchHistory::Load( std::shared_ptr<JsonFile> appConfig, const std::string& configKey )
{
    m_AppConfig = std::move( appConfig );
    m_ConfigKey = configKey;

    if( !m_AppConfig )
    {
        return;
    }

    Json arr;
    if( !m_AppConfig->GetValue<Json>( m_ConfigKey, arr ) || !arr.is_array( ) )
    {
        return;
    }

    for( const auto& item : arr )
    {
        if( item.is_string( ) )
        {
            m_Items.push_back( item.get<std::string>( ) );
        }
    }

    if( static_cast<int>( m_Items.size( ) ) > k_MaxItems )
    {
        m_Items.resize( k_MaxItems );
    }
}

void ImageScraper::SearchHistory::Push( const std::string& value )
{
    if( value.empty( ) )
    {
        return;
    }

    auto it = std::find( m_Items.begin( ), m_Items.end( ), value );
    if( it != m_Items.end( ) )
    {
        m_Items.erase( it );
    }

    m_Items.insert( m_Items.begin( ), value );

    if( static_cast<int>( m_Items.size( ) ) > k_MaxItems )
    {
        m_Items.resize( k_MaxItems );
    }

    Save( );
}

std::string ImageScraper::SearchHistory::GetMostRecent( ) const
{
    if( m_Items.empty( ) )
    {
        return { };
    }

    return m_Items.front( );
}

const std::vector<std::string>& ImageScraper::SearchHistory::GetItems( ) const
{
    return m_Items;
}

bool ImageScraper::SearchHistory::IsEmpty( ) const
{
    return m_Items.empty( );
}

void ImageScraper::SearchHistory::Save( )
{
    if( !m_AppConfig )
    {
        return;
    }

    Json arr = Json::array( );
    for( const auto& item : m_Items )
    {
        arr.push_back( item );
    }

    m_AppConfig->SetValue<Json>( m_ConfigKey, arr );
    if( !m_AppConfig->Serialise( ) )
    {
        WarningLog( "[%s] Failed to save search history for key: %s", __FUNCTION__, m_ConfigKey.c_str( ) );
    }
}
