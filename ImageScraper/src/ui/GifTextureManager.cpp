#define NOMINMAX
#include "ui/GifTextureManager.h"

ImageScraper::GifTextureManager::~GifTextureManager( )
{
    Free( );
}

void ImageScraper::GifTextureManager::Upload( const std::vector<unsigned char>& pixelData, int width, int height, int frameCount )
{
    Free( );
    m_Width  = width;
    m_Height = height;

    const int frameBytes = width * height * 4;
    m_Textures.reserve( static_cast<size_t>( frameCount ) );

    for( int i = 0; i < frameCount; ++i )
    {
        GLuint textureId = 0;
        glGenTextures( 1, &textureId );
        glBindTexture( GL_TEXTURE_2D, textureId );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
                      width, height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE,
                      pixelData.data( ) + i * frameBytes );
        m_Textures.push_back( textureId );
    }

    glBindTexture( GL_TEXTURE_2D, 0 );
}

void ImageScraper::GifTextureManager::UploadSingle( const unsigned char* pixelData, int width, int height )
{
    Free( );
    m_Width  = width;
    m_Height = height;

    GLuint textureId = 0;
    glGenTextures( 1, &textureId );
    glBindTexture( GL_TEXTURE_2D, textureId );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
                  width, height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE,
                  pixelData );
    glBindTexture( GL_TEXTURE_2D, 0 );
    m_Textures.push_back( textureId );
}

void ImageScraper::GifTextureManager::UpdateTexture( const unsigned char* pixelData ) const
{
    if( m_Textures.empty( ) )
    {
        return;
    }

    glBindTexture( GL_TEXTURE_2D, m_Textures[ 0 ] );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, pixelData );
    glBindTexture( GL_TEXTURE_2D, 0 );
}

void ImageScraper::GifTextureManager::Free( )
{
    if( !m_Textures.empty( ) )
    {
        glDeleteTextures( static_cast<GLsizei>( m_Textures.size( ) ), m_Textures.data( ) );
        m_Textures.clear( );
    }

    m_Width  = 0;
    m_Height = 0;
}

bool ImageScraper::GifTextureManager::IsEmpty( ) const
{
    return m_Textures.empty( );
}

int ImageScraper::GifTextureManager::TextureCount( ) const
{
    return static_cast<int>( m_Textures.size( ) );
}

GLuint ImageScraper::GifTextureManager::FrameTexture( int index ) const
{
    return m_Textures[ static_cast<size_t>( index ) ];
}

int ImageScraper::GifTextureManager::Width( ) const
{
    return m_Width;
}

int ImageScraper::GifTextureManager::Height( ) const
{
    return m_Height;
}
