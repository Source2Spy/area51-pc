//==============================================================================
//
//  MoviePlayer_WebM_Audio.cpp
//
//  Audio decoding for WebM playback using SDL3.
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
#include "x_math.hpp"

#include "MoviePlayer_WebM_Private.hpp"

using namespace movie_webm;

//==============================================================================
// CONSTANTS
//==============================================================================

static constexpr s32 OPUS_SAMPLE_RATE    = 48000;
static constexpr s32 MAX_OPUS_FRAME_MS   = 120;
static constexpr s32 AUDIO_BUFFER_MS     = 250;
static constexpr f64 AUDIO_SYNC_WINDOW   = 0.2;
static constexpr f64 AUDIO_SYNC_RESPONSE = 1.0;
static constexpr f64 AUDIO_SYNC_LIMIT    = 0.25;

//==============================================================================

audio_decoder::audio_decoder(void)
{
    m_Volume               = 1.0f;
    m_CodecType            = CODEC_NONE;
    m_Channels             = 0;
    m_SampleRate           = 0;
    m_isInitialized        = FALSE;
    m_SdlInitialized       = FALSE;
    m_pAudioStream         = NULL;
    m_pWakeSemaphore       = NULL;
    m_pClock               = NULL;
    x_AtomicInit(&m_Error, 0);
    m_ErrorText[0]         = 0;
    m_DevicePeriod         = 0.0;
    m_CodecDelay           = 0;
    m_TimestampTolerance   = 0;

    m_pReader              = NULL;
    m_bHasPendingSample    = FALSE;
    m_PendingFrame         = 0;
    m_BlockOffset          = 0;
    m_BlockStart           = 0;
    m_TrimStart            = 0;
    m_TrimEnd              = 0;
    m_MeasureFrame         = 0;
    m_MeasurePreviousBlockSize = 0;
    m_MeasuredSamples      = 0;
    m_NextSample           = 0;
    m_EndSample            = 0;
    m_bHasSampleTime       = FALSE;

    m_PCMCount             = 0;
    m_QueueRead            = 0;
    m_QueueCount           = 0;
    m_TargetFrames         = 0;
    m_bWaitingForSpace     = FALSE;
    m_bEndOfInput          = FALSE;
    m_bStreamFlushed       = FALSE;

    m_bSyncHold            = FALSE;
    m_OutputSample         = 0;
    m_SyncError            = 0.0;
    m_LastCallbackTime     = 0.0;

    m_pOpusDecoder         = NULL;
    m_pOpusMSDecoder       = NULL;
    m_OpusPreSkip          = 0;
    m_OpusPreSkipRemaining = 0;
    m_MaxFrameSamples      = 0;

    x_memset(&m_VorbisInfo, 0, sizeof(m_VorbisInfo));
    x_memset(&m_VorbisComment, 0, sizeof(m_VorbisComment));
    x_memset(&m_VorbisDsp, 0, sizeof(m_VorbisDsp));
    x_memset(&m_VorbisBlock, 0, sizeof(m_VorbisBlock));
    m_bVorbisHeaders       = FALSE;
    m_bVorbisDsp           = FALSE;
    m_bVorbisBlock         = FALSE;
    m_PreviousBlockSize    = 0;
    m_VorbisPacketIndex    = 0;
}

//==============================================================================

audio_decoder::~audio_decoder(void)
{
    Shutdown();
}

//==============================================================================

