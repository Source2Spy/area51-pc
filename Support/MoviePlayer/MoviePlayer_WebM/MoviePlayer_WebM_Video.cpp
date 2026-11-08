//==============================================================================
//
//  MoviePlayer_WebM_Video.cpp
//
//  Video decoding for WebM playback using libvpx.
//
//==============================================================================

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
// VIDEO DECODER HELPER IMPLEMENTATION
//==============================================================================

static 
inline u8 ClampByte(s32 Value)
{
    if (Value < 0)   return 0;
    if (Value > 255) return 255;
    return (u8)Value;
}

//==============================================================================
// VIDEO DECODER IMPLEMENTATION
//==============================================================================

video_decoder::video_decoder(void)
{
    x_memset(&m_CodecCtx, 0, sizeof(m_CodecCtx));
    m_pCodecIface      = NULL;
    m_bCodecInitialized = FALSE;
    m_FramePitch       = 0;
    m_Width            = 0;
    m_Height           = 0;
}

//==============================================================================

video_decoder::~video_decoder(void)
{
    Shutdown();
}

//==============================================================================

xbool video_decoder::Initialize(const player_config& Config)
{
    if ((Config.Width <= 0) || (Config.Width > 8192) ||
        (Config.Height <= 0) || (Config.Height > 8192))
        return FALSE;

    m_Width      = Config.Width;
    m_Height     = Config.Height;
    m_FramePitch = m_Width * 4;

    return InitializeCodec(Config);
}

//==============================================================================

void video_decoder::Shutdown(void)
{
    DestroyCodec();
    m_CompressedBuffer.Clear();
    m_FrameBuffer.Clear();
    m_FramePitch = 0;
    m_Width      = 0;
    m_Height     = 0;
}

//==============================================================================

xbool video_decoder::InitializeCodec(const player_config& Config)
{
    xstring CodecId = Config.VideoCodecId;
    CodecId.MakeUpper();

    if (CodecId.Find("VP9") != -1)
    {
        m_pCodecIface = vpx_codec_vp9_dx();
    }
    else if (CodecId.Find("VP8") != -1) 
    {
        m_pCodecIface = vpx_codec_vp8_dx();
    }
    else
    {
        x_DebugMsg("MoviePlayer_WebM: Unsupported video codec '%s'\n", (const char*)CodecId);
        return FALSE;
    }    

    if (!m_pCodecIface)
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to get codec interface for '%s'\n", (const char*)CodecId);
        return FALSE;
    }

    vpx_codec_err_t Err = vpx_codec_dec_init(&m_CodecCtx, m_pCodecIface, NULL, 0);
    if (Err != VPX_CODEC_OK)
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
    if (m_bCodecInitialized)
    {
        vpx_codec_destroy(&m_CodecCtx);
        x_memset(&m_CodecCtx, 0, sizeof(m_CodecCtx));
        m_bCodecInitialized = FALSE;
    }

    m_pCodecIface = NULL;
}

//==============================================================================

decode_status video_decoder::DecodeSample(const sample& Sample, mkvparser::IMkvReader* pReader, xbool ConvertImage)
{
    ASSERT(m_bCodecInitialized);
    ASSERT(Sample.pBlock);
    ASSERT(pReader);        
    
    if (!m_bCodecInitialized || !Sample.pBlock || !pReader)
        return DECODE_ERROR;

    const mkvparser::Block* pBlock = Sample.pBlock;
    xbool hasImage = FALSE;
    if ((Sample.FrameIndex < 0) || (Sample.FrameIndex >= pBlock->GetFrameCount()))
        return DECODE_ERROR;

    const mkvparser::Block::Frame& Frame = pBlock->GetFrame(Sample.FrameIndex);
    const s32 frameSize = (s32)Frame.len;

    if ((Frame.len <= 0) || (Frame.len > INT_MAX - INT_MAX / 4 - 4096))
        return DECODE_ERROR;

    if (frameSize > m_CompressedBuffer.GetCapacity())
    {
        const s32 newCapacity = frameSize + (frameSize / 4) + 4096;
        m_CompressedBuffer.SetCapacity(newCapacity);
    }

    m_CompressedBuffer.SetCount(frameSize);
    if (Frame.Read(pReader, m_CompressedBuffer.GetPtr()) < 0)
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to read video frame.\n");
        return DECODE_ERROR;
    }

    const vpx_codec_err_t Err = vpx_codec_decode(&m_CodecCtx, m_CompressedBuffer.GetPtr(), frameSize, NULL, 0);
    if (Err != VPX_CODEC_OK)
    {
        x_DebugMsg("MoviePlayer_WebM: vpx decode error %d (%s): %s\n",
                   (s32)Err, vpx_codec_err_to_string(Err),
                   vpx_codec_error_detail(&m_CodecCtx) ? vpx_codec_error_detail(&m_CodecCtx) : "");
        return DECODE_ERROR;
    }

    vpx_codec_iter_t Iter = NULL;
    const vpx_image_t* pImage;
    while ((pImage = vpx_codec_get_frame(&m_CodecCtx, &Iter)) != NULL)
    {
        hasImage = TRUE;
        if (ConvertImage && !ConvertFrame(*pImage))
            return DECODE_ERROR;
    }

    if (!hasImage)
        return DECODE_EMPTY;
    return ConvertImage ? DECODE_FRAME : DECODE_DISCARDED;
}

//==============================================================================

xbool video_decoder::ConvertFrame(const vpx_image_t& Image)
{
    if (Image.fmt != VPX_IMG_FMT_I420)
    {
        x_DebugMsg("MoviePlayer_WebM: Unsupported VPX image format %d\n", Image.fmt);
        return FALSE;
    }

    const s32 Width  = (s32)Image.d_w;
    const s32 Height = (s32)Image.d_h;

    if ((Width <= 0) || (Height <= 0))
        return FALSE;

    if ((Width > 8192) || (Height > 8192))  // 8K resolution
        return FALSE;

    m_Width      = Width;
    m_Height     = Height;
    m_FramePitch = m_Width * 4;

    const s32 RequiredSize = m_FramePitch * m_Height;
    m_FrameBuffer.SetCount(RequiredSize);

    const u8* pY = Image.planes[VPX_PLANE_Y];
    const u8* pU = Image.planes[VPX_PLANE_U];
    const u8* pV = Image.planes[VPX_PLANE_V];

    const s32 strideY = Image.stride[VPX_PLANE_Y];
    const s32 strideU = Image.stride[VPX_PLANE_U];
    const s32 strideV = Image.stride[VPX_PLANE_V];

    u8* pDest = m_FrameBuffer.GetPtr();

    for (s32 y = 0; y < m_Height; ++y)
    {
        const u8* rowY = pY + y * strideY;
        const u8* rowU = pU + (y / 2) * strideU;
        const u8* rowV = pV + (y / 2) * strideV;
        u8* rowDst = pDest + y * m_FramePitch;

        for (s32 x = 0; x < m_Width; ++x)
        {
            const s32 Y = rowY[x];
            const s32 U = rowU[x / 2];
            const s32 V = rowV[x / 2];

            const s32 C = Y - 16;
            const s32 D = U - 128;
            const s32 E = V - 128;

            s32 R = (298 * C + 409 * E + 128) >> 8;
            s32 G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            s32 B = (298 * C + 516 * D + 128) >> 8;

            rowDst[x * 4 + 0] = ClampByte(B);
            rowDst[x * 4 + 1] = ClampByte(G);
            rowDst[x * 4 + 2] = ClampByte(R);
            rowDst[x * 4 + 3] = 255;
        }
    }

    return TRUE;
}

//==============================================================================
