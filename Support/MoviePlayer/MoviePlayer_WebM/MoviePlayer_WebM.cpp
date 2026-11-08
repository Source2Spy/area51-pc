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

#include "x_target.hpp"

#if !defined( TARGET_DESKTOP ) && !defined( TARGET_MOBILE )
#error This file should only be compiled for desktop or mobile platforms. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SDL3/SDL.h"

#include "x_files.hpp"
#include "x_threads.hpp"
#include "x_memory.hpp"
#include "Entropy.hpp"
#include "../MoviePlayer.hpp"

#include "MoviePlayer_WebM_Private.hpp"
#include "UI/ui_renderer.hpp"

//==============================================================================
// CONSTANTS
//==============================================================================

namespace
{
    static const f64 VIDEO_PRESENT_LEAD = 0.005;
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
    m_Width              = 0;
    m_Height             = 0;
    m_Volume             = 1.0f;
    m_IsLooped           = FALSE;
    m_WorkerService      = x_worker_service(HNULL);
    m_pWakeSemaphore     = NULL;
    x_AtomicInit(&m_bCloseRequested, TRUE);
    x_AtomicInit(&m_bThreadExit, FALSE);
    x_AtomicInit(&m_bThreadRunning, FALSE);
    x_AtomicInit(&m_bThreadFinished, TRUE);
    m_bPaused            = FALSE;
    m_bPlaybackActive    = FALSE;
    m_bInputEOF          = FALSE;
    m_VideoEndTime       = 0.0;
    m_VideoFrameDuration = 1.0 / 30.0;
    m_AudioSampleRead    = 0;

    m_LastVideoTime      = -1.0;
    m_bHasPendingVideo   = FALSE;
    m_PendingVideoSample = movie_webm::sample();
    m_VideoSample        = movie_webm::sample();
    m_VideoFrameIndex    = 0;
}


//==============================================================================

movie_private::~movie_private(void)
{
    Kill();
}

//==============================================================================

void movie_private::Init(void)
{
    if (m_WorkerService.IsNonNull())
        return;

    x_AtomicStoreRelease(&m_bThreadExit, FALSE);
    x_AtomicStoreRelease(&m_bThreadRunning, FALSE);
    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);

    ASSERTS(x_WorkersIsInit(), "MoviePlayer_WebM requires x_workers to be initialized");
    if (!x_WorkersIsInit())
        return;
    if (!m_Clock.IsValid())
    {
        x_DebugMsg("MoviePlayer_WebM: Playback clock mutex is unavailable.\n");
        return;
    }

    m_pWakeSemaphore = SDL_CreateSemaphore(0);
    if (!m_pWakeSemaphore)
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_CreateSemaphore failed: %s\n", SDL_GetError());
        return;
    }

    if (!x_WorkerServiceStart(WorkerEntry, this, "MoviePlayerWebM", m_WorkerService))
    {
        SDL_DestroySemaphore(m_pWakeSemaphore);
        m_pWakeSemaphore = NULL;
        ASSERTS(FALSE, "MoviePlayer_WebM failed to start worker service");
        return;
    }

    #if X_WORKERS_DEBUG && X_WORKERS_DEBUG_LOG
    x_DebugMsg("MoviePlayer_WebM: using x_worker service %08x\n", m_WorkerService.Handle);
    #endif

    while (!x_AtomicLoadAcquire(&m_bThreadRunning))
    {
        x_DelayThread(1);
    }
}

//==============================================================================

