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

static const f64 VIDEO_PRESENT_LEAD  = 0.005;
static const f64 VIDEO_REFRESH_LIMIT = 0.100;
static const f64 AUDIO_PREROLL       = 0.100;

//==============================================================================

static
s32 VideoPlaneSize(s32 LumaSize, s32 Plane)
{
    return (Plane == VPX_PLANE_Y) ? LumaSize : (LumaSize + 1) / 2;
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

    m_LastVideoTime      = -1.0;
    m_LastVideoPublishTime = -1.0;
    m_bHasPendingVideo   = FALSE;
    m_PendingVideoTime    = 0.0;
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
    if( m_WorkerService.IsNonNull() )
    {
        return;
    }

    x_AtomicStoreRelease(&m_bThreadExit, FALSE);
    x_AtomicStoreRelease(&m_bThreadRunning, FALSE);
    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);

    ASSERTS(x_WorkersIsInit(), "MoviePlayer_WebM requires x_workers to be initialized");
    if( !x_WorkersIsInit() )
    {
        return;
    }

    if( !m_Clock.IsValid() )
    {
        x_DebugMsg("MoviePlayer_WebM: Playback clock mutex is unavailable.\n");
        return;
    }

    m_pWakeSemaphore = SDL_CreateSemaphore(0);
    if( !m_pWakeSemaphore )
    {
        x_DebugMsg("MoviePlayer_WebM: SDL_CreateSemaphore failed: %s\n", SDL_GetError());
        return;
    }

    if( !x_WorkerServiceStart(WorkerEntry, this, "MoviePlayerWebM", m_WorkerService) )
    {
        SDL_DestroySemaphore(m_pWakeSemaphore);
        m_pWakeSemaphore = NULL;
        ASSERTS(FALSE, "MoviePlayer_WebM failed to start worker service");
        return;
    }

    #if X_WORKERS_DEBUG && X_WORKERS_DEBUG_LOG
    x_DebugMsg("MoviePlayer_WebM: using x_worker service %08x\n", m_WorkerService.Handle);
    #endif

    while( !x_AtomicLoadAcquire(&m_bThreadRunning) )
    {
        x_DelayThread(1);
    }
}

//==============================================================================

xbool movie_private::Open(const char* pFilename, xbool PlayResident, xbool IsLooped, x_language Language)
{
    (void)PlayResident;

    if( !pFilename || !pFilename[0] )
    {
        return FALSE;
    }

    Init();
    if( m_WorkerService.IsNull() )
    {
        return FALSE;
    }

    Close();

    m_PlaybackMutex.Enter();
    x_AtomicStoreRelease(&m_bCloseRequested, FALSE);

    m_IsLooped = IsLooped;
    m_Config   = movie_webm::player_config();
    m_Config.IsLooped = IsLooped;
    m_Config.AudioLanguage = Language;

    if( !m_Container.Open(pFilename, m_Config) )
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to open movie '%s'\n", pFilename);
        m_PlaybackMutex.Exit();
        return FALSE;
    }

    if( !m_VideoDecoder.Initialize(m_Config) )
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to initialize video decoder for '%s'\n", pFilename);
        m_Container.Close();
        m_PlaybackMutex.Exit();
        return FALSE;
    }

    if( m_Config.HasAudio )
    {
        if( !m_AudioDecoder.Initialize(m_Config, m_Clock, m_pWakeSemaphore) )
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
    m_RenderData.Width        = 0;
    m_RenderData.Height       = 0;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.bIsValid     = FALSE;
    m_RenderData.Pixels.Clear();
    m_RenderData.DataMutex.Exit();

    m_Clock.Reset();

    m_bPaused           = FALSE;
    m_bInputEOF         = FALSE;
    x_AtomicStoreRelease(&m_bThreadFinished, FALSE);
    m_bPlaybackActive   = FALSE;

    m_VideoEndTime = 0.0;
    m_VideoFrameDuration = m_Config.VideoFrameDuration > 0.0 ? m_Config.VideoFrameDuration
                        : m_Config.FrameRate > 0.0f ? 1.0 / m_Config.FrameRate : 1.0 / 30.0;

    if( !PrimePlayback() )
    {
        m_PlaybackMutex.Exit();
        Close();
        return FALSE;
    }

    m_Clock.Start();
    if( m_Config.HasAudio && !m_AudioDecoder.Resume() )
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
    if( m_pWakeSemaphore )
    {
        SDL_SignalSemaphore(m_pWakeSemaphore);
    }

    m_PlaybackMutex.Enter();

    m_bPlaybackActive = FALSE;
    m_bInputEOF       = FALSE;

    m_Clock.Stop();

    m_RenderData.DataMutex.Enter();
    m_RenderData.Width        = 0;
    m_RenderData.Height       = 0;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.bIsValid     = FALSE;
    m_RenderData.Pixels.Clear();
    m_RenderData.DataMutex.Exit();

    ClearPendingSamples();
    m_AudioDecoder.Shutdown();
    m_VideoDecoder.Shutdown();
    m_Container.Close();

    DestroyVideoTextures();

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
    if( m_WorkerService.IsNull() )
    {
        return;
    }

    x_AtomicStoreRelease(&m_bThreadExit, TRUE);
    SDL_SignalSemaphore(m_pWakeSemaphore);

    if( m_WorkerService.IsNonNull() )
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
    if( m_bPlaybackActive && !m_bPaused )
    {
        if( !m_Config.HasAudio || m_AudioDecoder.Pause() )
        {
            m_Clock.Pause();
            m_bPaused = TRUE;
        }
        else
        {
            FailPlayback("pause audio", -1);
        }
    }
    m_PlaybackMutex.Exit();

    if( m_pWakeSemaphore )
    {
        SDL_SignalSemaphore(m_pWakeSemaphore);
    }
}

