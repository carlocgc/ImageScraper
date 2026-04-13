#define NOMINMAX
#include "ui/MediaPreviewPanel.h"
#include "log/Logger.h"
#include "stb/stb_image.h"

#include <fstream>
#include <algorithm>

ImageScraper::MediaPreviewPanel::~MediaPreviewPanel( )
{
    FreeTextures( );
}

void ImageScraper::MediaPreviewPanel::Update( )
{
    LoadPending( );

    ImGui::SetNextWindowSize( ImVec2( 480, 480 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Media Preview", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    if( m_Textures.empty( ) )
    {
        ImGui::TextDisabled( "No media loaded yet" );
        ImGui::End( );
        return;
    }

    // Advance GIF frame
    if( m_Textures.size( ) > 1 )
    {
        m_FrameAccumMs += ImGui::GetIO( ).DeltaTime * 1000.0f;
        const int delayMs = m_FrameDelaysMs.empty( ) ? 100 : m_FrameDelaysMs[ m_CurrentFrame ];
        if( m_FrameAccumMs >= static_cast<float>( delayMs ) )
        {
            m_FrameAccumMs -= static_cast<float>( delayMs );
            m_CurrentFrame = ( m_CurrentFrame + 1 ) % static_cast<int>( m_Textures.size( ) );
        }
    }

    // Reserve a line at the bottom for the GIF frame counter
    const bool isGif = m_Textures.size( ) > 1;
    const float counterH = isGif ? ImGui::GetTextLineHeightWithSpacing( ) : 0.0f;

    // Scale image to fit the available content region while preserving aspect ratio
    const ImVec2 avail = ImGui::GetContentRegionAvail( );
    float drawW = static_cast<float>( m_Width );
    float drawH = static_cast<float>( m_Height );
    const float availH = avail.y - counterH;

    if( drawW > avail.x )
    {
        drawH = drawH * ( avail.x / drawW );
        drawW = avail.x;
    }
    if( drawH > availH )
    {
        drawW = drawW * ( availH / drawH );
        drawH = availH;
    }

    // Centre the image in the available region
    const float offsetX = ( avail.x - drawW ) * 0.5f;
    const float offsetY = ( availH - drawH ) * 0.5f;
    const ImVec2 imageCursor{ ImGui::GetCursorPosX( ) + offsetX, ImGui::GetCursorPosY( ) + offsetY };
    ImGui::SetCursorPos( imageCursor );

    const GLuint tex = m_Textures[ m_CurrentFrame ];
    ImGui::Image( reinterpret_cast<ImTextureID>( static_cast<uintptr_t>( tex ) ), ImVec2( drawW, drawH ) );

    if( ImGui::IsItemHovered( ) )
    {
        ImGui::SetTooltip( "%s", m_CurrentFilePath.c_str( ) );
    }

    if( isGif )
    {
        // Pin the counter to the bottom-left of the content area
        ImGui::SetCursorPos( ImVec2( imageCursor.x - offsetX, imageCursor.y - offsetY + availH ) );
        ImGui::Text( "Frame %d / %d", m_CurrentFrame + 1, static_cast<int>( m_Textures.size( ) ) );
    }

    ImGui::End( );
}

void ImageScraper::MediaPreviewPanel::OnFileDownloaded( const std::string& filepath )
{
    std::lock_guard<std::mutex> lock( m_PendingMutex );
    m_PendingFilePath = filepath;
    m_HasPending      = true;
}

void ImageScraper::MediaPreviewPanel::LoadPending( )
{
    std::string filepath;
    {
        std::lock_guard<std::mutex> lock( m_PendingMutex );
        if( !m_HasPending )
        {
            return;
        }
        filepath    = m_PendingFilePath;
        m_HasPending = false;
    }

    // Read file into memory
    std::ifstream file( filepath, std::ios::binary | std::ios::ate );
    if( !file.is_open( ) )
    {
        ErrorLog( "[%s] Could not open file for preview: %s", __FUNCTION__, filepath.c_str( ) );
        return;
    }

    const std::streamsize size = file.tellg( );
    file.seekg( 0, std::ios::beg );
    std::vector<unsigned char> fileData( static_cast<size_t>( size ) );
    if( !file.read( reinterpret_cast<char*>( fileData.data( ) ), size ) )
    {
        ErrorLog( "[%s] Could not read file for preview: %s", __FUNCTION__, filepath.c_str( ) );
        return;
    }

    FreeTextures( );
    m_CurrentFrame  = 0;
    m_FrameAccumMs  = 0.0f;
    m_CurrentFilePath = filepath;

    if( IsGif( filepath ) )
    {
        int* delays  = nullptr;
        int  width   = 0;
        int  height  = 0;
        int  frames  = 0;
        int  channels = 0;

        unsigned char* data = stbi_load_gif_from_memory(
            fileData.data( ), static_cast<int>( fileData.size( ) ),
            &delays, &width, &height, &frames, &channels, 4 );

        if( !data )
        {
            ErrorLog( "[%s] stbi_load_gif_from_memory failed for: %s", __FUNCTION__, filepath.c_str( ) );
            return;
        }

        LoadGif( data, width, height, frames, delays );
        stbi_image_free( data );
        // delays is allocated by stbi and must be freed separately
        if( delays )
        {
            free( delays );
        }
    }
    else
    {
        int width = 0, height = 0, channels = 0;
        unsigned char* data = stbi_load_from_memory(
            fileData.data( ), static_cast<int>( fileData.size( ) ),
            &width, &height, &channels, 4 );

        if( !data )
        {
            DebugLog( "[%s] stbi_load_from_memory failed (unsupported format?) for: %s", __FUNCTION__, filepath.c_str( ) );
            return;
        }

        LoadStaticImage( data, width, height );
        stbi_image_free( data );
    }
}

void ImageScraper::MediaPreviewPanel::LoadStaticImage( const unsigned char* data, int width, int height )
{
    GLuint textureId = 0;
    glGenTextures( 1, &textureId );
    glBindTexture( GL_TEXTURE_2D, textureId );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );

    m_Textures.push_back( textureId );
    m_Width  = width;
    m_Height = height;
}

void ImageScraper::MediaPreviewPanel::LoadGif( const unsigned char* data, int width, int height, int frames, const int* delays )
{
    const int frameBytes = width * height * 4;

    m_Textures.reserve( static_cast<size_t>( frames ) );
    m_FrameDelaysMs.reserve( static_cast<size_t>( frames ) );

    for( int i = 0; i < frames; ++i )
    {
        GLuint textureId = 0;
        glGenTextures( 1, &textureId );
        glBindTexture( GL_TEXTURE_2D, textureId );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data + i * frameBytes );

        m_Textures.push_back( textureId );

        // stbi GIF delays are in centiseconds; convert to milliseconds
        const int delayMs = delays ? delays[ i ] * 10 : 100;
        m_FrameDelaysMs.push_back( std::max( delayMs, 20 ) );  // floor at 20ms (~50fps)
    }

    m_Width  = width;
    m_Height = height;
}

void ImageScraper::MediaPreviewPanel::FreeTextures( )
{
    if( !m_Textures.empty( ) )
    {
        glDeleteTextures( static_cast<GLsizei>( m_Textures.size( ) ), m_Textures.data( ) );
        m_Textures.clear( );
    }
    m_FrameDelaysMs.clear( );
    m_Width         = 0;
    m_Height        = 0;
    m_CurrentFrame  = 0;
    m_FrameAccumMs  = 0.0f;
}

bool ImageScraper::MediaPreviewPanel::IsGif( const std::string& filepath ) const
{
    const auto dot = filepath.rfind( '.' );
    if( dot == std::string::npos )
    {
        return false;
    }
    std::string ext = filepath.substr( dot + 1 );
    std::transform( ext.begin( ), ext.end( ), ext.begin( ),
        []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return ext == "gif";
}