xbool audio_decoder::Initialize(const player_config& Config, sync_clock& Clock, SDL_Semaphore* pWakeSemaphore)
{
    Shutdown();
    m_pClock = &Clock;
    m_pWakeSemaphore = pWakeSemaphore;
    m_Volume = 1.0f;
    m_Channels = Config.AudioChannels;
    m_SampleRate = Config.AudioSampleRate;
    x_AtomicStoreRelease(&m_Error, 0);
    m_ErrorText[0] = 0;

    xbool initialized = FALSE;
    if( Config.AudioCodecId == "A_OPUS" )
    {
        m_CodecType = CODEC_OPUS;
        initialized = InitializeOpus(Config);
    }
    else if( Config.AudioCodecId == "A_VORBIS" )
    {
        m_CodecType = CODEC_VORBIS;
        initialized = InitializeVorbis(Config);
    }

    if( !initialized || (m_Channels <= 0) || (m_Channels > 255) ||
        (m_SampleRate <= 0) || (Config.CodecDelay < 0) )
    {
        x_DebugMsg("MoviePlayer_WebM: Invalid audio configuration for '%s'.\n", (const char*)Config.AudioCodecId);
        Shutdown();
        return FALSE;
    }

    m_CodecDelay = TimeToSamples(Config.CodecDelay);
    m_TimestampTolerance = (s32)x_min((f64)S32_MAX, (Config.TimecodeScale * m_SampleRate + WEBM_NANOSEC - 1.0) / WEBM_NANOSEC);
    const s64 capacity = ((s64)m_SampleRate * AUDIO_BUFFER_MS + 999) / 1000 + m_MaxFrameSamples;
    if( (capacity <= 0) || (capacity > S32_MAX / (m_Channels * (s32)sizeof(s16))) ||
        (capacity > S32_MAX / (s32)sizeof(s64)) )
    {
        Shutdown();
        return FALSE;
    }

    m_TargetFrames = (s32)capacity - m_MaxFrameSamples;
    m_PCMBuffer.SetCount(m_MaxFrameSamples * m_Channels);
    m_QueuePCM.SetCount((s32)capacity * m_Channels);
    m_QueueTimes.SetCount((s32)capacity);
    m_OutputBuffer.SetCount(m_MaxFrameSamples * m_Channels);

    if( !InitializeSDL() )
    {
        Shutdown();
        return FALSE;
    }

    m_isInitialized = TRUE;
    if( !Reset() )
    {
        Shutdown();
        return FALSE;
    }
    return TRUE;
}

//==============================================================================

void audio_decoder::Shutdown(void)
{
    ShutdownSDL();
    DestroyOpus();
    DestroyVorbis();

    m_CodecType = CODEC_NONE;
    m_isInitialized = FALSE;
    m_bHasPendingSample = FALSE;
    m_pReader = NULL;
    m_PendingSample = sample();

    m_CompressedBuffer.Clear();
    m_PCMBuffer.Clear();
    m_QueuePCM.Clear();
    m_QueueTimes.Clear();
    m_OutputBuffer.Clear();
    m_QueueCount = 0;

    m_pClock = NULL;
    m_pWakeSemaphore = NULL;
}

//==============================================================================

xbool audio_decoder::Reset(void)
{
    if( !Pause() || !SDL_ClearAudioStream(m_pAudioStream) )
    {
        return FALSE;
    }

    xbool result = TRUE;
    if( m_pOpusDecoder )
    {
        result = opus_decoder_ctl(m_pOpusDecoder, OPUS_RESET_STATE) == OPUS_OK;
    }
    if( m_pOpusMSDecoder )
    {
        result = opus_multistream_decoder_ctl(m_pOpusMSDecoder, OPUS_RESET_STATE) == OPUS_OK;
    }
    if( m_CodecType == CODEC_VORBIS )
    {
        result = InitializeVorbisState();
    }

    m_OpusPreSkipRemaining = m_OpusPreSkip;
    m_PreviousBlockSize = 0;

    m_bHasPendingSample = FALSE;
    m_PendingSample = sample();
    m_pReader = NULL;
    m_bHasSampleTime = FALSE;
    m_NextSample = 0;
    m_EndSample = 0;

    SDL_LockAudioStream(m_pAudioStream);
    m_QueueRead = 0;
    m_QueueCount = 0;
    m_bWaitingForSpace = FALSE;
    m_bEndOfInput = FALSE;
    m_bStreamFlushed = FALSE;
    m_bSyncHold = FALSE;
    m_OutputSample = 0;

    x_AtomicStoreRelease(&m_Error, 0);
    m_ErrorText[0] = 0;
    ResetSync();
    SDL_UnlockAudioStream(m_pAudioStream);

    return result && !HasError();
}

//==============================================================================

s64 audio_decoder::TimeToSamples(s64 Nanoseconds) const
{
    const s64 seconds = Nanoseconds / 1000000000;
    const s64 fraction = Nanoseconds % 1000000000;
    return seconds * m_SampleRate + (fraction * m_SampleRate + (fraction < 0 ? -500000000 : 500000000)) / 1000000000;
}

//==============================================================================

xbool audio_decoder::ReadFrame(s32 FrameIndex)
{
    const mkvparser::Block::Frame& Frame = m_PendingSample.pBlock->GetFrame(FrameIndex);
    if( (Frame.len <= 0) || (Frame.len > S32_MAX) )
    {
        return FALSE;
    }

    m_CompressedBuffer.SetCount((s32)Frame.len);
    return Frame.Read(m_pReader, m_CompressedBuffer.GetPtr()) == 0;
}