//==============================================================================

void movie_private::Resume(void)
{
    m_PlaybackMutex.Enter();
    if( m_bPlaybackActive && m_bPaused )
    {
        m_Clock.Resume();

        if( !m_Config.HasAudio || m_AudioDecoder.Resume() )
        {
            m_bPaused = FALSE;
        }
        else
        {
            FailPlayback("resume audio", -1);
        }
    }
    m_PlaybackMutex.Exit();

    if( m_pWakeSemaphore )
    {
        SDL_SignalSemaphore(m_pWakeSemaphore);
    }
}

//==============================================================================

void movie_private::Render(void)
{
    m_RenderData.DataMutex.Enter();

    const xbool hasNewFrame = m_RenderData.bHasNewFrame;
    const xbool isValid     = m_RenderData.bIsValid;

    if( hasNewFrame && isValid )
    {
        if( !UpdateVideoTexture() )
        {
            m_RenderData.DataMutex.Exit();
            return;
        }

        m_RenderData.bHasNewFrame = FALSE;
    }

    m_RenderData.DataMutex.Exit();

    if( isValid )
    {
        RenderVideoFrame();
    }
}

//==============================================================================

void movie_private::WorkerEntry(void* pData)
{
    movie_private* pThis = reinterpret_cast<movie_private*>(pData);

    ASSERT(pThis);

    if( pThis )
    {
        pThis->ThreadMain();
    }
}

//==============================================================================

void movie_private::ThreadMain(void)
{
    x_AtomicStoreRelease(&m_bThreadRunning, TRUE);

    while( !x_AtomicLoadAcquire(&m_bThreadExit) )
    {
        m_PlaybackMutex.Enter();
        const f64 sleepTime = ThreadLoop();
        m_PlaybackMutex.Exit();

        if( sleepTime < 0.0 )
        {
            SDL_WaitSemaphore(m_pWakeSemaphore);
        }
        else
        {
            WaitForPlayback(sleepTime);
        }
    }

    x_AtomicStoreRelease(&m_bThreadRunning, FALSE);
    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);
}

//==============================================================================

