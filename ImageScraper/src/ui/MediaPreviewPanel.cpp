#define NOMINMAX
#include "ui/MediaPreviewPanel.h"
#include "async/TaskManager.h"
#include "log/Logger.h"
#include "stb/stb_image.h"

#include <fstream>
#include <algorithm>
#include <filesystem>

ImageScraper::MediaPreviewPanel::~MediaPreviewPanel( )
{
    FreeTextures( );
}

void ImageScraper::MediaPreviewPanel::Update( )
{
    // Upload any newly decoded image (fast — just GPU buffer copies, main thread only)
    {
        std::unique_ptr<DecodedImage> decoded;
        {
            std::lock_guard<std::mutex> lock( m_DecodedMutex );
            decoded = std::move( m_PendingDecoded );
        }
        if( decoded )
        {
            UploadDecoded( *decoded );
            m_LoadingFileName.clear( );
        }
    }

    // If a new path arrived and we're not already decoding, kick off a background decode
    KickDecodeIfNeeded( );

    ImGui::SetNextWindowSize( ImVec2( 480, 480 ), ImGuiCond_FirstUseEver );

    // Capture content-region screen coords for the text overlays
    ImVec2 contentScreenMin{ 0.0f, 0.0f };
    ImVec2 contentScreenMax{ 0.0f, 0.0f };
    bool   windowOpen = false;

    if( ImGui::Begin( "Media Preview", nullptr ) )
    {
        windowOpen = true;

        const ImVec2 winPos = ImGui::GetWindowPos( );
        contentScreenMin = ImVec2( winPos.x + ImGui::GetWindowContentRegionMin( ).x,
                                   winPos.y + ImGui::GetWindowContentRegionMin( ).y );
        contentScreenMax = ImVec2( winPos.x + ImGui::GetWindowContentRegionMax( ).x,
                                   winPos.y + ImGui::GetWindowContentRegionMax( ).y );

        if( m_Textures.empty( ) )
        {
            ImGui::TextDisabled( m_IsDecoding ? "Loading..." : "No media loaded yet" );
        }
        else
        {
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
                ImGui::SetCursorPos( ImVec2( imageCursor.x - offsetX, imageCursor.y - offsetY + availH ) );
                ImGui::Text( "Frame %d / %d", m_CurrentFrame + 1, static_cast<int>( m_Textures.size( ) ) );
            }
        }
    }
    ImGui::End( );

    // Text overlays drawn directly on the foreground (no window, no background — FPS-counter style)
    if( windowOpen )
    {
        constexpr float k_Pad = 6.0f;
        ImDrawList* dl = ImGui::GetForegroundDrawList( );
        const ImU32 colText    = ImGui::GetColorU32( ImGuiCol_Text );
        const ImU32 colDisabled = ImGui::GetColorU32( ImGuiCol_TextDisabled );

        // Top-left: name of the currently displayed file
        if( !m_CurrentFilePath.empty( ) )
        {
            const std::string name = std::filesystem::path( m_CurrentFilePath ).filename( ).string( );
            dl->AddText( ImVec2( contentScreenMin.x + k_Pad, contentScreenMin.y + k_Pad ),
                         colText, name.c_str( ) );
        }

        // Bottom-right: loading indicator
        if( m_IsDecoding )
        {
            const std::string msg = "Loading: " + m_LoadingFileName;
            const ImVec2 textSize = ImGui::CalcTextSize( msg.c_str( ) );
            dl->AddText(
                ImVec2( contentScreenMax.x - textSize.x - k_Pad,
                        contentScreenMax.y - textSize.y - k_Pad ),
                colDisabled, msg.c_str( ) );
        }
    }
}

void ImageScraper::MediaPreviewPanel::OnFileDownloaded( const std::string& filepath )
{
    std::lock_guard<std::mutex> lock( m_PathMutex );
    m_LatestPath    = filepath;
    m_HasLatestPath = true;
}

