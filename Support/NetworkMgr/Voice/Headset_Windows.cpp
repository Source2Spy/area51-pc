//==============================================================================
//
//  Headset_Windows.cpp
//
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_WINDOWS)
#error This should only be getting compiled for the PC platform. Check your dependancies.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include <mmsystem.h>

#include "x_log.hpp"
#include "x_memory.hpp"

#include "Headset.hpp"


#include "Speex.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

static const s32 WINDOWS_VOICE_FRAME_COUNT  = 1;
static const s32 WINDOWS_VOICE_BUFFER_COUNT = 4;
static const s32 WINDOWS_VOICE_RETRY_TIME   = 1;

static const s32 WINDOWS_VOICE_ENCODE_BYTES = SPEEX8_BYTES_PER_EFRAME * WINDOWS_VOICE_FRAME_COUNT;
static const s32 WINDOWS_VOICE_DECODE_BYTES = SPEEX8_SAMPLES_PER_FRAME * WINDOWS_VOICE_FRAME_COUNT * sizeof( s16 );
static const s32 WINDOWS_VOICE_FIFO_BYTES   = 256;

//------------------------------------------------------------------------------

enum buffer_state
{
    BUFFER_FREE       = 0,
    BUFFER_QUEUED     = 1,
    BUFFER_READY      = 2,
    BUFFER_PROCESSING = 3,
};

//------------------------------------------------------------------------------

struct windows_voice_state
{
    HWAVEIN         m_Input;
    HWAVEOUT        m_Output;
    WAVEHDR         m_CaptureHeaders [ WINDOWS_VOICE_BUFFER_COUNT ];
    WAVEHDR         m_PlaybackHeaders[ WINDOWS_VOICE_BUFFER_COUNT ];
    byte            m_CaptureBuffers [ WINDOWS_VOICE_BUFFER_COUNT ][ WINDOWS_VOICE_DECODE_BYTES ];
    byte            m_PlaybackBuffers[ WINDOWS_VOICE_BUFFER_COUNT ][ WINDOWS_VOICE_DECODE_BYTES ];
    xbool           m_CapturePrepared [ WINDOWS_VOICE_BUFFER_COUNT ];
    xbool           m_PlaybackPrepared[ WINDOWS_VOICE_BUFFER_COUNT ];
    volatile LONG   m_CaptureState [ WINDOWS_VOICE_BUFFER_COUNT ];
    volatile LONG   m_PlaybackState[ WINDOWS_VOICE_BUFFER_COUNT ];
    u32             m_CaptureGeneration[ WINDOWS_VOICE_BUFFER_COUNT ];
    s32             m_NextCaptureBuffer;
    volatile LONG   m_ShuttingDown;
    f32             m_RetryTime;
};

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
LONG GetBufferState( volatile LONG* pState )
{
    return InterlockedCompareExchange( pState, 0, 0 );
}

//==============================================================================

static 
s32 FindCaptureBuffer( windows_voice_state* pState, WAVEHDR* pHeader )
{
    for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
    {
        if( &pState->m_CaptureHeaders[ i ] == pHeader )
        {
            return i;
        }
    }

    return -1;
}

//==============================================================================

static 
s32 FindPlaybackBuffer( windows_voice_state* pState, WAVEHDR* pHeader )
{
    for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
    {
        if( &pState->m_PlaybackHeaders[ i ] == pHeader )
        {
            return i;
        }
    }

    return -1;
}

//==============================================================================

static 
void CALLBACK WaveInCallback( HWAVEIN hWaveIn,
                              UINT    Message,
                              DWORD_PTR Instance,
                              DWORD_PTR Parameter1,
                              DWORD_PTR Parameter2 )
{
    (void)hWaveIn;
    (void)Parameter2;

    windows_voice_state* pState = reinterpret_cast<windows_voice_state*>( Instance );
    if( (pState == NULL) || (GetBufferState( &pState->m_ShuttingDown ) != FALSE) )
    {
        return;
    }

    if( Message != MM_WIM_DATA )
    {
        return;
    }

    WAVEHDR* pHeader = reinterpret_cast<WAVEHDR*>( Parameter1 );
    s32 BufferIndex = FindCaptureBuffer( pState, pHeader );
    if( BufferIndex >= 0 )
    {
        InterlockedCompareExchange( &pState->m_CaptureState[ BufferIndex ], BUFFER_READY, BUFFER_QUEUED );
    }
}

//==============================================================================

