#include "io/FileConverter.h"
#include "utils/DownloadUtils.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
}

const std::map<std::string, std::string> ImageScraper::FileConverter::s_ConversionMappings =
{
    { "gifv", "gif" },
    { "mp4", "gif" }
};

bool ImageScraper::FileConverter::TryConvert( const std::string& sourceFilePath, ConvertFlags convertFlags )
{
    for( const auto& [source, target] : s_ConversionMappings )
    {
        const std::string ext = DownloadHelpers::ExtractExtFromFile( sourceFilePath );

        if( ext != source )
        {
            continue;
        }

        const std::size_t dotPos = sourceFilePath.find_last_of( '.' );
        const std::string newFilePath = sourceFilePath.substr( 0, dotPos + 1 ) + target;

        // TODO Do conversion in a more generic way, have a map of file handler functions
        if( !convert_gifv_to_gif( sourceFilePath, newFilePath ) )
        {
            ErrorLog( "[%s] NOT IMPLEMENTED Skipped conversion: %s", __FUNCTION__, sourceFilePath.c_str( ) );
            return false;
        }

        InfoLog( "[%s] Convesion complete: %s", __FUNCTION__, newFilePath.c_str( ) );

        if( static_cast< uint8_t >( convertFlags ) & static_cast< uint8_t >( ConvertFlags::DeleteSource ) )
        {
            const int deleteResult = std::remove( sourceFilePath.c_str( ) );

            if( deleteResult == 0 )
            {
                InfoLog( "[%s] Source file deleted: %s" __FUNCTION__, sourceFilePath.c_str( ) );
            }
            else
            {
                WarningLog( "[%s] Failed to delete source file: %s" __FUNCTION__, sourceFilePath.c_str( ) );
            }
        }

        return true;
    }

    return false;
}

bool ImageScraper::FileConverter::convert_gifv_to_gif( const std::string& input_file, const std::string& output_file )
{
    return false;
    /*
    // Open the input file and find the video stream
    AVFormatContext* format_ctx = nullptr;

    if( int result = avformat_open_input( &format_ctx, input_file.c_str( ), nullptr, nullptr ); result < 0 )
    {
        char err_buf[ 256 ];
        av_make_error_string( err_buf, sizeof( err_buf ), result );
        std::cerr << "Could not open input file, error: " << err_buf << std::endl;
        return false;
    }

    if( avformat_find_stream_info( format_ctx, nullptr ) < 0 )
    {
        std::cerr << "Could not find stream information" << std::endl;
        return false;
    }

    int video_stream_index = -1;
    for( unsigned int i = 0; i < format_ctx->nb_streams; i++ )
    {
#pragma warning(push)
#pragma warning(disable: 26812)
        if( format_ctx->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO )
        {
#pragma warning(pop)
            video_stream_index = i;
            break;
        }
    }

    if( video_stream_index == -1 )
    {
        std::cerr << "Could not find a video stream" << std::endl;
        return false;
    }

    // Set up the output file and codec
    const AVOutputFormat* output_format = av_guess_format( "gif", nullptr, nullptr );
    AVFormatContext* output_format_ctx = nullptr;

    avformat_alloc_output_context2( &output_format_ctx, output_format, nullptr, output_file.c_str( ) );
    AVStream* output_stream = avformat_new_stream( output_format_ctx, nullptr );

    // Set the codec parameters
    const AVCodec* codec = avcodec_find_encoder( AV_CODEC_ID_GIF );
    AVCodecContext* codec_ctx = avcodec_alloc_context3( codec );

    AVCodecContext* input_codec_ctx = nullptr;
    const int input_codec_ctx_result = avcodec_parameters_to_context( input_codec_ctx, format_ctx->streams[ video_stream_index ]->codecpar );
    if ( input_codec_ctx_result < 0 )
    {
        char err_buf[ 256 ];
        av_make_error_string( err_buf, sizeof( err_buf ), input_codec_ctx_result );
        std::cerr << "Could create codec context, error: " << err_buf << std::endl;
        return false;
    }

    codec_ctx->width = input_codec_ctx->width;
    codec_ctx->height = input_codec_ctx->height;
    codec_ctx->time_base = input_codec_ctx->time_base;
#pragma warning(push)
#pragma warning(disable: 26812)
    codec_ctx->pix_fmt = AV_PIX_FMT_RGB8;
#pragma warning(pop)

    avcodec_open2( codec_ctx, codec, nullptr );
    avcodec_parameters_from_context( output_stream->codecpar, codec_ctx );
    output_stream->time_base = codec_ctx->time_base;

    // Open the output file
    if( avio_open( &output_format_ctx->pb, output_file.c_str( ), AVIO_FLAG_WRITE ) < 0 )
    {
        std::cerr << "Could not open output file" << std::endl;
        return false;
    }

    avformat_write_header( output_format_ctx, nullptr );

    // Set up a SwsContext to convert pixel formats
    SwsContext* sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, input_codec_ctx->pix_fmt,
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    // Prepare the input and output frames
    AVFrame* frame = av_frame_alloc( );
    AVFrame* frame_rgb8 = av_frame_alloc( );
    int num_bytes = av_image_get_buffer_size( codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, 1 );
    uint8_t* buffer = ( uint8_t* )av_malloc( num_bytes * sizeof( uint8_t ) );

    av_image_fill_arrays( frame_rgb8->data, frame_rgb8->linesize, buffer, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, 1 );

    // Read and convert the frames
    AVPacket packet;
    while( av_read_frame( format_ctx, &packet ) >= 0 )
    {
        if( packet.stream_index == video_stream_index )
        {
            int response = avcodec_send_packet( input_codec_ctx, &packet );
            if( response >= 0 )
            {
                while( response >= 0 )
                {
                    response = avcodec_receive_frame( input_codec_ctx, frame );
                    if( response == AVERROR( EAGAIN ) || response == AVERROR_EOF )
                    {
                        break;
                    }
                    else if( response < 0 )
                    {
                        std::cerr << "Error while receiving a frame" << std::endl;
                        return false;
                    }

                    sws_scale( sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, frame_rgb8->data, frame_rgb8->linesize );

                    frame_rgb8->pts = frame->pts;

                    avcodec_send_frame( codec_ctx, frame_rgb8 );
                    avcodec_receive_packet( codec_ctx, &packet );

                    av_interleaved_write_frame( output_format_ctx, &packet );
                }
            }
        }
        av_packet_unref( &packet );
    }

    // Write the trailer and close the output file
    av_write_trailer( output_format_ctx );
    avio_close( output_format_ctx->pb );

    // Free allocated resources
    av_frame_free( &frame );
    av_frame_free( &frame_rgb8 );
    av_free( buffer );
    sws_freeContext( sws_ctx );
    avcodec_free_context( &codec_ctx );
    avcodec_free_context( &input_codec_ctx );
    avformat_free_context( output_format_ctx );
    avformat_close_input( &format_ctx );

    return true;
    */
}