//==============================================================================

s32 audio_decoder::GetPacketSamples(s32& PreviousBlockSize)
{
    if( m_CodecType == CODEC_OPUS )
    {
        return opus_packet_get_nb_samples(m_CompressedBuffer.GetPtr(), m_CompressedBuffer.GetCount(), m_SampleRate);
    }

    ogg_packet packet;
    x_memset(&packet, 0, sizeof(packet));
    packet.packet = m_CompressedBuffer.GetPtr();
    packet.bytes = m_CompressedBuffer.GetCount();
    const long size = vorbis_packet_blocksize(&m_VorbisInfo, &packet);
    if( size <= 0 )
    {
        return -1;
    }

    const s32 count = PreviousBlockSize ? (PreviousBlockSize + (s32)size) / 4 : 0;
    PreviousBlockSize = (s32)size;
    return count;
}

//==============================================================================

xbool audio_decoder::BeginSample(const sample& Sample, mkvparser::IMkvReader* pReader)
{
    if( !m_isInitialized || m_bHasPendingSample || !Sample.pBlock || !pReader ||
        !Sample.pBlock->GetFrameCount() || (Sample.DiscardPadding == (-S64_MAX - 1)) )
    {
        return FALSE;
    }

    m_PendingSample = Sample;
    m_pReader = pReader;
    m_PendingFrame = 0;
    m_BlockOffset = 0;
    m_BlockStart = TimeToSamples(Sample.TimeNanoseconds) - m_CodecDelay;
    if( m_bHasSampleTime && (x_abs(m_BlockStart - m_NextSample) <= m_TimestampTolerance) )
    {
        m_BlockStart = m_NextSample;
    }

    m_TrimStart = Sample.DiscardPadding < 0 ? TimeToSamples(-Sample.DiscardPadding) : 0;
    m_TrimEnd = S64_MAX;
    m_MeasureFrame = 0;
    m_MeasurePreviousBlockSize = m_PreviousBlockSize;
    m_MeasuredSamples = 0;
    m_bHasPendingSample = TRUE;
    return TRUE;
}

//==============================================================================

work_status audio_decoder::Pump(void)
{
    if( HasError() )
    {
        return WORK_ERROR;
    }
    if( !m_bHasPendingSample )
    {
        return WORK_BLOCKED;
    }

    SDL_LockAudioStream(m_pAudioStream);
    const xbool full = m_QueueCount >= m_TargetFrames;
    m_bWaitingForSpace = full;
    SDL_UnlockAudioStream(m_pAudioStream);
    if( full )
    {
        return WORK_BLOCKED;
    }

    const s32 frameCount = m_PendingSample.pBlock->GetFrameCount();
    if( (m_PendingSample.DiscardPadding > 0) && (m_MeasureFrame < frameCount) )
    {
        if( !ReadFrame(m_MeasureFrame) )
        {
            return WORK_ERROR;
        }

        const s32 samples = GetPacketSamples(m_MeasurePreviousBlockSize);
        if( (samples < 0) || (samples > m_MaxFrameSamples) )
        {
            return WORK_ERROR;
        }

        m_MeasuredSamples += samples;
        if( ++m_MeasureFrame == frameCount )
        {
            m_TrimEnd = m_MeasuredSamples - TimeToSamples(m_PendingSample.DiscardPadding);
            if( m_TrimEnd < 0 )
            {
                return WORK_ERROR;
            }
        }

        return WORK_PROGRESS;
    }

    if( !ReadFrame(m_PendingFrame) )
    {
        return WORK_ERROR;
    }

    const xbool decoded = m_CodecType == CODEC_OPUS
                        ? DecodeOpusFrame(m_CompressedBuffer.GetPtr(), m_CompressedBuffer.GetCount())
                        : DecodeVorbisPacket(m_CompressedBuffer.GetPtr(), m_CompressedBuffer.GetCount());
    if( !decoded )
    {
        return WORK_ERROR;
    }

    const xbool last = m_PendingFrame + 1 == frameCount;
    const s64 nextOffset = m_BlockOffset + m_PCMCount;
    if( last && (m_TrimStart > nextOffset) )
    {
        return WORK_ERROR;
    }

    const s32 skip = x_min(m_OpusPreSkipRemaining, m_PCMCount);
    m_OpusPreSkipRemaining -= skip;
    const s64 packetStart = m_BlockStart + m_BlockOffset;
    const s64 begin = x_max((s64)skip, x_max(m_TrimStart - m_BlockOffset, m_EndSample - packetStart));
    const s64 end = x_min((s64)m_PCMCount, m_TrimEnd - m_BlockOffset);
    if( begin < end )
    {
        SDL_LockAudioStream(m_pAudioStream);
        // PCM behind the output cursor no longer has a slot in the timeline.
        const s64 first = x_max(begin, m_OutputSample - packetStart);
        const s32 capacity = m_QueueTimes.GetCount();
        for( s64 i = first; i < end; )
        {
            const s32 slot = (m_QueueRead + m_QueueCount) % capacity;
            const s32 count = x_min((s32)(end - i), capacity - slot);
            x_memcpy(m_QueuePCM.GetPtr() + slot * m_Channels,
                     m_PCMBuffer.GetPtr() + (s32)i * m_Channels,
                     count * m_Channels * (s32)sizeof(s16));
            for( s32 j = 0; j < count; ++j )
            {
                m_QueueTimes[slot + j] = packetStart + i + j;
            }
            m_QueueCount += count;
            i += count;
        }
        SDL_UnlockAudioStream(m_pAudioStream);
        m_EndSample = packetStart + end;
    }

    m_BlockOffset = nextOffset;
    ++m_PendingFrame;
    if( last )
    {
        m_NextSample = x_max(m_NextSample, m_BlockStart + m_BlockOffset);
        m_bHasSampleTime = TRUE;
        m_bHasPendingSample = FALSE;
    }

    return WORK_PROGRESS;
}