static 
void CALLBACK WaveOutCallback( HWAVEOUT hWaveOut,
                               UINT     Message,
                               DWORD_PTR Instance,
                               DWORD_PTR Parameter1,
                               DWORD_PTR Parameter2 )
{
    (void)hWaveOut;
    (void)Parameter2;

    windows_voice_state* pState = reinterpret_cast<windows_voice_state*>( Instance );
    if( (pState == NULL) || (GetBufferState( &pState->m_ShuttingDown ) != FALSE) )
    {
        return;
    }

    if( Message != MM_WOM_DONE )
    {
        return;
    }

    WAVEHDR* pHeader = reinterpret_cast<WAVEHDR*>( Parameter1 );
    s32 BufferIndex = FindPlaybackBuffer( pState, pHeader );
    if( BufferIndex >= 0 )
    {
        InterlockedCompareExchange( &pState->m_PlaybackState[ BufferIndex ], BUFFER_FREE, BUFFER_QUEUED );
    }
}

//==============================================================================

static 
void CloseWindowsAudio( windows_voice_state* pState )
{
    if( pState == NULL )
    {
        return;
    }

    InterlockedExchange( &pState->m_ShuttingDown, TRUE );

    if( pState->m_Input != NULL )
    {
        waveInStop( pState->m_Input );
        waveInReset( pState->m_Input );

        for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
        {
            if( pState->m_CapturePrepared[ i ] )
            {
                waveInUnprepareHeader( pState->m_Input,
                                       &pState->m_CaptureHeaders[ i ],
                                       sizeof( WAVEHDR ) );
                pState->m_CapturePrepared[ i ] = FALSE;
            }
        }

        waveInClose( pState->m_Input );
        pState->m_Input = NULL;
    }

    if( pState->m_Output != NULL )
    {
        waveOutReset( pState->m_Output );

        for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
        {
            if( pState->m_PlaybackPrepared[ i ] )
            {
                waveOutUnprepareHeader( pState->m_Output,
                                        &pState->m_PlaybackHeaders[ i ],
                                        sizeof( WAVEHDR ) );
                pState->m_PlaybackPrepared[ i ] = FALSE;
            }
        }

        waveOutClose( pState->m_Output );
        pState->m_Output = NULL;
    }

    for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
    {
        InterlockedExchange( &pState->m_CaptureState [ i ], BUFFER_FREE );
        InterlockedExchange( &pState->m_PlaybackState[ i ], BUFFER_FREE );
        pState->m_CaptureGeneration[ i ] = 0;
    }
    pState->m_NextCaptureBuffer = 0;
}

//==============================================================================

