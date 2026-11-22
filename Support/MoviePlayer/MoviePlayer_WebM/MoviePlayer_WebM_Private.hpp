//==============================================================================
//
//  MoviePlayer_WebM_Private.hpp
//
//  Internal declarations for the WebM movie player backend.
//
//==============================================================================

#ifndef MOVIEPLAYER_WEBM_PRIVATE_HPP
#define MOVIEPLAYER_WEBM_PRIVATE_HPP

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
#include "Entropy.hpp"

#include "mkvparser/mkvparser.h"
#include "mkvparser/mkvreader.h"
#include "webm/reader.h"
#include "vpx/vpx_image.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#include "SDL3/SDL_audio.h"
#include "opus.h"
#include "opus_multistream.h"
#include "vorbis/codec.h"
#include "ogg/ogg.h"

struct SDL_AudioStream;
struct SDL_Semaphore;
struct SDL_Mutex;
struct OpusDecoder;
struct OpusMSDecoder;
struct vorbis_info;
struct vorbis_comment;
struct vorbis_dsp_state;
struct vorbis_block;

namespace movie_webm
{
//==============================================================================
// CONSTANTS
//==============================================================================

static const f64 WEBM_NANOSEC = 1000000000.0;

//==============================================================================
// ENUMS AND STRUCTURES
//==============================================================================

enum stream_Type
{
    STREAm_Type_UNKNOWN = 0,
    STREAm_Type_VIDEO,
    STREAm_Type_AUDIO
};

//------------------------------------------------------------------------------

enum sample_status
{
    SAMPLE_ERROR = -1,
    SAMPLE_END,
    SAMPLE_READY
};

//------------------------------------------------------------------------------

enum decode_status
{
    DECODE_ERROR = -1,
    DECODE_EMPTY,
    DECODE_FRAME
};

//------------------------------------------------------------------------------

enum work_status
{
    WORK_ERROR = -1,
    WORK_BLOCKED,
    WORK_PROGRESS
};

//------------------------------------------------------------------------------

class sync_clock;

struct player_config
{
    player_config(void)
    {
        VideoTrack      = -1;
        AudioTrack      = -1;
        Width           = 0;
        Height          = 0;
        FrameRate       = 0.0f;
        Duration        = 0.0;
        TimecodeScale   = WEBM_NANOSEC;
        HasAudio        = FALSE;
        IsLooped        = FALSE;
        AudioChannels   = 0;
        AudioSampleRate = 0;
        AudioBitDepth   = 16;
        CodecDelay      = 0;
        VideoFrameDuration = 0.0;
        AudioLanguage   = XL_LANG_ENGLISH;
        AudioCodecId.Clear();
        VideoCodecId.Clear();
        AudioCodecPrivate.Clear();
    }

    s32         VideoTrack;
    s32         AudioTrack;
    s32         Width;
    s32         Height;
    f32         FrameRate;
    f64         Duration;
    f64         TimecodeScale;
    xbool       HasAudio;
    xbool       IsLooped;
    xstring     VideoCodecId;
    xstring     AudioCodecId;
    s32         AudioChannels;
    s32         AudioSampleRate;
    s32         AudioBitDepth;
    s64         CodecDelay;
    f64         VideoFrameDuration;
    x_language  AudioLanguage;
    xarray<u8>  AudioCodecPrivate;
};

//------------------------------------------------------------------------------

struct sample
{
    sample(void)
    {
        Type        = STREAm_Type_UNKNOWN;
        pEntry      = NULL;
        pBlock      = NULL;
        pCluster    = NULL;
        TimeSeconds = 0.0;
        TimeNanoseconds = 0;
        IsKeyFrame  = FALSE;
        DiscardPadding = 0;
        FrameIndex = 0;
    }

    stream_Type                 Type;
    const mkvparser::BlockEntry* pEntry;
    const mkvparser::Block*     pBlock;
    const mkvparser::Cluster*   pCluster;
    f64                         TimeSeconds;
    s64                         TimeNanoseconds;
    xbool                       IsKeyFrame;
    s64                         DiscardPadding;
    s32                         FrameIndex;
};

//------------------------------------------------------------------------------

class sample_queue
{
public:
    xbool   IsEmpty(void) const { return m_Count == 0; }
    xbool   IsFull (void) const { return m_Count == CAPACITY; }
    void    Clear  (void) { m_Read = m_Count = 0; }
    const sample* Peek(void) const { return IsEmpty() ? NULL : &m_Samples[m_Read]; }

