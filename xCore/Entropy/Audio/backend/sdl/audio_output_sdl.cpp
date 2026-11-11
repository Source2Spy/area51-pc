//==============================================================================
//
//  audio_output_sdl.cpp
//
//  SDL3 audio output device backend
//
//==============================================================================

#include "x_target.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "x_debug.hpp"

#include "Audio/backend/sdl/audio_output_sdl.hpp"

#include "SDL3/SDL.h"

//==============================================================================
//  DEFINES
//==============================================================================

#ifdef AUDIO_OUTPUT_VERBOSE
#define AUDIO_OUTPUT_LOGGING_ENABLED 1
#else
#define AUDIO_OUTPUT_LOGGING_ENABLED 0
#endif

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

sdl_audio_device::sdl_audio_device( void )
{
    m_Initialized    = FALSE;
    m_SdlInitialized = FALSE;
    m_pAudioStream   = NULL;
    m_pRender        = NULL;
    m_pRenderContext = NULL;
    m_pOutput        = NULL;
    m_FrameSamples   = 0;
}

//==============================================================================

sdl_audio_device::~sdl_audio_device( void )
{
    Kill();
}

//==============================================================================

void sdl_audio_device::AudioOutputStreamCallback( void* pUserData, SDL_AudioStream* pStream, int AdditionalAmount, int )
{
    sdl_audio_device* pDevice = (sdl_audio_device*)pUserData;
    if( !pDevice )
    {
        return;
    }

    while( AdditionalAmount > 0 )
    {
        s32 nSamples = AdditionalAmount / (sizeof(s16) * 2);
        if( nSamples <= 0 )
        {
            nSamples = pDevice->m_FrameSamples;
        }

        if( nSamples > pDevice->m_FrameSamples )
        {
            nSamples = pDevice->m_FrameSamples;
        }

        s32 nBytes = pDevice->MixFrame( nSamples );
        if( nBytes <= 0 )
        {
            break;
        }

        if( !SDL_PutAudioStreamData( pStream, pDevice->m_pOutput, nBytes ) )
        {
            break;
        }

        AdditionalAmount -= nBytes;
    }
}

//==============================================================================

s32 sdl_audio_device::MixFrame( s32 nSamples )
{
    if( !m_Initialized || !m_pAudioStream || !m_pRender || !m_pOutput || (nSamples <= 0) )
    {
        return 0;
    }

    return m_pRender( m_pRenderContext, m_pOutput, nSamples );
}

//==============================================================================

xbool sdl_audio_device::Init( s32 SampleRate, s32 FrameSamples, sdl_audio_render_fn* pRender, void* pRenderContext )
{
    ASSERT( !m_Initialized );
    ASSERT( FrameSamples > 0 );
    ASSERT( pRender );

    CLOG_MESSAGE( AUDIO_OUTPUT_LOGGING_ENABLED, "AudioOutput", "sdl_audio_device::Init()" );

    m_FrameSamples   = FrameSamples;
    m_pRender        = pRender;
    m_pRenderContext = pRenderContext;
    m_pOutput        = (s16*)x_malloc( sizeof(s16) * m_FrameSamples * 2 );
    if( !m_pOutput )
    {
        m_FrameSamples   = 0;
        m_pRender        = NULL;
        m_pRenderContext = NULL;
        return FALSE;
    }

    if( !SDL_InitSubSystem( SDL_INIT_AUDIO ) )
    {
        x_free( m_pOutput );
        m_pOutput        = NULL;
        m_FrameSamples   = 0;
        m_pRender        = NULL;
        m_pRenderContext = NULL;
        return FALSE;
    }

    m_SdlInitialized = TRUE;

    SDL_AudioSpec Spec;
    x_memset( &Spec, 0, sizeof(Spec) );
    Spec.format   = SDL_AUDIO_S16;
    Spec.channels = 2;
    Spec.freq     = SampleRate;

    m_pAudioStream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &Spec, AudioOutputStreamCallback, this );
    if( !m_pAudioStream )
    {
        x_free( m_pOutput );
        m_pOutput        = NULL;
        m_FrameSamples   = 0;
        m_pRender        = NULL;
        m_pRenderContext = NULL;
        m_SdlInitialized = FALSE;
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
        return FALSE;
    }

    m_Initialized = TRUE;

    if( !SDL_ResumeAudioStreamDevice( m_pAudioStream ) )
    {
        Kill();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void sdl_audio_device::Kill( void )
{
    if( !m_Initialized && !m_pAudioStream && !m_SdlInitialized && !m_pOutput && !m_pRender )
    {
        return;
    }

    CLOG_MESSAGE( AUDIO_OUTPUT_LOGGING_ENABLED, "AudioOutput", "sdl_audio_device::Kill()" );

    if( m_pAudioStream )
    {
        SDL_PauseAudioStreamDevice( m_pAudioStream );
        SDL_DestroyAudioStream( m_pAudioStream );
        m_pAudioStream = NULL;
    }

    m_Initialized = FALSE;

    m_pRender        = NULL;
    m_pRenderContext = NULL;
    m_FrameSamples   = 0;

    if( m_pOutput )
    {
        x_free( m_pOutput );
        m_pOutput = NULL;
    }

    if( m_SdlInitialized )
    {
        m_SdlInitialized = FALSE;
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
    }
}

//==============================================================================

xbool sdl_audio_device::Lock( void )
{
    if( !m_pAudioStream )
    {
        return TRUE;
    }

    return SDL_LockAudioStream( m_pAudioStream ) ? TRUE : FALSE;
}

//==============================================================================

void sdl_audio_device::Unlock( void )
{
    if( m_pAudioStream )
    {
        SDL_UnlockAudioStream( m_pAudioStream );
    }
}