//==============================================================================

f64 audio_decoder::GetBufferedTime(void)
{
    SDL_LockAudioStream(m_pAudioStream);
    const s32 frames = m_QueueCount;
    SDL_UnlockAudioStream(m_pAudioStream);
    return (f64)frames / m_SampleRate;
}

//==============================================================================

void audio_decoder::EndInput(void)
{
    if( m_bEndOfInput )
    {
        return;
    }

    SDL_LockAudioStream(m_pAudioStream);
    m_bEndOfInput = TRUE;
    SDL_UnlockAudioStream(m_pAudioStream);
}

//==============================================================================

xbool audio_decoder::IsDrained(void)
{
    SDL_LockAudioStream(m_pAudioStream);
    const s32 queued = SDL_GetAudioStreamQueued(m_pAudioStream);
    const s32 available = SDL_GetAudioStreamAvailable(m_pAudioStream);
    if( (queued < 0) || (available < 0) )
    {
        SetError();
    }

    const xbool drained = m_bEndOfInput && !m_QueueCount && m_bStreamFlushed && !queued && !available;
    SDL_UnlockAudioStream(m_pAudioStream);
    return drained;
}

//==============================================================================

void SDLCALL audio_decoder::AudioCallback(void* pData, SDL_AudioStream* pStream, int AdditionalAmount, int TotalAmount)
{
    (void)pStream;

    audio_decoder* pThis = static_cast<audio_decoder*>(pData);
    if( !pThis->HasError() )
    {
        pThis->OutputPCM(AdditionalAmount, TotalAmount);
    }
}

//==============================================================================

void audio_decoder::SetError(void)
{
    if( HasError() )
    {
        return;
    }

    SDL_strlcpy(m_ErrorText, SDL_GetError(), sizeof(m_ErrorText));
    x_AtomicStoreRelease(&m_Error, 1);
    SDL_SignalSemaphore(m_pWakeSemaphore);
}

//==============================================================================

void audio_decoder::DropExpiredPCM(s64 FirstSample)
{
    const s32 capacity = m_QueueTimes.GetCount();
    s32 begin = 0;
    s32 end = m_QueueCount;

    while( begin < end )
    {
        const s32 middle = begin + (end - begin) / 2;
        if( m_QueueTimes[(m_QueueRead + middle) % capacity] < FirstSample )
        {
            begin = middle + 1;
        }
        else
        {
            end = middle;
        }
    }

    m_QueueRead = (m_QueueRead + begin) % capacity;
    m_QueueCount -= begin;
}

//==============================================================================