    void Push(const sample& Sample)
    {
        ASSERT(!IsFull());
        m_Samples[(m_Read + m_Count) % CAPACITY] = Sample;
        ++m_Count;
    }

    sample Pop(void)
    {
        ASSERT(!IsEmpty());
        const sample Sample = m_Samples[m_Read];
        m_Read = (m_Read + 1) % CAPACITY;
        --m_Count;
        return Sample;
    }

private:
    enum 
    { 
        CAPACITY = 64 
    };

    sample  m_Samples[CAPACITY];
    s32     m_Read = 0;
    s32     m_Count = 0;
};

//------------------------------------------------------------------------------

struct render_data
{
    render_data(void)
    {
        Width       = 0;
        Height      = 0;
        bHasNewFrame= FALSE;
        bIsValid    = FALSE;
    }

    xarray<u8>  Pixels;
    s32         Width;
    s32         Height;
    xbool       bHasNewFrame;
    xbool       bIsValid;
    xmutex      DataMutex;
};

//==============================================================================
// FILE READER
//==============================================================================

class file_reader : public mkvparser::IMkvReader
{
public:
                    file_reader     (void);
                   ~file_reader     (void);

    xbool           Open            (const char* pFilename);
    void            Close           (void);

    virtual int     Read            (long long position, long length, unsigned char* buffer);
    virtual int     Length          (long long* total, long long* available);

private:
    X_FILE*         m_pFile;
    long long       m_Length;
};

//==============================================================================
// CONTAINER
//==============================================================================

class container
{
public:
                    container       (void);
                   ~container       (void);

    xbool           Open            (const char* pFilename, player_config& Config);
    void            Close           (void);
    void            Rewind          (void);

    sample_status   PeekSample      (sample& OutSample);
    sample_status   ReadSample      (sample& OutSample);

    s32             GetError        (void) const { return m_Error; }

    mkvparser::IMkvReader* GetReader(void) { return &m_FileReader; }

private:
    sample_status   AcquireSample   (sample& OutSample);
    xbool           InitializeSegment   (player_config& Config);
    xbool           SelectTracks        (player_config& Config);
    xbool           AdvanceCluster      (void);
    xbool           AdvanceEntry        (void);

private:
    file_reader                 m_FileReader;
    mkvparser::Segment*         m_pSegment;
    const mkvparser::Tracks*    m_pTracks;
    const mkvparser::Cluster*   m_pCluster;
    const mkvparser::BlockEntry* m_pEntry;
    f64                         m_TimeScale;
    f64                         m_Duration;
    s32                         m_VideoTrack;
    s32                         m_AudioTrack;
    xbool                       m_bEOF;
    s32                         m_Error;
    xbool                       m_bHasPendingSample;
    sample                      m_PendingSample;
};

//==============================================================================
// VIDEO DECODER
//==============================================================================

class video_decoder
{
public:
                    video_decoder   (void);
                   ~video_decoder   (void);

    xbool           Initialize      (const player_config& Config);
    void            Shutdown        (void);

    decode_status      DecodeSample    (const sample& Sample, mkvparser::IMkvReader* pReader);
    const vpx_image_t& GetDecodedImage(void) const { ASSERT(m_pDecodedImage); return *m_pDecodedImage; }

private:
    xbool           InitializeCodec (const player_config& Config);
    void            DestroyCodec    (void);

private:
    vpx_codec_ctx_t             m_CodecCtx;
    const vpx_codec_iface_t*    m_pCodecIface;
    xbool                       m_bCodecInitialized;
    const vpx_image_t*          m_pDecodedImage;

    xarray<u8>                  m_CompressedBuffer;
};

//==============================================================================
// AUDIO DECODER
//==============================================================================

class audio_decoder
{
public:
                    audio_decoder   (void);
                   ~audio_decoder   (void);

    xbool           Initialize      (const player_config& Config, sync_clock& Clock, SDL_Semaphore* pWakeSemaphore);
    void            Shutdown        (void);
    xbool           Reset           (void);
    xbool           BeginSample     (const sample& Sample, mkvparser::IMkvReader* pReader);
    work_status     Pump            (void);
    xbool           HasPendingSample(void) const { return m_bHasPendingSample; }
    void            EndInput        (void);
    xbool           IsDrained       (void);
    xbool           HasError        (void) const { return x_AtomicLoadAcquire(&m_Error) != 0; }
    const char*     GetError        (void) const { return m_ErrorText; }
    f64             GetEndTime      (void) const { return (f64)m_EndSample / m_SampleRate; }
    f64             GetDevicePeriod (void) const { return m_DevicePeriod; }
    f64             GetBufferedTime (void);