void ImageScraper::MediaPreviewPanel::KickDecodeIfNeeded( )
{
    if( m_IsDecoding )
    {
        return;
    }

    std::string filepath;
    {
        std::lock_guard<std::mutex> lock( m_PathMutex );
        if( !m_HasLatestPath )
        {
            return;
        }
        filepath        = m_LatestPath;
        m_HasLatestPath = false;
    }

    m_LoadingFileName = std::filesystem::path( filepath ).filename( ).string( );
    m_IsDecoding = true;

    TaskManager::Instance( ).Submit( TaskManager::s_NetworkContext, [ this, filepath ]( )
    {
        auto decoded = DecodeFile( filepath );

        {
            std::lock_guard<std::mutex> lock( m_DecodedMutex );
            m_PendingDecoded = std::move( decoded );
        }

        m_IsDecoding = false;
    } );
}

void ImageScraper::MediaPreviewPanel::UploadDecoded( const DecodedImage& decoded )
{
    FreeTextures( );

    if( decoded.m_PixelData.empty( ) )
    {
        return;
    }

    m_CurrentFilePath = decoded.m_FilePath;
    m_Width           = decoded.m_Width;
    m_Height          = decoded.m_Height;
    m_CurrentFrame    = 0;
    m_FrameAccumMs    = 0.0f;
    m_FrameDelaysMs   = decoded.m_FrameDelaysMs;

    const int frameBytes = decoded.m_Width * decoded.m_Height * 4;

    m_Textures.reserve( static_cast<size_t>( decoded.m_Frames ) );
    for( int i = 0; i < decoded.m_Frames; ++i )
    {
        GLuint textureId = 0;
        glGenTextures( 1, &textureId );
        glBindTexture( GL_TEXTURE_2D, textureId );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
                      decoded.m_Width, decoded.m_Height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE,
                      decoded.m_PixelData.data( ) + i * frameBytes );
        m_Textures.push_back( textureId );
    }
}

void ImageScraper::MediaPreviewPanel::FreeTextures( )
{
    if( !m_Textures.empty( ) )
    {
        glDeleteTextures( static_cast<GLsizei>( m_Textures.size( ) ), m_Textures.data( ) );
        m_Textures.clear( );
    }
    m_FrameDelaysMs.clear( );
    m_Width        = 0;
    m_Height       = 0;
    m_CurrentFrame = 0;
    m_FrameAccumMs = 0.0f;
}

// static
std::unique_ptr<ImageScraper::MediaPreviewPanel::DecodedImage>
ImageScraper::MediaPreviewPanel::DecodeFile( const std::string& filepath )
{
    // Read file into memory
    std::ifstream file( filepath, std::ios::binary | std::ios::ate );
    if( !file.is_open( ) )
    {
        ErrorLog( "[%s] Could not open file for preview: %s", __FUNCTION__, filepath.c_str( ) );
        return nullptr;
    }

    const std::streamsize size = file.tellg( );
    file.seekg( 0, std::ios::beg );
    std::vector<unsigned char> fileData( static_cast<size_t>( size ) );
    if( !file.read( reinterpret_cast<char*>( fileData.data( ) ), size ) )
    {
        ErrorLog( "[%s] Could not read file for preview: %s", __FUNCTION__, filepath.c_str( ) );
        return nullptr;
    }

    auto decoded       = std::make_unique<DecodedImage>( );
    decoded->m_FilePath = filepath;

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
            return nullptr;
        }

        decoded->m_Width  = width;
        decoded->m_Height = height;
        decoded->m_Frames = frames;

        const int frameBytes = width * height * 4;
        decoded->m_PixelData.assign( data, data + static_cast<size_t>( frames ) * frameBytes );

        decoded->m_FrameDelaysMs.reserve( static_cast<size_t>( frames ) );
        for( int i = 0; i < frames; ++i )
        {
            const int delayMs = delays ? delays[ i ] * 10 : 100;
            decoded->m_FrameDelaysMs.push_back( std::max( delayMs, 20 ) );
        }

        stbi_image_free( data );
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
            return nullptr;
        }

        decoded->m_Width  = width;
        decoded->m_Height = height;
        decoded->m_Frames = 1;
        decoded->m_PixelData.assign( data, data + static_cast<size_t>( width ) * height * 4 );

        stbi_image_free( data );
    }

    return decoded;
}

// static
bool ImageScraper::MediaPreviewPanel::IsGif( const std::string& filepath )
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