xbool movie_private::Open(const char* pFilename, xbool PlayResident, xbool IsLooped, x_language Language)
{
    (void)PlayResident;

    if (!pFilename || !pFilename[0])
        return FALSE;

    Init();
    if (m_WorkerService.IsNull())
        return FALSE;

    Close();

    m_PlaybackMutex.Enter();
    x_AtomicStoreRelease(&m_bCloseRequested, FALSE);

    m_IsLooped = IsLooped;
    m_Config   = movie_webm::player_config();
    m_Config.IsLooped = IsLooped;
    m_Config.AudioLanguage = Language;

    if (!m_Container.Open(pFilename, m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to open movie '%s'\n", pFilename);
        m_PlaybackMutex.Exit();
        return FALSE;
    }

    if (!m_VideoDecoder.Initialize(m_Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to initialize video decoder for '%s'\n", pFilename);
        m_Container.Close();
        m_PlaybackMutex.Exit();
        return FALSE;
    }

    if (m_Config.HasAudio)
    {
        if (!m_AudioDecoder.Initialize(m_Config, m_Clock, m_pWakeSemaphore))
        {
            x_DebugMsg("MoviePlayer_WebM: Audio initialization failed for '%s'\n", pFilename);
            m_PlaybackMutex.Exit();
            Close();
            return FALSE;
        }
        else
        {
            m_AudioDecoder.SetVolume(m_Volume);
        }
    }

    m_Width         = m_Config.Width;
    m_Height        = m_Config.Height;
    m_LastVideoTime = -1.0;

    m_RenderData.DataMutex.Enter();
    m_RenderData.pFrameData   = NULL;
    m_RenderData.Width        = 0;
    m_RenderData.Height       = 0;
    m_RenderData.Pitch        = 0;
    m_RenderData.FrameTime    = 0.0;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.bIsValid     = FALSE;
    m_RenderBuffer.Clear();
    m_RenderData.DataMutex.Exit();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();
    m_VideoSample = movie_webm::sample();
    m_VideoFrameIndex = 0;

    m_Clock.Reset();

    m_bPaused           = FALSE;
    m_bInputEOF         = FALSE;
    x_AtomicStoreRelease(&m_bThreadFinished, FALSE);
    m_bPlaybackActive   = FALSE;

    m_VideoEndTime = 0.0;
    m_VideoFrameDuration = m_Config.VideoFrameDuration > 0.0 ? m_Config.VideoFrameDuration
                        : m_Config.FrameRate > 0.0f ? 1.0 / m_Config.FrameRate : 1.0 / 30.0;
    if (!PrimePlayback())
    {
        m_PlaybackMutex.Exit();
        Close();
        return FALSE;
    }

    m_Clock.Start();
    if (m_Config.HasAudio && !m_AudioDecoder.Resume())
    {
        m_PlaybackMutex.Exit();
        Close();
        return FALSE;
    }

    m_bPlaybackActive = TRUE;

    m_PlaybackMutex.Exit();
    SDL_SignalSemaphore(m_pWakeSemaphore);
    return TRUE;
}

//==============================================================================

void movie_private::Close(void)
{
    x_AtomicStoreRelease(&m_bCloseRequested, TRUE);
    if (m_pWakeSemaphore)
        SDL_SignalSemaphore(m_pWakeSemaphore);

    m_PlaybackMutex.Enter();

    m_bPlaybackActive = FALSE;
    m_bInputEOF       = FALSE;

    m_Clock.Stop();

    m_RenderData.DataMutex.Enter();
    m_RenderData.pFrameData   = NULL;
    m_RenderData.Width        = 0;
    m_RenderData.Height       = 0;
    m_RenderData.Pitch        = 0;
    m_RenderData.FrameTime    = 0.0;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.bIsValid     = FALSE;
    m_RenderBuffer.Clear();
    m_RenderData.DataMutex.Exit();

    m_bHasPendingVideo = FALSE;
    m_PendingVideoSample = movie_webm::sample();
    m_VideoSample = movie_webm::sample();
    m_VideoFrameIndex = 0;

    m_AudioDecoder.Shutdown();
    m_AudioSamples.Clear();
    m_AudioSampleRead = 0;
    m_VideoDecoder.Shutdown();
    m_Container.Close();

    DestroyVideoTexture();

    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);

    m_PlaybackMutex.Exit();
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
    if (m_WorkerService.IsNull())
        return;

    x_AtomicStoreRelease(&m_bThreadExit, TRUE);
    SDL_SignalSemaphore(m_pWakeSemaphore);

    if (m_WorkerService.IsNonNull())
    {
        x_WorkerServiceWait(m_WorkerService);
        x_WorkerServiceRelease(m_WorkerService);
        m_WorkerService = x_worker_service(HNULL);
    }

    SDL_DestroySemaphore(m_pWakeSemaphore);
    m_pWakeSemaphore = NULL;
}

//==============================================================================

void movie_private::SetVolume(f32 Volume)
{
    m_PlaybackMutex.Enter();
    m_Volume = Volume;
    m_AudioDecoder.SetVolume(Volume);
    m_PlaybackMutex.Exit();
}

//==============================================================================

void movie_private::Pause(void)
{
    m_PlaybackMutex.Enter();
    if (m_bPlaybackActive && !m_bPaused)
    {
        if (!m_Config.HasAudio || m_AudioDecoder.Pause())
        {
            m_Clock.Pause();
            m_bPaused = TRUE;
        }
        else
            FailPlayback("pause audio", -1);
    }
    m_PlaybackMutex.Exit();
    if (m_pWakeSemaphore)
        SDL_SignalSemaphore(m_pWakeSemaphore);
}

//==============================================================================

void movie_private::Resume(void)
{
    m_PlaybackMutex.Enter();
    if (m_bPlaybackActive && m_bPaused)
    {
        m_Clock.Resume();
        if (!m_Config.HasAudio || m_AudioDecoder.Resume())
            m_bPaused = FALSE;
        else
            FailPlayback("resume audio", -1);
    }
    m_PlaybackMutex.Exit();
    if (m_pWakeSemaphore)
        SDL_SignalSemaphore(m_pWakeSemaphore);
}

//==============================================================================

void movie_private::Render(void)
{
    m_RenderData.DataMutex.Enter();

    const xbool hasNewFrame = m_RenderData.bHasNewFrame;
    const xbool isValid     = m_RenderData.bIsValid;

    if (hasNewFrame && isValid &&
        (m_RenderData.FrameTime <= m_Clock.GetTime() + VIDEO_PRESENT_LEAD))
    {
        if (!vram_IsValid(m_VideoTexture))
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

void movie_private::WorkerEntry(void* pData)
{
    movie_private* pThis = reinterpret_cast<movie_private*>(pData);

    ASSERT(pThis);

    if (pThis)
    {
        pThis->ThreadMain();
    }
}

//==============================================================================

void movie_private::ThreadMain(void)
{
    x_AtomicStoreRelease(&m_bThreadRunning, TRUE);

    while (!x_AtomicLoadAcquire(&m_bThreadExit))
    {
        m_PlaybackMutex.Enter();
        const f64 sleepTime = ThreadLoop();
        m_PlaybackMutex.Exit();

        if (sleepTime < 0.0)
            SDL_WaitSemaphore(m_pWakeSemaphore);
        else
            WaitForPlayback(sleepTime);
    }

    x_AtomicStoreRelease(&m_bThreadRunning, FALSE);
    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);
}

//==============================================================================

f64 movie_private::ThreadLoop(void)
{
    if (x_AtomicLoadAcquire(&m_bCloseRequested) || !m_bPlaybackActive || m_bPaused)
        return -1.0;

    if (!PumpAudio())
        return 0.0;

    const f64 now = m_Clock.GetTime();
    if (m_bHasPendingVideo)
    {
        const f64 delta = m_PendingVideoSample.TimeSeconds - now;
        if (delta <= VIDEO_PRESENT_LEAD)
        {
            if (delta >= -m_VideoFrameDuration)
            {
                UpdateRenderBuffer(m_PendingVideoSample);
            }
            m_bHasPendingVideo = FALSE;
        }
    }

    if (!m_bHasPendingVideo && m_VideoSample.pBlock)
    {
        const movie_webm::sample Frame = GetVideoFrame(m_VideoSample, m_VideoFrameIndex++);
        if (m_VideoFrameIndex == m_VideoSample.pBlock->GetFrameCount())
            m_VideoSample = movie_webm::sample();
        const movie_webm::decode_status decoded = ProcessVideoSample(Frame);
        if (decoded == movie_webm::DECODE_ERROR)
            FailPlayback("decode video", -1, &Frame);
        else if (decoded == movie_webm::DECODE_FRAME)
        {
            m_PendingVideoSample = Frame;
            m_bHasPendingVideo = TRUE;
        }
        return 0.0;
    }

    movie_webm::sample Sample;
    const movie_webm::sample_status status = m_bInputEOF ? movie_webm::SAMPLE_END : m_Container.PeekSample(Sample);
    if (status == movie_webm::SAMPLE_ERROR)
    {
        FailPlayback("read container", m_Container.GetError());
        return 0.0;
    }
    if (status == movie_webm::SAMPLE_END)
    {
        m_bInputEOF = TRUE;
        if (m_Config.HasAudio)
        {
            if (m_AudioDecoder.HasPendingSample() || m_AudioSamples.GetCount())
            {
                if (!m_AudioDecoder.HasPendingSample())
                    return 0.0;
                return m_bHasPendingVideo
                     ? x_max(0.0, m_PendingVideoSample.TimeSeconds - now - VIDEO_PRESENT_LEAD) : -1.0;
            }
            m_AudioDecoder.EndInput();
        }
        const f64 end = m_Config.HasAudio
                      ? x_max(m_VideoEndTime, m_AudioDecoder.GetEndTime() + m_AudioDecoder.GetDevicePeriod())
                      : m_VideoEndTime;
        if (!m_bHasPendingVideo && (now >= end) &&
            (!m_Config.HasAudio || m_AudioDecoder.IsDrained()))
        {
            HandleEndOfStream();
            return 0.0;
        }
        if (m_bHasPendingVideo)
            return x_max(0.0, m_PendingVideoSample.TimeSeconds - now - VIDEO_PRESENT_LEAD);
        return now < end ? end - now : m_AudioDecoder.GetDevicePeriod();
    }

    if (Sample.Type == movie_webm::STREAm_Type_AUDIO)
    {
        if (m_Container.ReadSample(Sample) != movie_webm::SAMPLE_READY)
        {
            FailPlayback("read audio block", m_Container.GetError(), &Sample);
            return 0.0;
        }
        if (m_Config.HasAudio)
            m_AudioSamples.Append(Sample);
        return 0.0;
    }
    if (m_bHasPendingVideo)
        return x_max(0.0, m_PendingVideoSample.TimeSeconds - now - VIDEO_PRESENT_LEAD);

    if (m_Container.ReadSample(Sample) != movie_webm::SAMPLE_READY)
    {
        FailPlayback("read video block", m_Container.GetError(), &Sample);
        return 0.0;
    }
    m_VideoSample = Sample;
    m_VideoFrameIndex = 0;
    return 0.0;
}

//==============================================================================

xbool movie_private::PumpAudio(void)
{
    if (!m_Config.HasAudio)
        return TRUE;
    if (m_AudioDecoder.HasError())
    {
        x_DebugMsg("MoviePlayer_WebM: SDL audio error: %s\n", m_AudioDecoder.GetError());
        FailPlayback("output audio", -1);
        return FALSE;
    }
    if (!m_AudioDecoder.HasPendingSample() && (m_AudioSampleRead < m_AudioSamples.GetCount()))
    {
        const movie_webm::sample& Sample = m_AudioSamples[m_AudioSampleRead];
        if (!m_AudioDecoder.BeginSample(Sample, m_Container.GetReader()))
        {
            FailPlayback("prepare audio block", -1, &Sample);
            return FALSE;
        }
        ++m_AudioSampleRead;
        if (m_AudioSampleRead == m_AudioSamples.GetCount())
        {
            m_AudioSamples.Clear();
            m_AudioSampleRead = 0;
        }
    }
    if (!m_AudioDecoder.Pump())
    {
        FailPlayback("decode audio", -1);
        return FALSE;
    }
    return TRUE;
}

//==============================================================================

xbool movie_private::PrimePlayback(void)
{
    movie_webm::sample Sample;
    while (!x_AtomicLoadAcquire(&m_bCloseRequested))
    {
        const movie_webm::sample_status status = m_Container.ReadSample(Sample);
        if (status != movie_webm::SAMPLE_READY)
        {
            FailPlayback("prepare first frame", m_Container.GetError());
            return FALSE;
        }
        if (Sample.Type == movie_webm::STREAm_Type_AUDIO)
        {
            if (m_Config.HasAudio)
                m_AudioSamples.Append(Sample);
            continue;
        }
        for (s32 i = 0; i < Sample.pBlock->GetFrameCount(); ++i)
        {
            const movie_webm::sample Frame = GetVideoFrame(Sample, i);
            const movie_webm::decode_status decoded = ProcessVideoSample(Frame);
            if (decoded == movie_webm::DECODE_ERROR)
            {
                FailPlayback("decode first frame", -1, &Frame);
                return FALSE;
            }
            if (decoded == movie_webm::DECODE_FRAME)
            {
                if (i + 1 < Sample.pBlock->GetFrameCount())
                {
                    m_VideoSample = Sample;
                    m_VideoFrameIndex = i + 1;
                }
                UpdateRenderBuffer(Frame);
                return PumpAudio();
            }
        }
    }
    return FALSE;
}

//==============================================================================

movie_webm::decode_status movie_private::ProcessVideoSample(const movie_webm::sample& Sample)
{
    const f64 interval = m_LastVideoTime >= 0.0 ? Sample.TimeSeconds - m_LastVideoTime : 0.0;
    const f64 duration = x_max(m_VideoFrameDuration, interval);
    const xbool convert = !m_bPlaybackActive || (Sample.TimeSeconds >= m_Clock.GetTime() - duration);
    const movie_webm::decode_status status = m_VideoDecoder.DecodeSample(Sample, m_Container.GetReader(), convert);
    if ((status == movie_webm::DECODE_FRAME) || (status == movie_webm::DECODE_DISCARDED))
    {
        if ((m_LastVideoTime >= 0.0) && (m_Config.VideoFrameDuration <= 0.0) && (interval > 0.0))
            m_VideoFrameDuration = interval;
        m_LastVideoTime = Sample.TimeSeconds;
        m_VideoEndTime = Sample.TimeSeconds + m_VideoFrameDuration;
    }
    return status;
}

//==============================================================================

movie_webm::sample movie_private::GetVideoFrame(const movie_webm::sample& Sample, s32 FrameIndex) const
{
    movie_webm::sample Frame = Sample;
    const f64 duration = m_Config.VideoFrameDuration > 0.0 ? m_Config.VideoFrameDuration : m_VideoFrameDuration;
    Frame.FrameIndex = FrameIndex;
    Frame.TimeNanoseconds += (s64)(FrameIndex * duration * movie_webm::WEBM_NANOSEC + 0.5);
    Frame.TimeSeconds = (f64)Frame.TimeNanoseconds / movie_webm::WEBM_NANOSEC;
    return Frame;
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

    m_RenderData.DataMutex.Enter();
    m_RenderBuffer.SetCount(frameSize);
    x_memcpy(m_RenderBuffer.GetPtr(), pFrameData, frameSize);

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

void movie_private::FailPlayback(const char* pOperation, s32 Code, const movie_webm::sample* pSample)
{
    x_DebugMsg("MoviePlayer_WebM: %s failed (%d), track %d at %.6f sec.\n",
               pOperation, Code, pSample ? (s32)pSample->pBlock->GetTrackNumber() : -1,
               pSample ? pSample->TimeSeconds : m_Clock.GetTime());
    m_bPlaybackActive = FALSE;
    m_bHasPendingVideo = FALSE;
    m_AudioDecoder.Shutdown();
    m_AudioSamples.Clear();
    m_AudioSampleRead = 0;
    m_VideoSample = movie_webm::sample();
    m_PendingVideoSample = movie_webm::sample();
    m_VideoDecoder.Shutdown();
    m_Container.Close();
    m_RenderData.DataMutex.Enter();
    m_RenderData.bIsValid = FALSE;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.pFrameData = NULL;
    m_RenderBuffer.Clear();
    m_RenderData.DataMutex.Exit();
    m_Clock.Stop();
    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);
}

//==============================================================================

void movie_private::HandleEndOfStream(void)
{
    if (m_IsLooped)
    {
        ResetPlayback();
        return;
    }
    if (m_Config.HasAudio && !m_AudioDecoder.Pause())
    {
        FailPlayback("finish audio", -1);
        return;
    }
    m_bPlaybackActive = FALSE;
    m_Clock.Stop();
    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);
}

//==============================================================================

void movie_private::WaitForPlayback(f64 Seconds)
{
    if (Seconds <= 0.0)
        return;

    const s32 sleepMS = (s32)x_min(Seconds * 1000.0 + 0.5, (f64)INT_MAX);
    if (sleepMS > 0)
    {
        SDL_WaitSemaphoreTimeout(m_pWakeSemaphore, sleepMS);
    }
}

//==============================================================================

void movie_private::ResetPlayback(void)
{
    m_bPlaybackActive = FALSE;
    if (m_Config.HasAudio && !m_AudioDecoder.Reset())
    {
        FailPlayback("reset audio", -1);
        return;
    }
    m_Container.Rewind();
    m_VideoDecoder.Shutdown();
    if (!m_VideoDecoder.Initialize(m_Config))
    {
        FailPlayback("reset video", -1);
        return;
    }
    m_AudioSamples.Clear();
    m_AudioSampleRead = 0;
    m_bHasPendingVideo = FALSE;
    m_VideoSample = movie_webm::sample();
    m_VideoFrameIndex = 0;
    m_bInputEOF = FALSE;
    m_LastVideoTime = -1.0;
    m_VideoEndTime = 0.0;
    m_VideoFrameDuration = m_Config.VideoFrameDuration > 0.0 ? m_Config.VideoFrameDuration
                        : m_Config.FrameRate > 0.0f ? 1.0 / m_Config.FrameRate : 1.0 / 30.0;
    m_Clock.Reset();
    if (!PrimePlayback())
        return;
    m_Clock.Start();
    if (m_Config.HasAudio && !m_AudioDecoder.Resume())
    {
        FailPlayback("restart audio", -1);
        return;
    }
    m_bPlaybackActive = TRUE;
}

//==============================================================================

xbool movie_private::CreateVideoTexture(void)
{
    if ((m_RenderData.Width <= 0) || (m_RenderData.Height <= 0))
        return FALSE;

    DestroyVideoTexture();

    vram_texture_desc Desc;
    Desc.Width      = (u32)m_RenderData.Width;
    Desc.Height     = (u32)m_RenderData.Height;
    Desc.Format     = VRAM_TEXTURE_FORMAT_BGRA8;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.pDebugName = "MovieVideoTexture";
    return vram_CreateTexture(m_VideoTexture, Desc);
}

//==============================================================================

void movie_private::DestroyVideoTexture(void)
{
    vram_DestroyTexture(m_VideoTexture);
}

//==============================================================================

xbool movie_private::UpdateVideoTexture(void)
{
    if (!vram_IsValid(m_VideoTexture) ||
        !m_RenderData.pFrameData ||
        (m_RenderData.Width <= 0) ||
        (m_RenderData.Height <= 0) ||
        (m_RenderData.Pitch <= 0))
    {
        return FALSE;
    }

    if ((m_VideoTexture.Desc.Width != (u32)m_RenderData.Width) ||
        (m_VideoTexture.Desc.Height != (u32)m_RenderData.Height))
    {
        if (!CreateVideoTexture())
            return FALSE;
    }

    vram_texture_upload_desc Upload;
    Upload.Region.Width  = (u32)m_RenderData.Width;
    Upload.Region.Height = (u32)m_RenderData.Height;
    Upload.Region.Depth  = 1;
    Upload.pData         = m_RenderData.pFrameData;
    Upload.Size          = (u32)(m_RenderData.Pitch * m_RenderData.Height);
    Upload.RowPitch      = (u32)m_RenderData.Pitch;
    Upload.SlicePitch    = Upload.Size;
    Upload.bCycle        = TRUE;
    return vram_UploadTexture(m_VideoTexture, Upload);
}

//==============================================================================

void movie_private::RenderVideoFrame(void)
{
    if (!vram_IsValid(m_VideoTexture))
        return;

    ui_viewport const& Viewport = g_UIRenderer.GetViewport();
    rect const& Bounds = Viewport.GetLogicalBounds();
    vector2 Position = Bounds.Min;
    vector2 Size     = Bounds.GetSize();
    vector2 UV0(0.0f, 0.0f);
    vector2 UV1(1.0f, 1.0f);

    const shader_resource* pVideoResource = vram_GetShaderResource(m_VideoTexture);
    if (pVideoResource)
    {
        g_UIRenderer.DrawImage(*pVideoResource,
                               Position,
                               Size,
                               UV0,
                               UV1,
                               XCOLOR_WHITE,
                               0.0f,
                               UI_BLEND_ALPHA,
                               UI_SAMPLER_LINEAR_CLAMP);
    }
}

//==============================================================================