    void            SetVolume       (f32 Volume);
    xbool           Pause           (void);
    xbool           Resume          (void);

private:
    enum codec_type
    {
        CODEC_NONE = 0,
        CODEC_OPUS,
        CODEC_VORBIS
    };

    static void SDLCALL AudioCallback   (void* pData, SDL_AudioStream* pStream, int AdditionalAmount, int TotalAmount);
    void            OutputPCM           (s32 AdditionalBytes, s32 TotalBytes);
    void            DropExpiredPCM      (s64 FirstSample);
    void            ResetSync           (void);
    void            SetError            (void);
    xbool           InitializeSDL       (void);
    void            ShutdownSDL         (void);
    xbool           InitializeOpus      (const player_config& Config);
    void            DestroyOpus         (void);
    xbool           InitializeVorbis    (const player_config& Config);
    xbool           InitializeVorbisState(void);
    xbool           ParseVorbisPrivate  (const player_config& Config);
    void            DestroyVorbis       (void);
    void            DestroyVorbisState  (xbool ClearHeaders);
    xbool           ReadFrame           (s32 FrameIndex);
    s32             GetPacketSamples    (s32& PreviousBlockSize);
    xbool           DecodeOpusFrame     (const u8* pData, s32 DataSize);
    xbool           DecodeVorbisPacket  (const u8* pData, s32 DataSize);
    s64             TimeToSamples       (s64 Nanoseconds) const;

private:
    f32                         m_Volume;
    codec_type                  m_CodecType;
    s32                         m_Channels;
    s32                         m_SampleRate;
    xbool                       m_isInitialized;
    xbool                       m_SdlInitialized;
    SDL_AudioStream*            m_pAudioStream;
    SDL_Semaphore*              m_pWakeSemaphore;
    sync_clock*                 m_pClock;
    x_atomic_s32                m_Error;
    char                        m_ErrorText[256];
    f64                         m_DevicePeriod;
    s64                         m_CodecDelay;
    s32                         m_TimestampTolerance;

    sample                      m_PendingSample;
    mkvparser::IMkvReader*      m_pReader;
    xbool                       m_bHasPendingSample;
    s32                         m_PendingFrame;
    s64                         m_BlockOffset;
    s64                         m_BlockStart;
    s64                         m_TrimStart;
    s64                         m_TrimEnd;
    s32                         m_MeasureFrame;
    s32                         m_MeasurePreviousBlockSize;
    s64                         m_MeasuredSamples;
    s64                         m_NextSample;
    s64                         m_EndSample;
    xbool                       m_bHasSampleTime;
    s32                         m_PCMCount;
    xarray<u8>                  m_CompressedBuffer;
    xarray<s16>                 m_PCMBuffer;

    xarray<s16>                 m_QueuePCM;
    // Queue timestamps are in source sample frames.
    xarray<s64>                 m_QueueTimes;
    xarray<s16>                 m_OutputBuffer;
    s32                         m_QueueRead;
    s32                         m_QueueCount;
    s32                         m_TargetFrames;
    xbool                       m_bWaitingForSpace;
    xbool                       m_bEndOfInput;
    xbool                       m_bStreamFlushed;
    xbool                       m_bSyncHold;
    s64                         m_OutputSample;
    f64                         m_SyncError;
    f64                         m_LastCallbackTime;

    // Opus
    OpusDecoder*                m_pOpusDecoder;
    OpusMSDecoder*              m_pOpusMSDecoder;
    xarray<u8>                  m_OpusMapping;
    s32                         m_OpusPreSkip;
    s32                         m_OpusPreSkipRemaining;
    s32                         m_MaxFrameSamples;

    // Vorbis
    vorbis_info                 m_VorbisInfo;
    vorbis_comment              m_VorbisComment;
    vorbis_dsp_state            m_VorbisDsp;
    vorbis_block                m_VorbisBlock;
    xbool                       m_bVorbisHeaders;
    xbool                       m_bVorbisDsp;
    xbool                       m_bVorbisBlock;
    s32                         m_PreviousBlockSize;
    long long                   m_VorbisPacketIndex;
    xarray<u8>                  m_VorbisHeaders[3];
};

//==============================================================================
// SYNC CLOCK
//==============================================================================

class sync_clock
{
public:
                    sync_clock      (void);
                   ~sync_clock      (void);