void audio_decoder::OutputPCM(s32 AdditionalBytes, s32 TotalBytes)
{
    const s32 queued = SDL_GetAudioStreamQueued(m_pAudioStream);
    if( queued < 0 )
    {
        SetError();
        return;
    }

    const s32 frameBytes   = m_Channels * (s32)sizeof(s16);
    const s64 queuedFrames = queued / frameBytes;
    const f64 now          = m_pClock->GetTime();
    const f64 delta        = x_max(0.0, now - m_LastCallbackTime);
    const s64 clockSample  = (s64)(now * m_SampleRate);

    const f64 streamTime = (f64)(m_OutputSample - queuedFrames) / m_SampleRate;
    f64 error = now - streamTime;
    if( (f64)(clockSample - m_OutputSample) / m_SampleRate > AUDIO_SYNC_LIMIT )
    {
        if( !SDL_ClearAudioStream(m_pAudioStream) )
        {
            SetError();
            return;
        }

        m_OutputSample = clockSample;
        AdditionalBytes = TotalBytes;
        m_SyncError = 0.0;
        error = 0.0;
    }

    if( error < -AUDIO_SYNC_LIMIT )
    {
        m_bSyncHold = TRUE;
        m_SyncError = 0.0;
    }
    if( m_bSyncHold && (error >= -m_DevicePeriod) )
    {
        m_bSyncHold = FALSE;
        m_SyncError = 0.0;
    }
    if( !m_bSyncHold && (x_abs(error) <= AUDIO_SYNC_LIMIT) )
    {
        const f64 weight = x_min(1.0, delta / AUDIO_SYNC_WINDOW);
        m_SyncError += (error - m_SyncError) * weight;
    }

    m_LastCallbackTime = now;
    const f32 ratio = m_bSyncHold || (x_abs(m_SyncError) <= m_DevicePeriod) ? 1.0f
                    : (f32)x_max(0.99, x_min(1.01, 1.0 + m_SyncError / AUDIO_SYNC_RESPONSE));
    if( !SDL_SetAudioStreamFrequencyRatio(m_pAudioStream, ratio) )
    {
        SetError();
        return;
    }

    s32 wanted = m_bSyncHold ? 0 : AdditionalBytes / frameBytes;
    const s32 capacity = m_QueueTimes.GetCount();
    if( m_QueueCount && (m_QueueTimes[m_QueueRead] < m_OutputSample) )
    {
        DropExpiredPCM(m_OutputSample);
    }

    while( wanted > 0 )
    {
        const s32 count = x_min(wanted, m_MaxFrameSamples);
        s32 produced = 0;
        while( produced < count )
        {
            if( !m_QueueCount && m_bEndOfInput )
            {
                break;
            }

            const s32 remaining = count - produced;
            s16* pOutput = m_OutputBuffer.GetPtr() + produced * m_Channels;
            if( m_QueueCount && (m_QueueTimes[m_QueueRead] == m_OutputSample) )
            {
                const s32 limit = x_min(remaining, x_min(m_QueueCount, capacity - m_QueueRead));
                s32 run = 1;
                while( (run < limit) && (m_QueueTimes[m_QueueRead + run] == m_OutputSample + run) )
                {
                    ++run;
                }

                x_memcpy(pOutput, m_QueuePCM.GetPtr() + m_QueueRead * m_Channels, run * frameBytes);
                m_QueueRead = (m_QueueRead + run) % capacity;
                m_QueueCount -= run;
                m_OutputSample += run;
                produced += run;
            }
            else
            {
                const s32 silence = m_QueueCount
                                  ? (s32)x_min((s64)remaining, m_QueueTimes[m_QueueRead] - m_OutputSample)
                                  : remaining;
                x_memset(pOutput, 0, silence * frameBytes);
                m_OutputSample += silence;
                produced += silence;
            }
        }

        if( !produced )
        {
            break;
        }

        if( !SDL_PutAudioStreamData(m_pAudioStream, m_OutputBuffer.GetPtr(), produced * frameBytes) )
        {
            SetError();
            return;
        }

        wanted -= produced;
    }

    if( m_bWaitingForSpace && (m_QueueCount < m_TargetFrames) )
    {
        m_bWaitingForSpace = FALSE;
        SDL_SignalSemaphore(m_pWakeSemaphore);
    }

    if( m_bEndOfInput && !m_QueueCount && !m_bStreamFlushed )
    {
        if( !SDL_FlushAudioStream(m_pAudioStream) )
        {
            SetError();
        }
        else
        {
            m_bStreamFlushed = TRUE;
        }

        SDL_SignalSemaphore(m_pWakeSemaphore);
    }
}

//==============================================================================