f64 movie_private::ThreadLoop(void)
{
    if( x_AtomicLoadAcquire(&m_bCloseRequested) || !m_bPlaybackActive || m_bPaused )
    {
        return -1.0;
    }

    const f64 presentTime = m_bHasPendingVideo ? m_Clock.GetTime() : 0.0;
    if( m_bHasPendingVideo && (m_PendingVideoTime <= presentTime + VIDEO_PRESENT_LEAD) )
    {
        UpdateRenderBuffer();
        m_LastVideoPublishTime = presentTime;
        m_bHasPendingVideo = FALSE;
    }

    const movie_webm::work_status input = PumpInput();
    if( input == movie_webm::WORK_ERROR )
    {
        return 0.0;
    }

    const movie_webm::work_status audio = PumpAudio();
    if( audio == movie_webm::WORK_ERROR )
    {
        return 0.0;
    }

    const xbool audioEOF = !m_Config.HasAudio ||
                           (m_bInputEOF && m_AudioSamples.IsEmpty() && !m_AudioDecoder.HasPendingSample());
    if( m_Config.HasAudio && audioEOF )
    {
        m_AudioDecoder.EndInput();
    }

    f64 end = 0.0;
    if( m_bInputEOF && audioEOF )
    {
        const f64 videoEnd = x_max(m_VideoEndTime, m_LastQueuedVideoTime + m_VideoFrameDuration);
        end = m_Config.HasAudio
            ? x_max(videoEnd, m_AudioDecoder.GetEndTime() + m_AudioDecoder.GetDevicePeriod())
            : videoEnd;
        if( (m_Clock.GetTime() >= end) && (!m_Config.HasAudio || m_AudioDecoder.IsDrained()) )
        {
            HandleEndOfStream();
            return 0.0;
        }
    }

    const xbool refillAudio = !audioEOF &&
                              ((input == movie_webm::WORK_PROGRESS) || (audio == movie_webm::WORK_PROGRESS)) &&
                              (m_AudioDecoder.GetBufferedTime() < AUDIO_PREROLL);
    const movie_webm::work_status video = refillAudio ? movie_webm::WORK_BLOCKED : PumpVideo();
    if( video == movie_webm::WORK_ERROR )
    {
        return 0.0;
    }

    if( (input == movie_webm::WORK_PROGRESS) || (audio == movie_webm::WORK_PROGRESS) ||
        (video == movie_webm::WORK_PROGRESS) )
    {
        return 0.0;
    }

    const f64 now = m_Clock.GetTime();
    if( m_bHasPendingVideo )
    {
        return x_max(0.0, m_PendingVideoTime - now - VIDEO_PRESENT_LEAD);
    }

    if( m_bInputEOF && audioEOF )
    {
        return now < end ? end - now : m_AudioDecoder.GetDevicePeriod();
    }

    return -1.0;
}

//==============================================================================

movie_webm::work_status movie_private::PumpInput(void)
{
    if( m_bInputEOF )
    {
        return movie_webm::WORK_BLOCKED;
    }

    movie_webm::sample Sample;
    const movie_webm::sample_status status = m_Container.PeekSample(Sample);
    if( status == movie_webm::SAMPLE_ERROR )
    {
        FailPlayback("read container", m_Container.GetError());
        return movie_webm::WORK_ERROR;
    }
    if( status == movie_webm::SAMPLE_END )
    {
        m_bInputEOF = TRUE;
        return movie_webm::WORK_PROGRESS;
    }

    const xbool isAudio = Sample.Type == movie_webm::STREAm_Type_AUDIO;
    movie_webm::sample_queue& Queue = isAudio ? m_AudioSamples : m_VideoSamples;
    if( Queue.IsFull() )
    {
        return movie_webm::WORK_BLOCKED;
    }

    if( m_Container.ReadSample(Sample) != movie_webm::SAMPLE_READY )
    {
        FailPlayback("read block", m_Container.GetError());
        return movie_webm::WORK_ERROR;
    }

    if( !isAudio || m_Config.HasAudio )
    {
        Queue.Push(Sample);
    }
    if( !isAudio )
    {
        const s32 lastFrame = x_max(0, Sample.pBlock->GetFrameCount() - 1);
        m_LastQueuedVideoTime = x_max(m_LastQueuedVideoTime, GetVideoFrame(Sample, lastFrame).TimeSeconds);
    }
    return movie_webm::WORK_PROGRESS;
}

