#define NOMINMAX
#include "ui/MediaPreviewPanel.h"
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
    // Upload any newly decoded image (GPU buffer copies — main thread only)
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

    // Kick a fast first-frame decode when a new file arrives
    KickDecodeIfNeeded( );

    // Capture content-region screen coords before rendering (needed for foreground text)
    ImVec2 contentScreenMin{ 0.0f, 0.0f };
    ImVec2 contentScreenMax{ 0.0f, 0.0f };
    bool   windowOpen = false;
    bool   imageClicked = false;

    ImGui::SetNextWindowSize( ImVec2( 480, 480 ), ImGuiCond_FirstUseEver );

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
            ImGui::TextDisabled( "No media loaded yet" );
        }
        else
        {
            // Advance GIF frame only while playing
            if( m_GifState == GifState::Playing && m_Textures.size( ) > 1 )
            {
                m_FrameAccumMs += ImGui::GetIO( ).DeltaTime * 1000.0f;
                const int delayMs = m_FrameDelaysMs.empty( ) ? 100 : m_FrameDelaysMs[ m_CurrentFrame ];
                if( m_FrameAccumMs >= static_cast<float>( delayMs ) )
                {
                    m_FrameAccumMs -= static_cast<float>( delayMs );
                    m_CurrentFrame = ( m_CurrentFrame + 1 ) % static_cast<int>( m_Textures.size( ) );
                }
            }

            // Scale image to fill the full content region
            const ImVec2 avail = ImGui::GetContentRegionAvail( );
            float drawW = static_cast<float>( m_Width );
            float drawH = static_cast<float>( m_Height );

            if( drawW > avail.x )
            {
                drawH = drawH * ( avail.x / drawW );
                drawW = avail.x;
            }
            if( drawH > avail.y )
            {
                drawW = drawW * ( avail.y / drawH );
                drawH = avail.y;
            }

            // Centre the image in the available region
            const float offsetX = ( avail.x - drawW ) * 0.5f;
            const float offsetY = ( avail.y - drawH ) * 0.5f;
            ImGui::SetCursorPos( ImVec2( ImGui::GetCursorPosX( ) + offsetX, ImGui::GetCursorPosY( ) + offsetY ) );

            const GLuint tex = m_Textures[ m_CurrentFrame ];
            ImGui::Image( reinterpret_cast<ImTextureID>( static_cast<uintptr_t>( tex ) ), ImVec2( drawW, drawH ) );

            if( ImGui::IsItemClicked( ImGuiMouseButton_Left ) )
            {
                imageClicked = true;
            }

            if( ImGui::IsItemHovered( ) )
            {
                ImGui::SetTooltip( "%s", m_CurrentFilePath.c_str( ) );
            }
        }
    }
    ImGui::End( );

    // Handle click — toggle play/pause for GIFs
    if( imageClicked )
    {
        if( m_GifState == GifState::StaticFrame )
        {
            KickFullGifDecode( );
        }
        else if( m_GifState == GifState::Playing )
        {
            m_GifState     = GifState::StaticFrame;
            m_CurrentFrame = 0;
            m_FrameAccumMs = 0.0f;
        }
    }

    // Foreground badge overlays
    if( windowOpen )
    {
        constexpr float k_Pad      = 6.0f;
        constexpr float k_BadgePad = 3.0f;
        ImDrawList* dl             = ImGui::GetForegroundDrawList( );
        const ImU32 colText        = ImGui::GetColorU32( ImGuiCol_Text );
        const ImU32 colBadgeBg     = IM_COL32( 0, 0, 0, 140 );
        const float lineH          = ImGui::GetTextLineHeight( );

        auto DrawBadge = [ & ]( ImVec2 pos, const char* text )
        {
            const ImVec2 sz = ImGui::CalcTextSize( text );
            dl->AddRectFilled(
                ImVec2( pos.x - k_BadgePad, pos.y - k_BadgePad ),
                ImVec2( pos.x + sz.x + k_BadgePad, pos.y + sz.y + k_BadgePad ),
                colBadgeBg, 3.0f );
            dl->AddText( pos, colText, text );
        };

        const float badgeY = contentScreenMin.y + k_Pad;

        // Top-left: filename
        if( !m_CurrentFilePath.empty( ) )
        {
            const std::string name = std::filesystem::path( m_CurrentFilePath ).filename( ).string( );
            DrawBadge( ImVec2( contentScreenMin.x + k_Pad, badgeY ), name.c_str( ) );
        }

        // Top-right: context-sensitive status
        std::string statusMsg;
        if( m_IsDecoding && m_GifState == GifState::LoadingFullFrames )
        {
            statusMsg = "Loading: " + m_LoadingFileName;
        }
        else if( m_GifState == GifState::StaticFrame )
        {
            statusMsg = "Click to play";
        }
        else if( m_GifState == GifState::Playing )
        {
            statusMsg = "Click to pause";
        }

        if( !statusMsg.empty( ) )
        {
            const ImVec2 sz = ImGui::CalcTextSize( statusMsg.c_str( ) );
            DrawBadge( ImVec2( contentScreenMax.x - sz.x - k_Pad, badgeY ), statusMsg.c_str( ) );
        }

        // Bottom-left: frame counter while playing
        if( m_GifState == GifState::Playing && m_Textures.size( ) > 1 )
        {
            char buf[ 32 ];
            snprintf( buf, sizeof( buf ), "Frame %d / %d", m_CurrentFrame + 1, static_cast<int>( m_Textures.size( ) ) );
            DrawBadge( ImVec2( contentScreenMin.x + k_Pad, contentScreenMax.y - lineH - k_Pad - k_BadgePad * 2 ), buf );
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
    m_IsDecoding      = true;

    // Always decode only the first frame — fast for both images and GIFs
    m_DecodeFuture = std::async( std::launch::async, [ this, filepath ]( )
    {
        auto decoded = DecodeFile( filepath, true );

        {
            std::lock_guard<std::mutex> lock( m_DecodedMutex );
            m_PendingDecoded = std::move( decoded );
        }

        m_IsDecoding = false;
    } );
}

void ImageScraper::MediaPreviewPanel::KickFullGifDecode( )
{
    if( m_IsDecoding )
    {
        return;
    }

    m_GifState        = GifState::LoadingFullFrames;
    m_LoadingFileName = std::filesystem::path( m_CurrentFilePath ).filename( ).string( );
    m_IsDecoding      = true;

    const std::string filepath = m_CurrentFilePath;

    m_DecodeFuture = std::async( std::launch::async, [ this, filepath ]( )
    {
        auto decoded = DecodeFile( filepath, false );

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
        m_GifState = GifState::None;
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

    if( decoded.m_Frames > 1 )
    {
        m_GifState = GifState::Playing;
    }
    else if( IsGif( decoded.m_FilePath ) )
    {
        m_GifState = GifState::StaticFrame;
    }
    else
    {
        m_GifState = GifState::None;
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
ImageScraper::MediaPreviewPanel::DecodeFile( const std::string& filepath, bool firstFrameOnly )
{
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

    auto decoded        = std::make_unique<DecodedImage>( );
    decoded->m_FilePath = filepath;

    // Full GIF decode — all frames with delays
    if( !firstFrameOnly && IsGif( filepath ) )
    {
        int* delays   = nullptr;
        int  width    = 0;
        int  height   = 0;
        int  frames   = 0;
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
        if( delays ) { free( delays ); }
    }
    else
    {
        // Single frame — covers static images and first-frame-only GIF preview
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
    if( dot == std::string::npos ) { return false; }
    std::string ext = filepath.substr( dot + 1 );
    std::transform( ext.begin( ), ext.end( ), ext.begin( ),
        []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return ext == "gif";
}
