//==============================================================================
//
//  MoviePlayer_WebM.cpp
//
//  Public entry points for the WebM movie backend.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

// Let it be only for PC, for now...

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include <d3d11.h>

#include "x_files.hpp"
#include "x_threads.hpp"
#include "x_memory.hpp"
#include "Entropy.hpp"
#include "../movieplayer.hpp"
#include "MoviePlayer_WebM_Private.hpp"

// Auto include WebM libs
#ifdef _MSC_VER
#pragma comment( lib, "ogg.lib" )
#pragma comment( lib, "opus.lib" )
#pragma comment( lib, "opusfile.lib" )
#pragma comment( lib, "vorbis.lib" )
#pragma comment( lib, "vorbisenc.lib" )
#pragma comment( lib, "vorbisfile.lib" )
#pragma comment( lib, "vpx.lib" )
#pragma comment( lib, "webm.lib" )
#endif

//==============================================================================
// CONSTANTS
//==============================================================================

namespace
{
    static const f64 VIDEO_PRESENT_LEAD = 0.005;
    static const f64 AUDIO_BUFFER_LEAD  = 0.25;
}

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

movie_player Movie;

//==============================================================================
// MOVIE PRIVATE IMPLEMENTATION
//==============================================================================

movie_private::movie_private(void)
{
    m_Width             = 0;
    m_Height            = 0;
    m_Volume            = 1.0f;
    m_IsLooped          = FALSE;
    m_bForceStretch     = TRUE;

    m_pThread           = NULL;
    m_bThreadExit       = FALSE;
    m_bThreadRunning    = FALSE;
    m_bThreadFinished   = TRUE;
    m_bPaused           = FALSE;
    m_bPlaybackActive   = FALSE;
    m_bVideoEOF         = FALSE;
    m_bThreadBusy       = FALSE;

    m_pTexture          = NULL;
    m_TextureVRAMID     = 0;
    m_LastVideoTime     = 0.0f;
    m_bHasPendingVideo  = FALSE;
    m_PendingVideoSample = movie_webm::sample();
}

//==============================================================================

movie_private::~movie_private(void)
{
    Kill();
}

//==============================================================================

void movie_private::Init(void)
{
    if (m_pThread)
        return;

    m_bThreadExit     = FALSE;
    m_bThreadRunning  = FALSE;
    m_bThreadFinished = TRUE;

    char* pArg = reinterpret_cast<char*>(this);
    char* pArgs[2] = { pArg, NULL };

    m_pThread = new xthread(ThreadEntry, "MoviePlayerWebM", 192 * 1024, 2, 1, pArgs);

    while (!m_bThreadRunning)
    {
        x_DelayThread(1);
    }
}

//==============================================================================