void audio_decoder::ResetSync(void)
{
    m_bSyncHold = FALSE;
    m_SyncError = 0.0;
    m_LastCallbackTime = m_pClock->GetTime();
    if( !SDL_SetAudioStreamFrequencyRatio(m_pAudioStream, 1.0f) )
    {
        SetError();
    }
}

//==============================================================================

void audio_decoder::SetVolume(f32 Volume)
{
    m_Volume = x_clamp(Volume, 0.0f, 1.0f);
    if( m_pAudioStream )
    {
        SDL_LockAudioStream(m_pAudioStream);
        if( !SDL_SetAudioStreamGain(m_pAudioStream, m_Volume) )
        {
            SetError();
        }
        SDL_UnlockAudioStream(m_pAudioStream);
    }
}

//==============================================================================

xbool audio_decoder::Pause(void)
{
    if( !m_pAudioStream )
    {
        return TRUE;
    }
    if( !SDL_PauseAudioStreamDevice(m_pAudioStream) )
    {
        return FALSE;
    }

    SDL_LockAudioStream(m_pAudioStream);
    ResetSync();
    SDL_UnlockAudioStream(m_pAudioStream);
    return !HasError();
}

//==============================================================================

xbool audio_decoder::Resume(void)
{
    if( !m_pAudioStream )
    {
        return TRUE;
    }

    SDL_LockAudioStream(m_pAudioStream);
    ResetSync();
    SDL_UnlockAudioStream(m_pAudioStream);
    return !HasError() && SDL_ResumeAudioStreamDevice(m_pAudioStream);
}

//==============================================================================

xbool audio_decoder::InitializeSDL(void)
{
    if( !SDL_InitSubSystem(SDL_INIT_AUDIO) )
    {
        return FALSE;
    }
    m_SdlInitialized = TRUE;

    SDL_AudioSpec spec;
    x_memset(&spec, 0, sizeof(spec));
    spec.format = SDL_AUDIO_S16;
    spec.channels = m_Channels;
    spec.freq = m_SampleRate;
    m_pAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, this);
    if( !m_pAudioStream )
    {
        return FALSE;
    }

    s32 frames = 0;
    if( !SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(m_pAudioStream), &spec, &frames) ||
        (frames <= 0) || (spec.freq <= 0) )
    {
        return FALSE;
    }

    m_DevicePeriod = (f64)frames / spec.freq;
    return SDL_SetAudioStreamGain(m_pAudioStream, m_Volume);
}

//==============================================================================

void audio_decoder::ShutdownSDL(void)
{
    if( m_pAudioStream )
    {
        SDL_PauseAudioStreamDevice(m_pAudioStream);
        SDL_SetAudioStreamGetCallback(m_pAudioStream, NULL, NULL);
        SDL_DestroyAudioStream(m_pAudioStream);
        m_pAudioStream = NULL;
    }

    if( m_SdlInitialized )
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_SdlInitialized = FALSE;
    }
}

//==============================================================================

xbool audio_decoder::InitializeOpus(const player_config& Config)
{
    const s32 size = Config.AudioCodecPrivate.GetCount();
    const u8* pHeader = Config.AudioCodecPrivate.GetPtr();
    if( (size < 19) || (x_memcmp(pHeader, "OpusHead", 8) != 0) || (pHeader[8] > 15) || !pHeader[9] )
    {
        return FALSE;
    }

    m_Channels = pHeader[9];
    m_SampleRate = OPUS_SAMPLE_RATE;
    m_MaxFrameSamples = (m_SampleRate * MAX_OPUS_FRAME_MS) / 1000;
    m_OpusPreSkip = pHeader[10] | (pHeader[11] << 8);

    const s16 gain = (s16)(pHeader[16] | (pHeader[17] << 8));
    s32 error = OPUS_OK;
    if( !pHeader[18] )
    {
        if( m_Channels > 2 )
        {
            return FALSE;
        }
        m_pOpusDecoder = opus_decoder_create(m_SampleRate, m_Channels, &error);
        if( (error != OPUS_OK) || !m_pOpusDecoder )
        {
            return FALSE;
        }
        error = opus_decoder_ctl(m_pOpusDecoder, OPUS_SET_GAIN(gain));
    }
    else
    {
        if( size < 21 + m_Channels )
        {
            return FALSE;
        }
        m_OpusMapping.SetCount(m_Channels);
        x_memcpy(m_OpusMapping.GetPtr(), pHeader + 21, m_Channels);
        m_pOpusMSDecoder = opus_multistream_decoder_create(m_SampleRate, m_Channels, pHeader[19], pHeader[20], m_OpusMapping.GetPtr(), &error);
        if( (error != OPUS_OK) || !m_pOpusMSDecoder )
        {
            return FALSE;
        }
        error = opus_multistream_decoder_ctl(m_pOpusMSDecoder, OPUS_SET_GAIN(gain));
    }

    if( error != OPUS_OK )
    {
        x_DebugMsg("MoviePlayer_WebM: Opus initialization failed (%d).\n", error);
    }
    return error == OPUS_OK;
}