static 
xbool OpenWindowsAudio( windows_voice_state* pState )
{
    WAVEFORMATEX Format;
    x_memset( &Format, 0, sizeof( Format ) );

    Format.wFormatTag      = WAVE_FORMAT_PCM;
    Format.nChannels       = 1;
    Format.nSamplesPerSec  = VOICE_SAMPLE_RATE;
    Format.wBitsPerSample  = sizeof( s16 ) * 8;
    Format.nBlockAlign     = Format.nChannels * sizeof( s16 );
    Format.nAvgBytesPerSec = Format.nSamplesPerSec * Format.nBlockAlign;

    CloseWindowsAudio( pState );
    InterlockedExchange( &pState->m_ShuttingDown, FALSE );

    MMRESULT Result = waveOutOpen( &pState->m_Output,
                                   WAVE_MAPPER,
                                   &Format,
                                   reinterpret_cast<DWORD_PTR>( WaveOutCallback ),
                                   reinterpret_cast<DWORD_PTR>( pState ),
                                   CALLBACK_FUNCTION );
    if( Result != MMSYSERR_NOERROR )
    {
        CloseWindowsAudio( pState );
        return FALSE;
    }

    Result = waveInOpen( &pState->m_Input,
                         WAVE_MAPPER,
                         &Format,
                         reinterpret_cast<DWORD_PTR>( WaveInCallback ),
                         reinterpret_cast<DWORD_PTR>( pState ),
                         CALLBACK_FUNCTION );
    if( Result != MMSYSERR_NOERROR )
    {
        CloseWindowsAudio( pState );
        return FALSE;
    }

    for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
    {
        WAVEHDR& CaptureHeader = pState->m_CaptureHeaders[ i ];
        x_memset( &CaptureHeader, 0, sizeof( CaptureHeader ) );
        CaptureHeader.lpData         = reinterpret_cast<LPSTR>( pState->m_CaptureBuffers[ i ] );
        CaptureHeader.dwBufferLength = WINDOWS_VOICE_DECODE_BYTES;

        Result = waveInPrepareHeader( pState->m_Input, &CaptureHeader, sizeof( WAVEHDR ) );
        if( Result != MMSYSERR_NOERROR )
        {
            CloseWindowsAudio( pState );
            return FALSE;
        }

        pState->m_CapturePrepared[ i ] = TRUE;
        InterlockedExchange( &pState->m_CaptureState[ i ], BUFFER_QUEUED );

        Result = waveInAddBuffer( pState->m_Input, &CaptureHeader, sizeof( WAVEHDR ) );
        if( Result != MMSYSERR_NOERROR )
        {
            CloseWindowsAudio( pState );
            return FALSE;
        }

        WAVEHDR& PlaybackHeader = pState->m_PlaybackHeaders[ i ];
        x_memset( &PlaybackHeader, 0, sizeof( PlaybackHeader ) );
        PlaybackHeader.lpData         = reinterpret_cast<LPSTR>( pState->m_PlaybackBuffers[ i ] );
        PlaybackHeader.dwBufferLength = WINDOWS_VOICE_DECODE_BYTES;

        Result = waveOutPrepareHeader( pState->m_Output, &PlaybackHeader, sizeof( WAVEHDR ) );
        if( Result != MMSYSERR_NOERROR )
        {
            CloseWindowsAudio( pState );
            return FALSE;
        }

        pState->m_PlaybackPrepared[ i ] = TRUE;
        InterlockedExchange( &pState->m_PlaybackState[ i ], BUFFER_FREE );
    }

    Result = waveInStart( pState->m_Input );
    if( Result != MMSYSERR_NOERROR )
    {
        CloseWindowsAudio( pState );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static 
void CopyAndScaleCaptureSamples( const byte* pSource,
                                        s32         SourceBytes,
                                        s16*        pDestination,
                                        f32         Sensitivity )
{
    x_memset( pDestination, 0, WINDOWS_VOICE_DECODE_BYTES );

    if( (pSource == NULL) || (SourceBytes <= 0) )
    {
        return;
    }

    Sensitivity = MAX( 0.0f, MIN( 1.0f, Sensitivity ) );
    s16 const* pSourceSamples = reinterpret_cast<s16 const*>( pSource );
    s32 SampleCount = MIN( SourceBytes / sizeof( s16 ),
                           WINDOWS_VOICE_DECODE_BYTES / sizeof( s16 ) );

    for( s32 i = 0; i < SampleCount; i++ )
    {
        s32 Sample = static_cast<s32>( pSourceSamples[ i ] * Sensitivity );
        Sample = MAX( -32768, MIN( 32767, Sample ) );
        pDestination[ i ] = static_cast<s16>( Sample );
    }
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void headset::Init( xbool EnableHardware )
{
    m_EncodeBlockSize       = WINDOWS_VOICE_ENCODE_BYTES;
    m_DecodeBlockSize       = WINDOWS_VOICE_DECODE_BYTES;
    m_HeadsetCount          = 0;
    m_HardwareEnabled       = EnableHardware;
    m_ActiveHeadset         = -1;
    m_IsTalking             = FALSE;
    m_TalkingGeneration     = 0;
    m_TalkingRequested      = FALSE;
    m_LoopbackEnabled       = FALSE;
    m_VoiceBanned           = FALSE;
    m_VoiceEnabled          = EnableHardware;
    m_VoiceAudible          = EnableHardware;
    m_VoiceThroughSpeaker   = FALSE;
    m_HeadsetMask           = 0;
    m_HeadsetVolume         = 1.0f;
    m_MicrophoneSensitivity = 1.0f;
    m_pWindowsState         = NULL;

    byte* pBuffer = new byte[ m_DecodeBlockSize +
                              m_EncodeBlockSize +
                              WINDOWS_VOICE_FIFO_BYTES * 2 ];

    m_pDecodeBuffer = pBuffer;
    pBuffer += m_DecodeBlockSize;
    m_pEncodeBuffer = pBuffer;
    pBuffer += m_EncodeBlockSize;

    m_ReadFifo.Init( pBuffer, WINDOWS_VOICE_FIFO_BYTES );
    pBuffer += WINDOWS_VOICE_FIFO_BYTES;
    m_WriteFifo.Init( pBuffer, WINDOWS_VOICE_FIFO_BYTES );

    if( !SpeexInit() )
    {
        LOG_WARNING( "headset::Init", "Unable to initialize the Speex voice codec." );
        m_HardwareEnabled = FALSE;
        m_VoiceEnabled    = FALSE;
        m_VoiceAudible    = FALSE;
        return;
    }

    windows_voice_state* pState = new windows_voice_state;
    x_memset( pState, 0, sizeof( windows_voice_state ) );
    pState->m_Input       = NULL;
    pState->m_Output      = NULL;
    pState->m_ShuttingDown = TRUE;
    pState->m_RetryTime   = 0.0f;
    m_pWindowsState       = pState;

    if( m_HardwareEnabled )
    {
        if( OpenWindowsAudio( pState ) )
        {
            m_HeadsetCount = 1;
            m_HeadsetMask  = 1;
            m_ActiveHeadset = 0;
            UpdateTalkingState();
        }
        else
        {
            LOG_WARNING( "headset::Init", "Unable to open the default Windows voice devices." );
            pState->m_RetryTime = WINDOWS_VOICE_RETRY_TIME;
        }
    }
}

//==============================================================================

void headset::Kill( void )
{
    windows_voice_state* pState = reinterpret_cast<windows_voice_state*>( m_pWindowsState );
    if( pState != NULL )
    {
        CloseWindowsAudio( pState );
        delete pState;
        m_pWindowsState = NULL;
    }

    m_HeadsetCount = 0;
    m_HeadsetMask  = 0;
    UpdateTalkingState();

    m_WriteFifo.Kill();
    m_ReadFifo.Kill();

    delete[] m_pDecodeBuffer;
    m_pDecodeBuffer = NULL;
    m_pEncodeBuffer = NULL;

    SpeexKill();
}

//==============================================================================

void headset::PeriodicUpdate( f32 DeltaTime )
{
    windows_voice_state* pState = reinterpret_cast<windows_voice_state*>( m_pWindowsState );
    if( (pState == NULL) || !m_HardwareEnabled )
    {
        return;
    }

    if( (pState->m_Input == NULL) || (pState->m_Output == NULL) )
    {
        pState->m_RetryTime -= DeltaTime;
        if( pState->m_RetryTime <= 0.0f )
        {
            if( OpenWindowsAudio( pState ) )
            {
                m_HeadsetCount  = 1;
                m_HeadsetMask   = 1;
                m_ActiveHeadset = 0;
                UpdateTalkingState();
            }
            else
            {
                m_HeadsetCount = 0;
                m_HeadsetMask  = 0;
                m_ActiveHeadset = -1;
                UpdateTalkingState();
                pState->m_RetryTime = WINDOWS_VOICE_RETRY_TIME;
            }
        }

        return;
    }

    for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
    {
        s32 BufferIndex = pState->m_NextCaptureBuffer;
        if( InterlockedCompareExchange( &pState->m_CaptureState[ BufferIndex ], BUFFER_PROCESSING, BUFFER_READY ) != BUFFER_READY )
        {
            break;
        }

        WAVEHDR& Header = pState->m_CaptureHeaders[ BufferIndex ];
        CopyAndScaleCaptureSamples( reinterpret_cast<const byte*>( Header.lpData ),
                                     static_cast<s32>( Header.dwBytesRecorded ),
                                     reinterpret_cast<s16*>( m_pDecodeBuffer ),
                                     m_MicrophoneSensitivity );

        s32 EncodedSize = m_EncodeBlockSize;
        xbool Encoded = FALSE;
        if( m_IsTalking && m_VoiceEnabled && !m_VoiceBanned &&
            (pState->m_CaptureGeneration[ BufferIndex ] == m_TalkingGeneration) )
        {
            Encoded = SpeexEncode( reinterpret_cast<s16*>( m_pDecodeBuffer ),
                                   m_DecodeBlockSize,
                                   m_pEncodeBuffer,
                                   &EncodedSize );
            Encoded = Encoded && (EncodedSize == m_EncodeBlockSize);
        }

        if( Encoded )
        {
            m_ReadFifo.Insert( m_pEncodeBuffer, m_EncodeBlockSize, m_EncodeBlockSize );
        }
        else
        {
            m_ReadFifo.Clear();
        }

        Header.dwBytesRecorded = 0;
        pState->m_CaptureGeneration[ BufferIndex ] = m_TalkingGeneration;
        InterlockedExchange( &pState->m_CaptureState[ BufferIndex ], BUFFER_QUEUED );

        MMRESULT Result = waveInAddBuffer( pState->m_Input, &Header, sizeof( WAVEHDR ) );
        if( Result != MMSYSERR_NOERROR )
        {
            LOG_WARNING( "headset::PeriodicUpdate", "waveInAddBuffer failed with result %u.", Result );
            CloseWindowsAudio( pState );
            m_HeadsetCount = 0;
            m_HeadsetMask  = 0;
            m_ActiveHeadset = -1;
            UpdateTalkingState();
            pState->m_RetryTime = WINDOWS_VOICE_RETRY_TIME;
            return;
        }
        pState->m_NextCaptureBuffer = (BufferIndex + 1) % WINDOWS_VOICE_BUFFER_COUNT;
    }
}

//==============================================================================

void headset::Update( f32 DeltaTime )
{
    PeriodicUpdate( DeltaTime );
    UpdateLoopBack();

    windows_voice_state* pState = reinterpret_cast<windows_voice_state*>( m_pWindowsState );
    if( (pState == NULL) ||
        (pState->m_Output == NULL) ||
        !m_HardwareEnabled )
    {
        return;
    }

    if( !m_VoiceEnabled || !m_VoiceAudible || m_VoiceBanned )
    {
        m_WriteFifo.Clear();
        return;
    }

    for( ;; )
    {
        s32 BufferIndex = -1;
        for( s32 i = 0; i < WINDOWS_VOICE_BUFFER_COUNT; i++ )
        {
            if( InterlockedCompareExchange( &pState->m_PlaybackState[ i ], BUFFER_PROCESSING, BUFFER_FREE ) == BUFFER_FREE )
            {
                BufferIndex = i;
                break;
            }
        }

        if( BufferIndex < 0 )
        {
            return;
        }

        if( !m_WriteFifo.Remove( m_pEncodeBuffer, m_EncodeBlockSize, m_EncodeBlockSize ) )
        {
            InterlockedExchange( &pState->m_PlaybackState[ BufferIndex ], BUFFER_FREE );
            return;
        }

        WAVEHDR& Header = pState->m_PlaybackHeaders[ BufferIndex ];
        x_memset( Header.lpData, 0, WINDOWS_VOICE_DECODE_BYTES );

        s32 DecodedSize = m_DecodeBlockSize;
        if( !SpeexDecode( m_pEncodeBuffer,
                          m_EncodeBlockSize,
                          reinterpret_cast<s16*>( Header.lpData ),
                          &DecodedSize ) ||
            (DecodedSize != m_DecodeBlockSize) )
        {
            x_memset( Header.lpData, 0, WINDOWS_VOICE_DECODE_BYTES );
            DecodedSize = m_DecodeBlockSize;
        }

        s16* pSamples = reinterpret_cast<s16*>( Header.lpData );
        s32 SampleCount = DecodedSize / sizeof( s16 );
        for( s32 i = 0; i < SampleCount; i++ )
        {
            pSamples[ i ] = static_cast<s16>( pSamples[ i ] * m_HeadsetVolume );
        }

        Header.dwBufferLength = DecodedSize;
        Header.dwBytesRecorded = DecodedSize;
        InterlockedExchange( &pState->m_PlaybackState[ BufferIndex ], BUFFER_QUEUED );

        MMRESULT Result = waveOutWrite( pState->m_Output, &Header, sizeof( WAVEHDR ) );
        if( Result != MMSYSERR_NOERROR )
        {
            InterlockedExchange( &pState->m_PlaybackState[ BufferIndex ], BUFFER_FREE );
            CloseWindowsAudio( pState );
            m_HeadsetCount = 0;
            m_HeadsetMask  = 0;
            m_ActiveHeadset = -1;
            UpdateTalkingState();
            pState->m_RetryTime = WINDOWS_VOICE_RETRY_TIME;
            return;
        }
    }
}

//==============================================================================

void headset::SetLoopback( xbool IsEnabled )
{
    m_LoopbackEnabled = IsEnabled;
}

//==============================================================================

void headset::ResetEncoder( void )
{
    SpeexReset();
}

//==============================================================================

void headset::SetSpeakerVolume( f32 SpeakerVolume )
{
    m_HeadsetVolume = MAX( 0.0f, MIN( 1.0f, SpeakerVolume ) );
}

//==============================================================================

void headset::OnHeadsetInsert( void )
{
}

//==============================================================================

void headset::OnHeadsetRemove( void )
{
}