xbool movie_private::Open(const char* pFilename, xbool PlayResident, xbool IsLooped)
{
    (void)PlayResident;

    if (!pFilename || !pFilename[0])
        return FALSE;

    Init();
    Close();

    m_IsLooped = IsLooped;
    m_Config   = movie_webm::player_config();
    m_Config.IsLooped = IsLooped;

    if (!m_Container.Open(pFilename, m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to open movie '%s'\n", pFilename);
        return FALSE;
    }

    if (!m_VideoDecoder.Initialize(m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to initialize video decoder for '%s'\n", pFilename);
        m_Container.Close();
        return FALSE;
    }

    if (m_Config.HasAudio)
    {
        if (!m_AudioDecoder.Initialize(m_Config))
        {
            x_DebugMsg("MoviePlayer_WebM: Audio initialization failed for '%s'\n", pFilename);
            m_Config.HasAudio = FALSE;
        }
        else
        {
            m_AudioDecoder.SetVolume(m_Volume);
        }
    }

    m_Width         = m_Config.Width;
    m_Height        = m_Config.Height;
    m_LastVideoTime = 0.0;

    m_RenderData.DataMutex.Enter();
    m_RenderData.pFrameData   = NULL;
    m_RenderData.Width        = 0;
    m_RenderData.Height       = 0;
    m_RenderData.Pitch        = 0;
    m_RenderData.FrameTime    = 0.0;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.bIsValid     = FALSE;
    m_RenderData.DataMutex.Exit();

    m_RenderBuffer.Clear();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    m_Clock.Reset();
    m_Clock.Start();

    m_bPaused           = FALSE;
    m_bVideoEOF         = FALSE;
    m_bThreadFinished   = FALSE;
    m_bPlaybackActive   = FALSE;
    m_bThreadBusy       = FALSE;

    PrimePlayback();

    m_bPlaybackActive = TRUE;

    return TRUE;
}

//==============================================================================

void movie_private::Close(void)
{
    m_bPlaybackActive = FALSE;
    m_bVideoEOF       = FALSE;

    while (m_bThreadBusy)
    {
        x_DelayThread(1);
    }

    m_Clock.Stop();

    m_RenderData.DataMutex.Enter();
    m_RenderData.pFrameData   = NULL;
    m_RenderData.Width        = 0;
    m_RenderData.Height       = 0;
    m_RenderData.Pitch        = 0;
    m_RenderData.FrameTime    = 0.0;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.bIsValid     = FALSE;
    m_RenderData.DataMutex.Exit();

    m_RenderBuffer.Clear();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    m_AudioDecoder.Shutdown();
    m_VideoDecoder.Shutdown();
    m_Container.Close();

    DestroyVideoTexture();

    m_bThreadFinished = TRUE;
}

//==============================================================================

void movie_private::Kill(void)
{
    Close();
    Shutdown();
}

//==============================================================================

void movie_private::Shutdown(void)
{
    if (!m_pThread)
        return;

    m_bThreadExit = TRUE;

    while (m_bThreadRunning)
    {
        x_DelayThread(1);
    }

    delete m_pThread;
    m_pThread = NULL;
}

//==============================================================================

void movie_private::SetVolume(f32 Volume)
{
    m_Volume = Volume;
    m_AudioDecoder.SetVolume(Volume);
}

//==============================================================================

void movie_private::Pause(void)
{
    if (m_bPaused)
        return;

    m_bPaused = TRUE;
    m_Clock.Pause();
}

//==============================================================================

void movie_private::Resume(void)
{
    if (!m_bPaused)
        return;

    m_bPaused = FALSE;
    m_Clock.Resume();
}

//==============================================================================

void movie_private::Render(void)
{
    if (!g_pd3dDevice)
        return;

    m_RenderData.DataMutex.Enter();

    const xbool hasNewFrame = m_RenderData.bHasNewFrame;
    const xbool isValid     = m_RenderData.bIsValid;

    if (hasNewFrame && isValid)
    {
        if (!m_pTexture)
        {
            if (!CreateVideoTexture())
            {
                m_RenderData.DataMutex.Exit();
                return;
            }
        }

        if (!UpdateVideoTexture())
        {
            m_RenderData.DataMutex.Exit();
            return;
        }

        m_RenderData.bHasNewFrame = FALSE;
    }

    m_RenderData.DataMutex.Exit();

    if (isValid)
    {
        RenderVideoFrame();
    }
}

//==============================================================================

void movie_private::ThreadEntry(s32 argc, char** argv)
{
    movie_private* pThis = NULL;

    if ((argc > 0) && argv && argv[0])
    {
        pThis = reinterpret_cast<movie_private*>(argv[0]);
    }

    ASSERT(pThis);

    if (pThis)
    {
        pThis->ThreadMain();
    }
}

//==============================================================================

void movie_private::ThreadMain(void)
{
    m_bThreadRunning  = TRUE;

    while (!m_bThreadExit)
    {
        ThreadLoop();

        if (!m_bPlaybackActive || m_bPaused)
        {
            x_DelayThread(4);
        }
    }

    m_bThreadRunning  = FALSE;
    m_bThreadFinished = TRUE;
}

//==============================================================================

void movie_private::ThreadLoop(void)
{
    if (!m_bPlaybackActive || m_bPaused)
        return;

    struct busy_scope
    {
        volatile xbool& m_bBusy;
        busy_scope(volatile xbool& b) : m_bBusy(b) { m_bBusy = TRUE; }
        ~busy_scope() { m_bBusy = FALSE; }
    } guard(m_bThreadBusy);

    movie_webm::sample Sample;

    if (m_bHasPendingVideo)
    {
        Sample = m_PendingVideoSample;
    }
    else
    {
        if (!m_Container.PeekSample(Sample))
        {
            HandleEndOfStream();
            return;
        }
    }

    const f64 playbackTime  = m_Clock.GetTime();
    const f64 frameDuration = (m_Config.FrameRate > 0.0f)
                              ? (1.0 / (f64)m_Config.FrameRate) : (1.0 / 30.0);

    if (Sample.Type == movie_webm::STREAM_TYPE_AUDIO)
    {
        if (m_Config.HasAudio && (Sample.TimeSeconds > playbackTime + AUDIO_BUFFER_LEAD))
        {
            SleepMilliseconds(Sample.TimeSeconds - (playbackTime + AUDIO_BUFFER_LEAD));
            return;
        }

        if (!m_Container.ReadSample(Sample))
        {
            HandleEndOfStream();
            return;
        }

        if (m_Config.HasAudio)
            m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());

        return;
    }

    if (Sample.Type != movie_webm::STREAM_TYPE_VIDEO)
    {
        m_Container.ReadSample(Sample);
        return;
    }

    if (m_bHasPendingVideo)
    {
        m_bHasPendingVideo = FALSE;
    }
    else
    {
        if (!m_Container.ReadSample(Sample))
        {
            HandleEndOfStream();
            return;
        }
    }

    const f64 delta = Sample.TimeSeconds - playbackTime;

    if (delta > VIDEO_PRESENT_LEAD)
    {
        m_PendingVideoSample = Sample;
        m_bHasPendingVideo   = TRUE;
        PumpAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
        SleepMilliseconds(delta - VIDEO_PRESENT_LEAD);
        return;
    }

    if (delta < -frameDuration)
    {
        m_LastVideoTime = Sample.TimeSeconds;
        PumpAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
        return;
    }

    if (!ProcessVideoSample(Sample))
        return;

    m_LastVideoTime = Sample.TimeSeconds;

    if (!m_IsLooped && m_Config.Duration > 0.0)
    {
        const f64 endThreshold = x_max(0.0, m_Config.Duration - frameDuration);
        if (Sample.TimeSeconds >= endThreshold)
        {
            HandleEndOfStream();
            return;
        }
    }

    PumpAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
}

//==============================================================================

xbool movie_private::PrimePlayback(void)
{
    movie_webm::sample Sample;
    xbool bDecodedFrame = FALSE;

    while (m_Container.PeekSample(Sample))
    {
        if (Sample.Type == movie_webm::STREAM_TYPE_AUDIO)
        {
            if (!m_Container.ReadSample(Sample))
                break;

            if (m_Config.HasAudio)
            {
                m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());
            }

            continue;
        }

        if (Sample.Type == movie_webm::STREAM_TYPE_VIDEO)
        {
            if (!m_Container.ReadSample(Sample))
                break;

            if (!ProcessVideoSample(Sample))
                break;

            m_LastVideoTime = Sample.TimeSeconds;
            PreloadAudio(Sample.TimeSeconds + AUDIO_BUFFER_LEAD);
            bDecodedFrame = TRUE;
            break;
        }

        if (!m_Container.ReadSample(Sample))
            break;
    }

    if (!bDecodedFrame)
    {
        m_Container.Rewind();

        if (m_Config.HasAudio)
        {
            m_AudioDecoder.Flush();
        }

        m_LastVideoTime = 0.0;
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void movie_private::PreloadAudio(f64 TargetTime)
{
    if (!m_Config.HasAudio)
        return;

    while (TRUE)
    {
        movie_webm::sample Sample;

        if (!m_Container.PeekSample(Sample))
            return;

        if (Sample.Type != movie_webm::STREAM_TYPE_AUDIO)
            return;

        if (Sample.TimeSeconds > TargetTime)
            return;

        if (!m_Container.ReadSample(Sample))
            return;

        m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());
    }
}

//==============================================================================

xbool movie_private::ProcessVideoSample(const movie_webm::sample& Sample)
{
    if (!m_VideoDecoder.DecodeSample(Sample, m_Container.GetReader()))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to decode video sample at %f sec\n", Sample.TimeSeconds);
        return FALSE;
    }

    UpdateRenderBuffer(Sample);
    return TRUE;
}