//==============================================================================

movie_webm::work_status movie_private::PumpAudio(void)
{
    if( !m_Config.HasAudio )
    {
        return movie_webm::WORK_BLOCKED;
    }
    if( m_AudioDecoder.HasError() )
    {
        x_DebugMsg("MoviePlayer_WebM: SDL audio error: %s\n", m_AudioDecoder.GetError());
        FailPlayback("output audio", -1);
        return movie_webm::WORK_ERROR;
    }

    if( !m_AudioDecoder.HasPendingSample() && !m_AudioSamples.IsEmpty() )
    {
        const movie_webm::sample Sample = m_AudioSamples.Pop();
        if( !m_AudioDecoder.BeginSample(Sample, m_Container.GetReader()) )
        {
            FailPlayback("prepare audio block", -1, &Sample);
            return movie_webm::WORK_ERROR;
        }
    }

    const movie_webm::work_status status = m_AudioDecoder.Pump();
    if( status == movie_webm::WORK_ERROR )
    {
        FailPlayback("decode audio", -1);
    }
    return status;
}

//==============================================================================

xbool movie_private::PrimePlayback(void)
{
    while( !x_AtomicLoadAcquire(&m_bCloseRequested) )
    {
        const movie_webm::work_status input = PumpInput();
        if( input == movie_webm::WORK_ERROR )
        {
            return FALSE;
        }

        const movie_webm::work_status audio = PumpAudio();
        if( audio == movie_webm::WORK_ERROR )
        {
            return FALSE;
        }

        const movie_webm::work_status video = PumpVideo();
        if( video == movie_webm::WORK_ERROR )
        {
            return FALSE;
        }

        if( m_bHasPendingVideo && (!m_Config.HasAudio || m_AudioDecoder.GetBufferedTime() >= AUDIO_PREROLL) )
        {
            return TRUE;
        }

        if( (input == movie_webm::WORK_BLOCKED) && (audio == movie_webm::WORK_BLOCKED) &&
            (video == movie_webm::WORK_BLOCKED) )
        {
            if( m_bHasPendingVideo || (!m_bInputEOF && m_AudioDecoder.HasPendingSample()) )
            {
                return TRUE;
            }

            FailPlayback("prepare first frame", m_Container.GetError());
            return FALSE;
        }
    }

    return FALSE;
}

//==============================================================================

movie_webm::work_status movie_private::PumpVideo(void)
{
    if( m_bHasPendingVideo )
    {
        return movie_webm::WORK_BLOCKED;
    }

    if( !m_VideoSample.pBlock )
    {
        if( m_VideoSamples.IsEmpty() )
        {
            return movie_webm::WORK_BLOCKED;
        }

        m_VideoSample = m_VideoSamples.Pop();
        m_VideoFrameIndex = 0;
    }

    const movie_webm::sample Frame = GetVideoFrame(m_VideoSample, m_VideoFrameIndex++);
    if( m_VideoFrameIndex >= m_VideoSample.pBlock->GetFrameCount() )
    {
        m_VideoSample = movie_webm::sample();
    }

    const movie_webm::decode_status status = m_VideoDecoder.DecodeSample(Frame, m_Container.GetReader());
    if( status == movie_webm::DECODE_ERROR )
    {
        FailPlayback("decode video", -1, &Frame);
        return movie_webm::WORK_ERROR;
    }

    if( status == movie_webm::DECODE_FRAME )
    {
        const f64 interval = Frame.TimeSeconds - m_LastVideoTime;
        if( (m_LastVideoTime >= 0.0) && (m_Config.VideoFrameDuration <= 0.0) && (interval > 0.0) )
        {
            m_VideoFrameDuration = interval;
        }
        m_LastVideoTime = Frame.TimeSeconds;
        m_VideoEndTime = x_max(m_VideoEndTime, Frame.TimeSeconds + m_VideoFrameDuration);

        if( ShouldSkipVideoFrame(Frame.TimeSeconds) )
        {
            return movie_webm::WORK_PROGRESS;
        }

        m_PendingVideoTime = Frame.TimeSeconds;
        m_bHasPendingVideo = TRUE;
    }

    return movie_webm::WORK_PROGRESS;
}

