//==============================================================================
//
//  MoviePlayer_WebM_Video.cpp
//
//  Video decoding for WebM playback using libvpx.
//
//==============================================================================

// TODO: GPU decoding.

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#if !defined( TARGET_DESKTOP ) && !defined( TARGET_MOBILE )
#error This file should only be compiled for desktop or mobile platforms. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "x_memory.hpp"

#include "MoviePlayer_WebM_Private.hpp"

using namespace movie_webm;

//==============================================================================
// VIDEO DECODER IMPLEMENTATION
//==============================================================================

video_decoder::video_decoder(void)
{
    x_memset(&m_CodecCtx, 0, sizeof(m_CodecCtx));

    m_pCodecIface      = NULL;
    m_bCodecInitialized = FALSE;
    m_pDecodedImage    = NULL;
}

//==============================================================================

video_decoder::~video_decoder(void)
{
    Shutdown();
}

//==============================================================================

xbool video_decoder::Initialize(const player_config& Config)
{
    if( (Config.Width <= 0) || (Config.Width > 8192) ||
        (Config.Height <= 0) || (Config.Height > 8192) )
    {
        return FALSE;
    }

    return InitializeCodec(Config);
}

//==============================================================================

void video_decoder::Shutdown(void)
{
    DestroyCodec();
    m_CompressedBuffer.Clear();
}

//==============================================================================

xbool video_decoder::InitializeCodec(const player_config& Config)
{
    xstring CodecId = Config.VideoCodecId;
    CodecId.MakeUpper();

    if( CodecId.Find("VP9") != -1 )
    {
        m_pCodecIface = vpx_codec_vp9_dx();
    }
    else if( CodecId.Find("VP8") != -1 )
    {
        m_pCodecIface = vpx_codec_vp8_dx();
    }
    else
    {
        x_DebugMsg("MoviePlayer_WebM: Unsupported video codec '%s'\n", (const char*)CodecId);
        return FALSE;
    }

    if( !m_pCodecIface )
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to get codec interface for '%s'\n", (const char*)CodecId);
        return FALSE;
    }

    vpx_codec_err_t Err = vpx_codec_dec_init(&m_CodecCtx, m_pCodecIface, NULL, 0);
    if( Err != VPX_CODEC_OK )
    {
        x_DebugMsg("MoviePlayer_WebM: vpx_codec_dec_init failed (%s)\n", vpx_codec_err_to_string(Err));
        x_memset(&m_CodecCtx, 0, sizeof(m_CodecCtx));
        m_pCodecIface = NULL;
        return FALSE;
    }

    m_bCodecInitialized = TRUE;
    return TRUE;
}

//==============================================================================

void video_decoder::DestroyCodec(void)
{
    if( m_bCodecInitialized )
    {
        vpx_codec_destroy(&m_CodecCtx);
        x_memset(&m_CodecCtx, 0, sizeof(m_CodecCtx));
        m_bCodecInitialized = FALSE;
    }

    m_pCodecIface = NULL;
    m_pDecodedImage = NULL;
}

//==============================================================================

decode_status video_decoder::DecodeSample(const sample& Sample, mkvparser::IMkvReader* pReader)
{
    m_pDecodedImage = NULL;
    ASSERT(m_bCodecInitialized);
    ASSERT(Sample.pBlock);
    ASSERT(pReader);

    if( !m_bCodecInitialized || !Sample.pBlock || !pReader )
    {
        return DECODE_ERROR;
    }

    const mkvparser::Block* pBlock = Sample.pBlock;
    if( (Sample.FrameIndex < 0) || (Sample.FrameIndex >= pBlock->GetFrameCount()) )
    {
        return DECODE_ERROR;
    }

    const mkvparser::Block::Frame& Frame = pBlock->GetFrame(Sample.FrameIndex);
    const s32 frameSize = (s32)Frame.len;

    if( (Frame.len <= 0) || (Frame.len > S32_MAX - S32_MAX / 4 - 4096) )
    {
        return DECODE_ERROR;
    }

    if( frameSize > m_CompressedBuffer.GetCapacity() )
    {
        const s32 newCapacity = frameSize + (frameSize / 4) + 4096;
        m_CompressedBuffer.SetCapacity(newCapacity);
    }

    m_CompressedBuffer.SetCount(frameSize);
    if( Frame.Read(pReader, m_CompressedBuffer.GetPtr()) < 0 )
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to read video frame.\n");
        return DECODE_ERROR;
    }

    const vpx_codec_err_t Err = vpx_codec_decode(&m_CodecCtx, m_CompressedBuffer.GetPtr(), frameSize, NULL, 0);
    if( Err != VPX_CODEC_OK )
    {
        x_DebugMsg("MoviePlayer_WebM: vpx decode error %d (%s): %s\n",
                   (s32)Err, vpx_codec_err_to_string(Err),
                   vpx_codec_error_detail(&m_CodecCtx) ? vpx_codec_error_detail(&m_CodecCtx) : "");
        return DECODE_ERROR;
    }

    vpx_codec_iter_t Iter = NULL;
    const vpx_image_t* pImage;
    while( (pImage = vpx_codec_get_frame(&m_CodecCtx, &Iter)) != NULL )
    {
        m_pDecodedImage = pImage;
    }

    if( !m_pDecodedImage )
    {
        return DECODE_EMPTY;
    }

    if( m_pDecodedImage->fmt != VPX_IMG_FMT_I420 )
    {
        x_DebugMsg("MoviePlayer_WebM: Unsupported VPX image format %d\n", m_pDecodedImage->fmt);
        return DECODE_ERROR;
    }

    if( !m_pDecodedImage->d_w || (m_pDecodedImage->d_w > 8192) ||
        !m_pDecodedImage->d_h || (m_pDecodedImage->d_h > 8192) )
    {
        return DECODE_ERROR;
    }

    return DECODE_FRAME;
}

//==============================================================================