//==============================================================================

void movie_private::UpdateRenderBuffer(const movie_webm::sample& Sample)
{
    const u8* pFrameData = m_VideoDecoder.GetFrameData();
    if (!pFrameData)
        return;

    const s32 pitch  = m_VideoDecoder.GetFramePitch();
    const s32 width  = m_VideoDecoder.GetWidth();
    const s32 height = m_VideoDecoder.GetHeight();

    const s32 frameSize = pitch * height;
    if (frameSize <= 0)
        return;

    m_RenderBuffer.SetCount(frameSize);
    x_memcpy(m_RenderBuffer.GetPtr(), pFrameData, frameSize);

    m_RenderData.DataMutex.Enter();
    m_RenderData.pFrameData   = m_RenderBuffer.GetPtr();
    m_RenderData.Width        = width;
    m_RenderData.Height       = height;
    m_RenderData.Pitch        = pitch;
    m_RenderData.FrameTime    = Sample.TimeSeconds;
    m_RenderData.bHasNewFrame = TRUE;
    m_RenderData.bIsValid     = TRUE;
    m_RenderData.DataMutex.Exit();
}

//==============================================================================

void movie_private::PumpAudio(f64 TargetTime)
{
    if (!m_Config.HasAudio)
        return;

    while (m_bPlaybackActive)
    {
        movie_webm::sample Sample;

        if (!m_Container.PeekSample(Sample))
        {
            if (!m_bHasPendingVideo)
                HandleEndOfStream();
            return;
        }

        if (Sample.Type != movie_webm::STREAM_TYPE_AUDIO)
            return;

        if (Sample.TimeSeconds > TargetTime)
            return;

        if (!m_Container.ReadSample(Sample))
        {
            if (!m_bHasPendingVideo)
                HandleEndOfStream();
            return;
        }

        m_AudioDecoder.DecodeSample(Sample, m_Container.GetReader());
    }
}