    xbool           IsValid         (void) const { return m_pMutex != NULL; }

    void            Start           (void);
    void            Stop            (void);
    void            Pause           (void);
    void            Resume          (void);
    void            Reset           (void);

    f64             GetTime         (void) const;
    xbool           IsRunning       (void) const;
    xbool           IsPaused        (void) const;

private:
    SDL_Mutex*      m_pMutex;
    xtick           m_StartTime;
    xtick           m_PauseTime;
    f64             m_PausedDuration;
    xbool           m_bRunning;
    xbool           m_bPaused;
};

} // namespace movie_webm

//==============================================================================
// MOVIE PRESENTATION
//==============================================================================

//==============================================================================
// MOVIE PRIVATE CLASS
//==============================================================================

class movie_private
{
public:
                    movie_private       (void);
                   ~movie_private       (void);

    void            Init                (void);
    xbool           Open                (const char* pFilename, xbool PlayResident, xbool IsLooped, x_language Language);
    void            Close               (void);
    void            Kill                (void);

    s32             GetWidth            (void)      { return m_Width; }
    s32             GetHeight           (void)      { return m_Height; }
    xbool           IsRunning           (void)      { return x_AtomicLoadAcquire(&m_bThreadRunning); }
    xbool           IsFinished          (void)      { return x_AtomicLoadAcquire(&m_bThreadFinished); }

    void            SetVolume           (f32 Volume);
    void            Pause               (void);
    void            Resume              (void);

    void            Render              (void);

private:
    void            Shutdown            (void);

    static void     WorkerEntry         (void* pData);
    void            ThreadMain          (void);
    f64             ThreadLoop          (void);

    movie_webm::work_status PumpInput   (void);
    movie_webm::work_status PumpVideo   (void);
    xbool           ShouldSkipVideoFrame(f64 FrameTime) const;
    movie_webm::sample GetVideoFrame    (const movie_webm::sample& Sample, s32 FrameIndex) const;
    void            UpdateRenderBuffer  (void);
    void            ResetPlayback       (void);
    xbool           PrimePlayback       (void);
    void            FailPlayback        (const char* pOperation, s32 Code, const movie_webm::sample* pSample = NULL);
    movie_webm::work_status PumpAudio   (void);
    void            ClearPendingSamples (void);
    void            HandleEndOfStream   (void);
    void            WaitForPlayback     (f64 Seconds);

    xbool           CreateVideoTextures (void);
    void            DestroyVideoTextures(void);
    xbool           UpdateVideoTexture  (void);
    void            RenderVideoFrame    (void);

private:
    // Core state
    s32                         m_Width;
    s32                         m_Height;
    f32                         m_Volume;
    xbool                       m_IsLooped;

    // Threading
    x_worker_service            m_WorkerService;
    SDL_Semaphore*              m_pWakeSemaphore;
    x_atomic_s32                m_bCloseRequested;
    x_atomic_s32                m_bThreadExit;
    x_atomic_s32                m_bThreadRunning;
    x_atomic_s32                m_bThreadFinished;
    xbool                       m_bPaused;
    xbool                       m_bPlaybackActive;
    xbool                       m_bInputEOF;
    f64                         m_VideoEndTime;
    f64                         m_VideoFrameDuration;
    xmutex                      m_PlaybackMutex;

    // Rendering
    vram_texture                m_VideoPlanes[3];
    movie_webm::render_data     m_RenderData;

    // Playback helpers
    movie_webm::container       m_Container;
    movie_webm::video_decoder   m_VideoDecoder;
    movie_webm::audio_decoder   m_AudioDecoder;
    movie_webm::sync_clock      m_Clock;
    movie_webm::player_config   m_Config;
    f64                         m_LastVideoTime;
    f64                         m_LastVideoPublishTime;
    f64                         m_LastQueuedVideoTime = 0.0;
    movie_webm::sample          m_VideoSample;
    s32                         m_VideoFrameIndex;
    f64                         m_PendingVideoTime;
    xbool                       m_bHasPendingVideo;
    movie_webm::sample_queue    m_AudioSamples;
    movie_webm::sample_queue    m_VideoSamples;
};

//==============================================================================
#endif // MOVIEPLAYER_WEBM_PRIVATE_HPP
//==============================================================================