//==============================================================================

void audio_decoder::DestroyOpus(void)
{
    if( m_pOpusDecoder )
    {
        opus_decoder_destroy(m_pOpusDecoder);
    }
    if( m_pOpusMSDecoder )
    {
        opus_multistream_decoder_destroy(m_pOpusMSDecoder);
    }

    m_pOpusDecoder = NULL;
    m_pOpusMSDecoder = NULL;
    m_OpusMapping.Clear();
    m_OpusPreSkip = 0;
    m_OpusPreSkipRemaining = 0;
}

//==============================================================================

xbool audio_decoder::DecodeOpusFrame(const u8* pData, s32 DataSize)
{
    m_PCMCount = m_pOpusMSDecoder
               ? opus_multistream_decode(m_pOpusMSDecoder, pData, DataSize, m_PCMBuffer.GetPtr(), m_MaxFrameSamples, 0)
               : opus_decode(m_pOpusDecoder, pData, DataSize, m_PCMBuffer.GetPtr(), m_MaxFrameSamples, 0);
    if( m_PCMCount < 0 )
    {
        x_DebugMsg("MoviePlayer_WebM: Opus decode failed (%d).\n", m_PCMCount);
    }
    return m_PCMCount >= 0;
}

//==============================================================================

xbool audio_decoder::InitializeVorbis(const player_config& Config)
{
    return ParseVorbisPrivate(Config) && InitializeVorbisState();
}

//==============================================================================

xbool audio_decoder::ParseVorbisPrivate(const player_config& Config)
{
    const s32 privateSize = Config.AudioCodecPrivate.GetCount();
    const u8* pPrivate    = (privateSize > 0) ? Config.AudioCodecPrivate.GetPtr() : NULL;

    if( !pPrivate || (privateSize <= 0) )
    {
        return FALSE;
    }

    const s32 packetCount = (s32)pPrivate[0] + 1;
    if( packetCount != 3 )
    {
        return FALSE;
    }

    const u8* pCurrent = pPrivate + 1;
    const u8* pEnd     = pPrivate + privateSize;

    s32 packetSizes[3] = {0, 0, 0};

    for( s32 i = 0; i < packetCount - 1; ++i )
    {
        s32 length = 0;
        while( pCurrent < pEnd )
        {
            const u8 value = *pCurrent++;
            if( length > privateSize - value )
            {
                return FALSE;
            }
            length += value;
            if( value < 255 )
            {
                break;
            }
        }
        if( (pCurrent == pEnd) && (pCurrent[-1] == 255) )
        {
            return FALSE;
        }
        packetSizes[i] = length;
    }

    for( s32 i = 0; i < packetCount; ++i )
    {
        s32 length = 0;
        if( i < packetCount - 1 )
        {
            length = packetSizes[i];
        }
        else
        {
            length = (s32)(pEnd - pCurrent);
        }

        if( (length < 0) || (length > pEnd - pCurrent) )
        {
            return FALSE;
        }

        m_VorbisHeaders[i].SetCount(length);
        if( length > 0 )
        {
            x_memcpy(m_VorbisHeaders[i].GetPtr(), pCurrent, length);
        }
        pCurrent += length;
    }

    if( m_VorbisHeaders[0].GetCount() < 16 )
    {
        return FALSE;
    }

    const u8* pHeader = m_VorbisHeaders[0].GetPtr();
    if( (pHeader[0] != 0x01) || (x_memcmp(pHeader + 1, "vorbis", 6) != 0) )
    {
        return FALSE;
    }

    const u8 channelCount = pHeader[11];
    if( channelCount > 0 )
    {
        m_Channels = channelCount;
    }

    const s32 sampleRate = (s32)(pHeader[12] | (pHeader[13] << 8) | (pHeader[14] << 16) | (pHeader[15] << 24));
    if( sampleRate > 0 )
    {
        m_SampleRate = sampleRate;
    }

    m_VorbisPacketIndex = 0;
    return TRUE;
}