//==============================================================================

xbool movie_private::ShouldSkipVideoFrame(f64 FrameTime) const
{
    if( m_LastVideoPublishTime < 0.0 )
    {
        return FALSE;
    }

    const f64 now = m_Clock.GetTime();
    if( (FrameTime >= now - m_VideoFrameDuration) ||
        (now - m_LastVideoPublishTime >= VIDEO_REFRESH_LIMIT) )
    {
        return FALSE;
    }

    const movie_webm::sample* pNext = m_VideoSample.pBlock ? &m_VideoSample : m_VideoSamples.Peek();
    const s32 nextFrameIndex = m_VideoSample.pBlock ? m_VideoFrameIndex : 0;
    if( !pNext )
    {
        return FALSE;
    }

    const f64 nextTime = GetVideoFrame(*pNext, nextFrameIndex).TimeSeconds;
    return (nextTime > FrameTime) && (nextTime <= now + VIDEO_PRESENT_LEAD);
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

void movie_private::UpdateRenderBuffer(void)
{
    // The worker does not decode again while this libvpx image is pending.
    const vpx_image_t& Image = m_VideoDecoder.GetDecodedImage();
    const s32 width  = (s32)Image.d_w;
    const s32 height = (s32)Image.d_h;
    const s32 chromaSize = VideoPlaneSize(width, VPX_PLANE_U) * VideoPlaneSize(height, VPX_PLANE_U);

    m_RenderData.DataMutex.Enter();
    m_RenderData.Pixels.SetCount(width * height + 2 * chromaSize);

    u8* pDest = m_RenderData.Pixels.GetPtr();
    for( s32 plane = 0; plane < 3; ++plane )
    {
        const s32 rowBytes = VideoPlaneSize(width, plane);
        const s32 rows = VideoPlaneSize(height, plane);
        for( s32 row = 0; row < rows; ++row )
        {
            x_memcpy(pDest, Image.planes[plane] + row * Image.stride[plane], rowBytes);
            pDest += rowBytes;
        }
    }

    m_RenderData.Width        = width;
    m_RenderData.Height       = height;
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
    m_AudioDecoder.Shutdown();

    ClearPendingSamples();
    m_VideoDecoder.Shutdown();
    m_Container.Close();

    m_RenderData.DataMutex.Enter();
    m_RenderData.bIsValid = FALSE;
    m_RenderData.bHasNewFrame = FALSE;
    m_RenderData.Pixels.Clear();
    m_RenderData.DataMutex.Exit();

    m_Clock.Stop();
    x_AtomicStoreRelease(&m_bThreadFinished, TRUE);
}

//==============================================================================

void movie_private::HandleEndOfStream(void)
{
    if( m_IsLooped )
    {
        ResetPlayback();
        return;
    }

    if( m_Config.HasAudio && !m_AudioDecoder.Pause() )
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
    if( Seconds <= 0.0 )
    {
        return;
    }

    const s32 sleepMS = (s32)x_min(Seconds * 1000.0 + 0.5, (f64)S32_MAX);
    if( sleepMS > 0 )
    {
        SDL_WaitSemaphoreTimeout(m_pWakeSemaphore, sleepMS);
    }
}

//==============================================================================

void movie_private::ResetPlayback(void)
{
    m_bPlaybackActive = FALSE;
    if( m_Config.HasAudio && !m_AudioDecoder.Reset() )
    {
        FailPlayback("reset audio", -1);
        return;
    }

    m_Container.Rewind();
    m_VideoDecoder.Shutdown();
    if( !m_VideoDecoder.Initialize(m_Config) )
    {
        FailPlayback("reset video", -1);
        return;
    }

    ClearPendingSamples();
    m_bInputEOF = FALSE;
    m_LastVideoTime = -1.0;
    m_VideoEndTime = 0.0;
    m_VideoFrameDuration = m_Config.VideoFrameDuration > 0.0 ? m_Config.VideoFrameDuration
                        : m_Config.FrameRate > 0.0f ? 1.0 / m_Config.FrameRate : 1.0 / 30.0;
    m_Clock.Reset();

    if( !PrimePlayback() )
    {
        return;
    }

    m_Clock.Start();
    if( m_Config.HasAudio && !m_AudioDecoder.Resume() )
    {
        FailPlayback("restart audio", -1);
        return;
    }

    m_bPlaybackActive = TRUE;
}

//==============================================================================

void movie_private::ClearPendingSamples(void)
{
    m_AudioSamples.Clear();
    m_VideoSamples.Clear();
    m_bHasPendingVideo = FALSE;
    m_PendingVideoTime = 0.0;
    m_LastVideoPublishTime = -1.0;
    m_LastQueuedVideoTime = 0.0;
    m_VideoSample = movie_webm::sample();
    m_VideoFrameIndex = 0;
}

//==============================================================================

xbool movie_private::CreateVideoTextures(void)
{
    DestroyVideoTextures();

    static const char* const PlaneNames[] = { "MovieVideoY", "MovieVideoU", "MovieVideoV" };
    for( s32 plane = 0; plane < 3; ++plane )
    {
        vram_texture_desc Desc;
        Desc.Width      = (u32)VideoPlaneSize(m_RenderData.Width, plane);
        Desc.Height     = (u32)VideoPlaneSize(m_RenderData.Height, plane);
        Desc.Format     = VRAM_TEXTURE_FORMAT_R8;
        Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
        Desc.pDebugName = PlaneNames[plane];

        if( !vram_CreateTexture(m_VideoPlanes[plane], Desc) )
        {
            DestroyVideoTextures();
            return FALSE;
        }
    }
    return TRUE;
}

//==============================================================================

void movie_private::DestroyVideoTextures(void)
{
    for( vram_texture& Plane : m_VideoPlanes )
    {
        vram_DestroyTexture(Plane);
    }
}

//==============================================================================

xbool movie_private::UpdateVideoTexture(void)
{
    if( !m_RenderData.Pixels.GetCount() )
    {
        return FALSE;
    }

    if( !vram_IsValid(m_VideoPlanes[VPX_PLANE_Y]) ||
        (m_VideoPlanes[VPX_PLANE_Y].Desc.Width != (u32)m_RenderData.Width) ||
        (m_VideoPlanes[VPX_PLANE_Y].Desc.Height != (u32)m_RenderData.Height) )
    {
        if( !CreateVideoTextures() )
        {
            return FALSE;
        }
    }

    vram_texture_upload_batch_item Items[3];
    const u8* pData = m_RenderData.Pixels.GetPtr();
    for( s32 plane = 0; plane < 3; ++plane )
    {
        Items[plane].pTexture = &m_VideoPlanes[plane];
        vram_texture_upload_desc& Upload = Items[plane].Upload;
        Upload.Region.Width  = m_VideoPlanes[plane].Desc.Width;
        Upload.Region.Height = m_VideoPlanes[plane].Desc.Height;
        Upload.Region.Depth  = 1;
        Upload.pData         = pData;
        Upload.RowPitch      = Upload.Region.Width;
        Upload.Size          = Upload.RowPitch * Upload.Region.Height;
        Upload.SlicePitch    = Upload.Size;
        Upload.bCycle        = TRUE;
        pData += Upload.Size;
    }

    return vram_UploadTextureBatch(Items, ARRAYSIZE(Items));
}

//==============================================================================

void movie_private::RenderVideoFrame(void)
{
    const shader_resource* pY = vram_GetShaderResource(m_VideoPlanes[VPX_PLANE_Y]);
    const shader_resource* pU = vram_GetShaderResource(m_VideoPlanes[VPX_PLANE_U]);
    const shader_resource* pV = vram_GetShaderResource(m_VideoPlanes[VPX_PLANE_V]);
    if( !pY || !pU || !pV )
    {
        return;
    }

    ui_viewport const& Viewport = g_UIRenderer.GetViewport();
    rect const& Bounds = Viewport.GetLogicalBounds();
    g_UIRenderer.DrawI420(*pY, *pU, *pV, Bounds.Min, Bounds.GetSize());
}

//==============================================================================
