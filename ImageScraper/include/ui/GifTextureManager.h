#pragma once

#include "imgui/imgui_impl_opengl3_loader.h"

#include <vector>

namespace ImageScraper
{
    // Manages the OpenGL texture objects for a single decoded media item.
    // Handles allocation, bulk upload, in-place update, and deletion.
    class GifTextureManager
    {
    public:
        ~GifTextureManager( );

        // Upload contiguous RGBA pixel data for all GIF frames (one texture per frame).
        void Upload( const std::vector<unsigned char>& pixelData, int width, int height, int frameCount );

        // Allocate and upload a single texture (used for the first decoded video frame).
        void UploadSingle( const unsigned char* pixelData, int width, int height );

        // Replace the first texture's pixel data in-place via glTexSubImage2D.
        // Intended for per-frame video updates. No-op when no texture has been allocated.
        void UpdateTexture( const unsigned char* pixelData ) const;

        // Delete all textures and reset dimensions.
        void Free( );

        bool   IsEmpty( )              const;
        int    TextureCount( )         const;
        GLuint FrameTexture( int index ) const;
        int    Width( )                const;
        int    Height( )               const;

    private:
        std::vector<GLuint> m_Textures{ };
        int                 m_Width{ 0 };
        int                 m_Height{ 0 };
    };
}