//==============================================================================

void movie_private::HandleEndOfStream(void)
{
    if (!m_bPlaybackActive)
        return;

    if (m_IsLooped)
    {
        ResetPlayback();
        return;
    }

    m_bPlaybackActive = FALSE;
    m_bVideoEOF       = TRUE;
    m_bThreadFinished = TRUE;
    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    if (m_Config.HasAudio)
    {
        m_AudioDecoder.Flush();
    }

    m_Clock.Stop();
}

//==============================================================================

void movie_private::SleepMilliseconds(f64 Seconds)
{
    if (Seconds <= 0.0)
        return;

    const f64 clamped = (Seconds > 0.0) ? Seconds : 0.0;
    const s32 sleepMS = (s32)(clamped * 1000.0 + 0.5);

    if (sleepMS > 0)
    {
        x_DelayThread(sleepMS);
    }
}

//==============================================================================

void movie_private::ResetPlayback(void)
{
    m_bPlaybackActive = FALSE;

    m_Container.Rewind();
    m_VideoDecoder.Shutdown();
    if (!m_VideoDecoder.Initialize(m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to reinitialize video decoder during loop.\n");
        m_bPlaybackActive = FALSE;
        m_bVideoEOF = TRUE;
        m_bThreadFinished = TRUE;
        return;
    }
    if (m_Config.HasAudio)
    {
        m_AudioDecoder.Flush();
    }
    m_LastVideoTime = 0.0;

    m_RenderData.DataMutex.Enter();
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.DataMutex.Exit();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();

    m_Clock.Reset();

    if (!PrimePlayback())
    {
        m_RenderData.DataMutex.Enter();
        m_RenderData.pFrameData   = NULL;
        m_RenderData.Width        = 0;
        m_RenderData.Height       = 0;
        m_RenderData.Pitch        = 0;
        m_RenderData.FrameTime    = 0.0;
        m_RenderData.bHasNewFrame = FALSE;
        m_RenderData.bIsValid     = FALSE;
        m_RenderData.DataMutex.Exit();

        m_bPlaybackActive = FALSE;
        m_bVideoEOF       = TRUE;
        m_bThreadFinished = TRUE;
        return;
    }

    m_Clock.Start();

    m_bVideoEOF       = FALSE;
    m_bThreadFinished = FALSE;
    m_bPlaybackActive = TRUE;
}

//==============================================================================

xbool movie_private::CreateVideoTexture(void)
{
    if (!g_pd3dDevice || (m_Width <= 0) || (m_Height <= 0))
        return FALSE;

    D3D11_TEXTURE2D_DESC Desc;
    x_memset(&Desc, 0, sizeof(Desc));
    Desc.Width              = m_Width;
    Desc.Height             = m_Height;
    Desc.MipLevels          = 1;
    Desc.ArraySize          = 1;
    Desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    Desc.SampleDesc.Count   = 1;
    Desc.Usage              = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = g_pd3dDevice->CreateTexture2D(&Desc, NULL, &m_pTexture);
    if (FAILED(hr) || (m_pTexture == NULL))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to create video texture.\n");
        return FALSE;
    }

    m_TextureVRAMID = vram_Register(m_pTexture);
    return (m_TextureVRAMID != 0);
}