//==============================================================================

xbool audio_decoder::InitializeVorbisState(void)
{
    DestroyVorbisState(FALSE);
    vorbis_info_init(&m_VorbisInfo);
    vorbis_comment_init(&m_VorbisComment);
    m_bVorbisHeaders = TRUE;

    for( s32 i = 0; i < 3; ++i )
    {
        ogg_packet packet;
        x_memset(&packet, 0, sizeof(packet));
        packet.packet = m_VorbisHeaders[i].GetPtr();
        packet.bytes = m_VorbisHeaders[i].GetCount();
        packet.b_o_s = i == 0;
        packet.packetno = i;

        const s32 result = vorbis_synthesis_headerin(&m_VorbisInfo, &m_VorbisComment, &packet);
        if( result != 0 )
        {
            x_DebugMsg("MoviePlayer_WebM: Vorbis header %d failed (%d).\n", i, result);
            return FALSE;
        }
    }

    m_Channels = m_VorbisInfo.channels;
    m_SampleRate = m_VorbisInfo.rate;
    m_MaxFrameSamples = (s32)vorbis_info_blocksize(&m_VorbisInfo, 1) / 2;

    if( vorbis_synthesis_init(&m_VorbisDsp, &m_VorbisInfo) != 0 )
    {
        return FALSE;
    }
    m_bVorbisDsp = TRUE;

    if( vorbis_block_init(&m_VorbisDsp, &m_VorbisBlock) != 0 )
    {
        return FALSE;
    }
    m_bVorbisBlock = TRUE;

    m_PreviousBlockSize = 0;
    m_VorbisPacketIndex = 2;
    return m_MaxFrameSamples > 0;
}

//==============================================================================

void audio_decoder::DestroyVorbis(void)
{
    DestroyVorbisState(TRUE);
}

//==============================================================================

void audio_decoder::DestroyVorbisState(xbool ClearHeaders)
{
    if( m_bVorbisBlock )
    {
        vorbis_block_clear(&m_VorbisBlock);
    }
    if( m_bVorbisDsp )
    {
        vorbis_dsp_clear(&m_VorbisDsp);
    }
    if( m_bVorbisHeaders )
    {
        vorbis_comment_clear(&m_VorbisComment);
        vorbis_info_clear(&m_VorbisInfo);
    }

    m_bVorbisBlock = FALSE;
    m_bVorbisDsp = FALSE;
    m_bVorbisHeaders = FALSE;

    if( ClearHeaders )
    {
        for( s32 i = 0; i < 3; ++i )
        {
            m_VorbisHeaders[i].Clear();
        }
    }
}

//==============================================================================

xbool audio_decoder::DecodeVorbisPacket(const u8* pData, s32 DataSize)
{
    ogg_packet packet;
    x_memset(&packet, 0, sizeof(packet));
    packet.packet = const_cast<u8*>(pData);
    packet.bytes = DataSize;
    packet.granulepos = -1;
    packet.packetno = ++m_VorbisPacketIndex;
    m_PreviousBlockSize = (s32)vorbis_packet_blocksize(&m_VorbisInfo, &packet);

    const s32 result = vorbis_synthesis(&m_VorbisBlock, &packet);
    if( result != 0 )
    {
        x_DebugMsg("MoviePlayer_WebM: Vorbis decode failed (%d).\n", result);
        return FALSE;
    }

    const s32 blockResult = vorbis_synthesis_blockin(&m_VorbisDsp, &m_VorbisBlock);
    if( blockResult != 0 )
    {
        x_DebugMsg("MoviePlayer_WebM: Vorbis block submission failed (%d).\n", blockResult);
        return FALSE;
    }

    f32** ppPCM = NULL;
    m_PCMCount = vorbis_synthesis_pcmout(&m_VorbisDsp, &ppPCM);
    if( (m_PCMCount < 0) || (m_PCMCount > m_MaxFrameSamples) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < m_PCMCount; ++i )
    {
        for( s32 ch = 0; ch < m_Channels; ++ch )
        {
            const f32 value = x_clamp(ppPCM[ch][i], -1.0f, 1.0f);
            m_PCMBuffer[i * m_Channels + ch] = (s16)(value * (value < 0.0f ? 32768.0f : 32767.0f));
        }
    }

    return vorbis_synthesis_read(&m_VorbisDsp, m_PCMCount) == 0;
}

//==============================================================================
