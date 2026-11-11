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

#include <climits>
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
    DECODE_FRAME,
    DECODE_DISCARDED
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

struct render_data
{
    render_data(void)
    {
        pFrameData  = NULL;
        Width       = 0;
        Height      = 0;
        Pitch       = 0;
        FrameTime   = 0.0;
        bHasNewFrame= FALSE;
        bIsValid    = FALSE;
    }

    u8*         pFrameData;
    s32         Width;
    s32         Height;
    s32         Pitch;
    f64         FrameTime;
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

    decode_status   DecodeSample    (const sample& Sample, mkvparser::IMkvReader* pReader, xbool ConvertImage = TRUE);

    const u8*       GetFrameData    (void) const { return (m_FrameBuffer.GetCount() > 0) ? m_FrameBuffer.GetPtr() : NULL; }
    s32             GetFramePitch   (void) const { return m_FramePitch; }
    s32             GetWidth        (void) const { return m_Width; }
    s32             GetHeight       (void) const { return m_Height; }

private:
    xbool           InitializeCodec (const player_config& Config);
    void            DestroyCodec    (void);
    xbool           ConvertFrame    (const vpx_image_t& Image);

private:
    vpx_codec_ctx_t             m_CodecCtx;
    const vpx_codec_iface_t*    m_pCodecIface;
    xbool                       m_bCodecInitialized;

    xarray<u8>                  m_CompressedBuffer;
    xarray<u8>                  m_FrameBuffer;
    s32                         m_FramePitch;
    s32                         m_Width;
    s32                         m_Height;
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
    xbool           Pump            (void);
    xbool           HasPendingSample(void) const { return m_bHasPendingSample; }
    void            EndInput        (void);
    xbool           IsDrained       (void);
    xbool           HasError        (void) const { return x_AtomicLoadAcquire(&m_Error) != 0; }
    const char*     GetError        (void) const { return m_ErrorText; }
    f64             GetEndTime      (void) const { return (f64)m_EndSample / m_SampleRate; }
    f64             GetDevicePeriod (void) const { return m_DevicePeriod; }

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

    static void SDLCALL AudioCallback       (void* pData, SDL_AudioStream* pStream, int AdditionalAmount, int TotalAmount);
    void            OutputPCM           (s32 Bytes);
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
    mkvparser::IMkvReader*       m_pReader;
    xbool                       m_bHasPendingSample;
    s32                         m_PendingFrame;
    s64                         m_BlockOffset;
    s64                         m_BlockStart;
    s64                         m_TrimStart;
    s64                         m_TrimEnd;
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
    xbool                       m_bUnderrun;
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

    movie_webm::decode_status ProcessVideoSample(const movie_webm::sample& Sample);
    movie_webm::sample GetVideoFrame    (const movie_webm::sample& Sample, s32 FrameIndex) const;
    void            UpdateRenderBuffer  (const movie_webm::sample& Sample);
    void            ResetPlayback       (void);
    xbool           PrimePlayback       (void);
    void            FailPlayback        (const char* pOperation, s32 Code, const movie_webm::sample* pSample = NULL);
    xbool           PumpAudio           (void);
    void            HandleEndOfStream   (void);
    void            WaitForPlayback     (f64 Seconds);

    xbool           CreateVideoTexture  (void);
    void            DestroyVideoTexture (void);
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
    vram_texture                m_VideoTexture;
    movie_webm::render_data     m_RenderData;
    xarray<u8>                  m_RenderBuffer;

    // Playback helpers
    movie_webm::container       m_Container;
    movie_webm::video_decoder   m_VideoDecoder;
    movie_webm::audio_decoder   m_AudioDecoder;
    movie_webm::sync_clock      m_Clock;
    movie_webm::player_config   m_Config;
    f64                         m_LastVideoTime;
    movie_webm::sample          m_VideoSample;
    s32                         m_VideoFrameIndex;
    movie_webm::sample          m_PendingVideoSample;
    xbool                       m_bHasPendingVideo;
    xarray<movie_webm::sample>   m_AudioSamples;
    s32                         m_AudioSampleRead;
};

#endif // MOVIEPLAYER_WEBM_PRIVATE_HPP