//==============================================================================

void movie_private::DestroyVideoTexture(void)
{
    if (m_TextureVRAMID)
    {
        vram_Unregister(m_TextureVRAMID);
        m_TextureVRAMID = 0;
    }

    if (m_pTexture)
    {
        m_pTexture->Release();
        m_pTexture = NULL;
    }
}

//==============================================================================

xbool movie_private::UpdateVideoTexture(void)
{
    if (!g_pd3dContext || !m_pTexture)
        return FALSE;

    if (!m_RenderData.pFrameData || (m_RenderData.Width <= 0) || (m_RenderData.Height <= 0))
        return FALSE;

    D3D11_MAPPED_SUBRESOURCE Mapped;
    HRESULT hr = g_pd3dContext->Map(m_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
    if (FAILED(hr))
        return FALSE;

    u8* pDst = reinterpret_cast<u8*>(Mapped.pData);
    const u8* pSrc = m_RenderData.pFrameData;

    for (s32 y = 0; y < m_RenderData.Height; ++y)
    {
        x_memcpy(pDst, pSrc, m_RenderData.Width * 4);
        pDst += Mapped.RowPitch;
        pSrc += m_RenderData.Pitch;
    }

    g_pd3dContext->Unmap(m_pTexture, 0);
    return TRUE;
}

//==============================================================================

void movie_private::RenderVideoFrame(void)
{
    if (!g_pd3dContext || (m_TextureVRAMID == 0))
        return;

    s32 WindowWidth = 0;
    s32 WindowHeight = 0;
    eng_GetRes(WindowWidth, WindowHeight);

    f32 destX;
    f32 destY;
    f32 destW;
    f32 destH;

    if (m_bForceStretch)
    {
        destX = 0.0f;
        destY = 0.0f;
        destW = (f32)WindowWidth;
        destH = (f32)WindowHeight;
    }
    else
    {
        const f32 aspectMovie  = (m_Height > 0) ? ((f32)m_Width / (f32)m_Height) : 1.0f;
        const f32 aspectScreen = (WindowHeight > 0) ? ((f32)WindowWidth / (f32)WindowHeight) : 1.0f;

        if (aspectScreen > aspectMovie)
        {
            destH = (f32)WindowHeight;
            destW = destH * aspectMovie;
            destX = ((f32)WindowWidth - destW) * 0.5f;
            destY = 0.0f;
        }
        else
        {
            destW = (f32)WindowWidth;
            destH = destW / aspectMovie;
            destX = 0.0f;
            destY = ((f32)WindowHeight - destH) * 0.5f;
        }
    }

    draw_Begin(DRAW_QUADS, DRAW_2D | DRAW_UI_RTARGET | DRAW_TEXTURED | DRAW_USE_ALPHA | DRAW_CULL_NONE);

    if (!m_bForceStretch)
    {
        draw_SetTexture();
        draw_Color(XCOLOR_BLACK);
        draw_Vertex(0.0f, 0.0f, 0.5f);
        draw_Vertex(0.0f, (f32)WindowHeight, 0.5f);
        draw_Vertex((f32)WindowWidth, (f32)WindowHeight, 0.5f);
        draw_Vertex((f32)WindowWidth, 0.0f, 0.5f);
    }

    vram_Activate(m_TextureVRAMID);
    draw_Color(XCOLOR_WHITE);
    draw_UV(0.0f, 0.0f); draw_Vertex(destX, destY, 0.5f);
    draw_UV(0.0f, 1.0f); draw_Vertex(destX, destY + destH, 0.5f);
    draw_UV(1.0f, 1.0f); draw_Vertex(destX + destW, destY + destH, 0.5f);
    draw_UV(1.0f, 0.0f); draw_Vertex(destX + destW, destY, 0.5f);

    draw_End();
}

//==============================================================================