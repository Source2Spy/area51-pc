//==============================================================================
//
//  audio_backend.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_channel_mgr.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_package.hpp"
#include "Audio/audio_helpers.hpp"
#include "Audio/audio_stream_mgr.hpp"
#include "Audio/audio_stream_runtime.hpp"
#include "e_ScratchMem.hpp"
#include "Audio/audio_stream_decoder_factory.hpp"
#include "Audio/backend/audio_output_mixer.hpp"
#include "Audio/backend/sdl/audio_output_sdl.hpp"

#include "Entropy/Entropy.hpp"
#include "x_threads.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define MAX_BACKEND_CHANNELS   (64)

//==============================================================================

static xbool        s_IsInitialized         = FALSE;        // Sentinel
static channel      s_Channels[ MAX_BACKEND_CHANNELS ];    // Channel buffer

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static 
xbool UpdatePosition( audio_runtime& Runtime, channel* pChannel )
{
    // Only need to do special stuff for cold samples
    if( pChannel->Type == COLD_SAMPLE )
    {
        // Played an entire buffers worth of samples?
        if( (pChannel->PrevBufferPosition >= pChannel->MidPoint) &&  
            (pChannel->CurrBufferPosition  <  pChannel->MidPoint) )
        {
            if( Runtime.Decoders.UsesRuntimeDecode( (compression_types)pChannel->Sample.pHotSample->CompressionType ) )
            {
                pChannel->nSamplesBase += STREAM_BUFFER_SIZE;
            }
            else
            {
                // What kind of compression?
                switch( pChannel->Sample.pHotSample->CompressionType )
                {
                    // PCM?
                    case PCM:
                    {
                        // This is one messed up equation...
                        pChannel->nSamplesBase += STREAM_BUFFER_SIZE;
                        break;
                    }

                    default:
                    {
                        ASSERT( 0 );
                        break;
                    }
                }
            }
        }
    
        // Update previous.
        pChannel->PrevBufferPosition = pChannel->CurrBufferPosition;
    }

    // Release position specified?
    if( pChannel->ReleasePosition )
    {
        // Calculate number of samples played.
        pChannel->PlayPosition = Runtime.Backend.GetSamplesPlayed( pChannel );

        // Past the release position?
        if( pChannel->PlayPosition >= pChannel->ReleasePosition )
        {
            // All bad...
            return FALSE;
        }
    }

    // All good!
    return TRUE;
}

//==============================================================================

static 
void UpdateStreamPCM( audio_runtime& Runtime, channel* pChannel )
{
    u32   CurrentPosition  = pChannel->CurrBufferPosition;
    u32   PreviousPosition = pChannel->StreamData.PreviousPosition; 
    s32   Transition;
    xbool bTransition;

    // Update previous.
    pChannel->StreamData.PreviousPosition = CurrentPosition;

    // Which buffer are we in?
    if( PreviousPosition <= pChannel->MidPoint )
    {
        // Did a buffer transition occur?
        bTransition = (CurrentPosition > pChannel->MidPoint);
        Transition  = 1;
    }
    else
    {
        // Did a buffer transition occur?
        bTransition = (CurrentPosition <= pChannel->MidPoint);
        Transition  = 2;
    }

    // Transition occur?
    if( bTransition )
    {
        // Update the base position if buffer wrap occured.
        if( Transition == 2 )
        {
            pChannel->Backend.BasePosition += STREAM_BUFFER_SIZE;
        }

        // Stream done?
        if( pChannel->StreamData.pStream->StreamDone )
        {
        }

        // Need to read from the stream?
        else if( pChannel->StreamData.StreamControl )
        {
            // Fill the read buffer.
            Runtime.StreamRuntime.Read( pChannel->StreamData.pStream );
        }
    }

    if( pChannel->StreamData.pStream && pChannel->StreamData.pStream->StreamDone )
    {
        // Calculate absolute position
        pChannel->Backend.CurrentPosition = pChannel->Backend.BasePosition + pChannel->CurrBufferPosition;

        // Need to release it?
        if( pChannel->Backend.CurrentPosition >= pChannel->Sample.pColdSample->nSamples )
        {
            // Nuke it.
            Runtime.Backend.ReleaseChannel( pChannel );
        }
    }
}

//==============================================================================

static 
void UpdateStreamDecoded( audio_runtime& Runtime, channel* pChannel )
{
    hot_sample* pSample = pChannel->Sample.pHotSample;

    // Did a wrap occur?
    if( (pChannel->StreamData.PreviousPosition >= pChannel->MidPoint) && 
        (pChannel->CurrBufferPosition < pChannel->MidPoint) )
    {
        LOG_MESSAGE( "UpdateStreamDecoded", "Played a BUFFER!" );
        pChannel->Backend.BasePosition += STREAM_BUFFER_SIZE;
    }

    // Update previous.
    pChannel->StreamData.PreviousPosition = pChannel->CurrBufferPosition;

    // Calculate absolute position
    pChannel->Backend.CurrentPosition = pChannel->Backend.BasePosition + pChannel->CurrBufferPosition;

    // Need to release it?
    if( pChannel->Backend.CurrentPosition >= pSample->nSamples )
    {
        if( pChannel->StreamData.pStream && !pChannel->StreamData.pStream->StreamDone )
        {
            return;
        }

        // Nuke it.
        Runtime.Backend.ReleaseChannel( pChannel );
    }
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_backend::audio_backend( void )
{
    m_pOutputMixer  = NULL;
    m_pOutputDevice = NULL;
    m_pRuntime      = NULL;
    m_PitchFactor                = 1.0f;
    m_FirstChannel  = s_Channels;
    m_LastChannel   = s_Channels + (MAX_BACKEND_CHANNELS - 1);
    m_RenderCommandOverflowCount = 0;
}

//==============================================================================

audio_backend::~audio_backend( void )
{
    if( s_IsInitialized )
    {
        Kill();
        return;
    }

    if( m_pOutputDevice )
    {
        m_pOutputDevice->Kill();
        delete m_pOutputDevice;
        m_pOutputDevice = NULL;
    }

    if( m_pOutputMixer )
    {
        delete m_pOutputMixer;
        m_pOutputMixer = NULL;
    }
}

//==============================================================================

s32 audio_backend::NumChannels( void )
{
    return MAX_BACKEND_CHANNELS;
}

//==============================================================================

channel* audio_backend::GetChannelBuffer( void )
{
    return s_Channels;
}

//==============================================================================

s32 audio_backend::RenderCallback( void* pContext, s16* pOutput, s32 nSamples )
{
    audio_backend* pBackend = (audio_backend*)pContext;

    if( !pBackend )
    {
        return 0;
    }

    return pBackend->Render( pOutput, nSamples );
}

//==============================================================================

s32 audio_backend::Render( s16* pOutput, s32 nSamples )
{
    if( !m_pOutputMixer )
    {
        return 0;
    }

    DrainRenderCommands();

    return m_pOutputMixer->MixFrame( pOutput, nSamples );
}

//==============================================================================

void audio_backend::DrainRenderCommands( void )
{
    audio_render_command Command;

    if( !m_pOutputMixer )
    {
        return;
    }

    while( m_RenderCommands.Pop( Command ) )
    {
        m_pOutputMixer->ApplyRenderCommand( Command );
    }
}

//==============================================================================

xbool audio_backend::IsRenderCommandProducerThread( void ) const
{
    if( !m_pRuntime )
    {
        return TRUE;
    }

    if( !m_pRuntime->ServiceRunning )
    {
        return TRUE;
    }

    return (m_pRuntime->ServiceThreadId >= 0) && (x_GetThreadID() == m_pRuntime->ServiceThreadId);
}

//==============================================================================

xbool audio_backend::QueueRenderCommand( const audio_render_command& Command )
{
    ASSERTS( IsRenderCommandProducerThread(), "audio_backend render command queue is SPSC; render commands must be produced by the audio service thread" );

    xbool Result = m_RenderCommands.Push( Command );

    if( !Result )
    {
        m_RenderCommandOverflowCount++;
        FlushRenderCommands();
        Result = m_RenderCommands.Push( Command );
    }

    ASSERTS( Result, "audio_backend render command queue overflow" );

    return Result;
}

//==============================================================================

void audio_backend::FlushRenderCommands( void )
{
    xbool Locked = TRUE;

    if( m_pOutputDevice )
    {
        Locked = m_pOutputDevice->Lock();
    }

    ASSERT( Locked );

    if( Locked )
    {
        DrainRenderCommands();
    }

    if( m_pOutputDevice && Locked )
    {
        m_pOutputDevice->Unlock();
    }
}

//==============================================================================

s32 audio_backend::GetAudioLevel( void )
{
    if( !m_pOutputMixer )
    {
        return 0;
    }

    return m_pOutputMixer->GetOutputLevel();
}

//==============================================================================

void audio_backend::Init( audio_runtime& Runtime, s32 MemSize )
{
    ASSERTS( !s_IsInitialized, "Already initialized" );

    m_pRuntime = &Runtime;
    m_PitchFactor = 1.0f;

    m_pOutputMixer  = new audio_output_mixer;
    m_pOutputDevice = new sdl_audio_device;

    ASSERT( m_pOutputMixer );
    ASSERT( m_pOutputDevice );

    m_pOutputMixer->Init();
    m_RenderCommands.Clear();
    m_RenderCommandOverflowCount = 0;

    (void)MemSize;

    for( s32 i=0 ; i<MAX_BACKEND_CHANNELS; i++ )
    {
        channel& Channel = s_Channels[ i ];
        Channel.Backend.hChannel        = m_pOutputMixer->AllocateChannel();
        Channel.Backend.CurrentPosition = 0;
        Channel.Backend.BasePosition    = 0;
        Channel.Backend.StartSerial     = 0;
        Channel.Backend.StartPending    = FALSE;
        Channel.Backend.InUse           = FALSE;
    }

    m_pOutputDevice->Init( AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLES_PER_FRAME, audio_backend::RenderCallback, this );

    // ok its done.
    s_IsInitialized = TRUE;
}

//==============================================================================

void audio_backend::SetPitchFactor( f32 PitchFactor )
{
    if( m_PitchFactor == PitchFactor )
    {
        return;
    }

    m_PitchFactor = PitchFactor;

    if( !s_IsInitialized )
    {
        return;
    }

    channel* pHead    = Runtime().Channels.UsedList();
    channel* pChannel = pHead->Link.pNext;

    while( pChannel != pHead )
    {
        pChannel->Dirty |= CHANNEL_DB_PITCH;
        pChannel = pChannel->Link.pNext;
    }
}

//==============================================================================

void audio_backend::Kill( void )
{
    ASSERT( s_IsInitialized );

    // Kill audio output
    if( m_pOutputDevice )
    {
        m_pOutputDevice->Kill();
        delete m_pOutputDevice;
        m_pOutputDevice = NULL;
    }

    if( m_pOutputMixer )
    {
        m_RenderCommands.Clear();
        delete m_pOutputMixer;
        m_pOutputMixer = NULL;
    }

    m_pRuntime = NULL;
    s_IsInitialized = FALSE;
}

//==============================================================================

void audio_backend::ResizeMemory( s32 )
{
}

//==============================================================================

void audio_backend::Update( void )
{
    u32         Dirty;
    channel*    pChannel;
    channel*    pHead;
    xbool       bCanStart;
    xbool       bQueueStart = FALSE;

    // Ok to do the hardwre update?
    bCanStart = GetDoBackendUpdate();

    // Clear it.
    ClearDoBackendUpdate();

    // Loop through active channels (starting with lowest priority).
    pHead    = Runtime().Channels.UsedList();
    pChannel = pHead->Link.pPrev;

    while( pChannel != pHead )
    {
        channel* pPrevChannel = pChannel->Link.pPrev;

        // Backend voice to update?
        if( pChannel->Backend.hChannel )
        {
            // Check if anything needs updating for the voice.
            Dirty = pChannel->Dirty;

            if( Dirty )
            {
                // Volume Dirty?
                if( Dirty & (CHANNEL_DB_VOLUME | CHANNEL_DB_PAN) )
                {
                    audio_render_command Command;
                    x_memset( &Command, 0, sizeof(Command) );
                    Command.Type              = AUDIO_RENDER_COMMAND_SET_VOLUME;
                    Command.Volume.hChannel   = pChannel->Backend.hChannel;
                    Command.Volume.VolumeL    = pChannel->Pan3d.GetX() * pChannel->Volume;
                    Command.Volume.VolumeR    = pChannel->Pan3d.GetY() * pChannel->Volume;

                    if( QueueRenderCommand( Command ) )
                    {
                        Dirty &= ~(CHANNEL_DB_VOLUME | CHANNEL_DB_PAN);
                    }
                }

                // Pitch Dirty?
                if( Dirty & CHANNEL_DB_PITCH )
                {
                    audio_render_command Command;
                    x_memset( &Command, 0, sizeof(Command) );
                    Command.Type            = AUDIO_RENDER_COMMAND_SET_PITCH;
                    Command.Pitch.hChannel  = pChannel->Backend.hChannel;
                    Command.Pitch.Pitch     = CalculateChannelPitch( pChannel, m_PitchFactor );

                    if( QueueRenderCommand( Command ) )
                    {
                        // Clear dirty bit.
                        Dirty &= ~CHANNEL_DB_PITCH;
                    }
                }

                if( Dirty & CHANNEL_DB_EFFECTSEND )
                {
                    // TODO: Put Effect send in.

                    // Clear dirty bit.
                    Dirty &= ~CHANNEL_DB_EFFECTSEND;
                }
            }

            // Update the dirty bits.
            pChannel->Dirty = Dirty;

            // Update state machine.
            switch( pChannel->State )
            {
                case STATE_NOT_STARTED:
                {
                    break;
                }

                case STATE_STARTING:
                {
                    xbool bStart = bCanStart;

                    if( pChannel->pElement && pChannel->pElement->pVoice && (pChannel->pElement->pVoice->StartQ==2) )
                    {
                        bStart      = TRUE;
                        bQueueStart = TRUE;
                    }

                    if( bStart )

                    {
                        if( StartChannel( pChannel ) )
                        {
                            pChannel->State = STATE_RUNNING;
                        }
                    }

                    break;
                }

                case STATE_RESUMING:
                {
                    if( bCanStart )
                    {
                        if( ResumeChannel( pChannel ) )
                        {
                            pChannel->State = STATE_RUNNING;
                        }
                    }

                    break;
                }

                case STATE_RUNNING:
                {
                    if( pChannel->Backend.StartPending )
                    {
                        u32 StartSerial = m_pOutputMixer->ChannelStartSerial( pChannel->Backend.hChannel );

                        if( StartSerial == pChannel->Backend.StartSerial )
                        {
                            pChannel->Backend.StartPending = FALSE;
                        }
                    }

                    if( pChannel->Backend.StartPending )
                    {
                        break;
                    }

                    audio_output_state OutputState = m_pOutputMixer->ChannelStatus( pChannel->Backend.hChannel );

                    pChannel->CurrBufferPosition = m_pOutputMixer->ChannelPosition( pChannel->Backend.hChannel );

                    if( !UpdatePosition( Runtime(), pChannel ) || (OutputState == AUDIO_OUTPUT_DONE) )
                    {
                        // Release the channel.
                        if( ReleaseChannel( pChannel ) )
                        {
                            pChannel->State = STATE_STOPPED;
                        }
                    }

                    break;
                }

                case STATE_PAUSING:
                {
                    if( bCanStart )
                    {
                        if( PauseChannel( pChannel ) )
                        {
                            pChannel->State = STATE_PAUSED;
                        }
                    }

                    break;
                }

                case STATE_PAUSED:
                case STATE_STOPPED:
                {
                    break;
                }
            }
        }

        // Previous channel...
        pChannel = pPrevChannel;
    }

    // Special stuff when we start a queued sound.
    if( bQueueStart )
    {
        // Loop through active channels (starting with lowest priority).
        pHead    = Runtime().Channels.UsedList();
        pChannel = pHead->Link.pPrev;

        while( pChannel != pHead )
        {    
            if( pChannel->pElement && pChannel->pElement->pVoice && (pChannel->pElement->pVoice->StartQ==2) )
            {
                pChannel->pElement->pVoice->StartQ = 0;
            }

            // Previous channel...
            pChannel = pChannel->Link.pPrev;
        }
    }
}

//==============================================================================

void* audio_backend::AllocAudioRam( s32 nBytes )
{
    return x_malloc(nBytes);
}

//==============================================================================

void audio_backend::FreeAudioRam( void* Address )
{
    x_free(Address);
}

//==============================================================================

s32 audio_backend::GetAudioRamFree     ( void )
{
    return 0;
}

//==============================================================================

xbool audio_backend::AcquireChannel( channel* pChannel )
{
    ASSERT( !pChannel->Backend.InUse );
    pChannel->Backend.StartPending = FALSE;
    pChannel->Backend.InUse = TRUE;

    // Tell the world!
    return( pChannel->Backend.hChannel != 0 );
}

//==============================================================================

xbool audio_backend::ReleaseChannel( channel* pChannel )
{
    // If the hardware channel is active, stop it!
    //if( pChannel->Backend.hChannel )
    if( pChannel->Backend.InUse )
    {
        audio_render_command Command;
        x_memset( &Command, 0, sizeof(Command) );
        Command.Type             = AUDIO_RENDER_COMMAND_END_CHANNEL;
        Command.Channel.hChannel = pChannel->Backend.hChannel;

        if( !QueueRenderCommand( Command ) )
        {
            return FALSE;
        }

        pChannel->Backend.StartPending = FALSE;
    }

    // Look for segue.
    if( pChannel->pElement && pChannel->pElement->pVoice )
    {
        voice* pVoice = pChannel->pElement->pVoice;
        voice* pNext  = pVoice->pSegueVoiceNext;
        voice* pPrev  = pVoice->pSegueVoicePrev;

        // Take it out of list
        if( pPrev )
        {
            pPrev->pSegueVoiceNext = NULL;
        }

        if( pNext && (pNext->StartQ == 0) )
        {
            // Mark the queued voice to start.
            pNext->StartQ          = 1;
            pNext->pSegueVoicePrev = NULL;
        }
    }

    if( pChannel->Backend.InUse )
    {
        // Does this channel have a stream?
        if( pChannel->StreamData.pStream && pChannel->StreamData.StreamControl )
        {
            // If so, nuke the stream
            Runtime().Streams.ReleaseStream( pChannel->StreamData.pStream );
            pChannel->StreamData.pStream = NULL;
        }

        // No longer in use.
        pChannel->Backend.InUse = FALSE;
    }

    return TRUE;
}

//==============================================================================

void audio_backend::ClearChannel( channel* pChannel )
{
}

//==============================================================================

xbool audio_backend::IsChannelActive( channel* pChannel )
{
    return pChannel->Backend.InUse;
}

//==============================================================================

void audio_backend::InitChannel( channel* pChannel )
{
    hot_sample*      pHotSample = pChannel->Sample.pHotSample;

    pChannel->Backend.CurrentPosition = 0;
    pChannel->Backend.StartPending     = FALSE;
    pChannel->CurrBufferPosition       = 0;
    pChannel->PrevBufferPosition       = 0;

    switch( pHotSample->CompressionType )
    {
    case PCM:
    {
        break;
    }

    case ADPCM:
    default:
    {
        ASSERT( 0 );
        break;
    }
    }
}

//==============================================================================

xbool audio_backend::StartChannel( channel* pChannel )
{
    // If the channel has hardware then start it!
    if( pChannel->Backend.hChannel )
    {
        hot_sample* pHotSample = pChannel->Sample.pHotSample;
        ASSERT( pHotSample );

        if( !pHotSample )
        {
            return FALSE;
        }

        audio_render_command Command;
        x_memset( &Command, 0, sizeof(Command) );
        u32 StartSerial = pChannel->Backend.StartSerial + 1;

        Command.Type                 = AUDIO_RENDER_COMMAND_START_CHANNEL;
        Command.Start.hChannel       = pChannel->Backend.hChannel;
        Command.Start.StartSerial    = StartSerial;
        Command.Start.pData          = (void*)pHotSample->AudioRam;
        Command.Start.SampleRate     = pHotSample->SampleRate;
        Command.Start.VolumeL        = pChannel->Volume * pChannel->Pan3d.GetX();
        Command.Start.VolumeR        = pChannel->Volume * pChannel->Pan3d.GetY();
        Command.Start.Pitch          = CalculateChannelPitch( pChannel, m_PitchFactor );

        if( pChannel->Type == COLD_SAMPLE )
        {
            xbool IsLooped    = (pHotSample->LoopEnd != 0);
            s32   LoopStart   = 0;
            s32   LoopEnd     = STREAM_BUFFER_SIZE;
            s32   SampleBytes = pHotSample->nSamples * 2;
            s32   BufferBytes = STREAM_BUFFER_SIZE * 2;

            if( !Runtime().Decoders.UsesRuntimeDecode( (compression_types)pHotSample->CompressionType ) && (SampleBytes < BufferBytes) )
            {
                IsLooped = FALSE;
                LoopEnd  = SampleBytes / 2;
            }

            Command.Start.nSamples  = LoopEnd;
            Command.Start.LoopCount = IsLooped ? 1 : 0;
            Command.Start.LoopStart = LoopStart;
            Command.Start.LoopEnd   = LoopEnd;
        }
        else
        {
            xbool IsLooped = (pHotSample->LoopEnd > 0);

            switch( pHotSample->CompressionType )
            {
            case PCM:
            {
                Command.Start.nSamples  = pHotSample->nSamples;
                Command.Start.LoopCount = IsLooped ? 1 : 0;
                Command.Start.LoopStart = pHotSample->LoopStart;
                Command.Start.LoopEnd   = pHotSample->LoopEnd;
                break;
            }

            case ADPCM:
            default:
            {
                ASSERT( 0 );
                return FALSE;
            }
            }
        }

        if( QueueRenderCommand( Command ) )
        {
            pChannel->Backend.StartSerial  = StartSerial;
            pChannel->Backend.StartPending = TRUE;
            return TRUE;
        }

        return FALSE;
    }

    return FALSE;
}

//==============================================================================

xbool audio_backend::StopChannel( channel* pChannel )
{
    // If the hardware channel is active, stop it!
    if( pChannel->Backend.hChannel )
    {
        audio_render_command Command;
        x_memset( &Command, 0, sizeof(Command) );
        Command.Type             = AUDIO_RENDER_COMMAND_STOP_CHANNEL;
        Command.Channel.hChannel = pChannel->Backend.hChannel;
        return QueueRenderCommand( Command );
    }

    return TRUE;
}

//==============================================================================

xbool audio_backend::PauseChannel( channel* pChannel )
{
    // If the channel has hardware then start it!
    if( pChannel->Backend.hChannel )
    {
        audio_render_command Command;
        x_memset( &Command, 0, sizeof(Command) );
        Command.Type             = AUDIO_RENDER_COMMAND_PAUSE_CHANNEL;
        Command.Channel.hChannel = pChannel->Backend.hChannel;
        return QueueRenderCommand( Command );
    }

    return TRUE;
}

//==============================================================================

xbool audio_backend::ResumeChannel( channel* pChannel )
{
    // If the hardware channel is active, stop it!
    if( pChannel->Backend.hChannel )
    {
        audio_render_command Command;
        x_memset( &Command, 0, sizeof(Command) );
        Command.Type             = AUDIO_RENDER_COMMAND_RESUME_CHANNEL;
        Command.Channel.hChannel = pChannel->Backend.hChannel;
        return QueueRenderCommand( Command );
    }

    return TRUE;
}

//==============================================================================

u32 audio_backend::GetSamplesPlayed( channel* pChannel )
{
    u32 SamplesPlayed = 0;

    if( pChannel )
    {
        if( Runtime().Decoders.UsesRuntimeDecode( (compression_types)pChannel->Sample.pHotSample->CompressionType ) )
        {
            pChannel->nSamplesAdjust = pChannel->CurrBufferPosition;
        }
        else
        {
            // What kind of compression?
            switch( pChannel->Sample.pHotSample->CompressionType )
            {
                case ADPCM:
                {
                    ASSERT( 0 );
                    break;
                }

                case PCM:
                {
                    pChannel->nSamplesAdjust = pChannel->CurrBufferPosition;
                    break;
                }

                default:
                {
                    ASSERT( 0 );
                    break;
                }
            }
        }

        // Tell the world. (base is always 0 on pc until streaming is implemented).
        SamplesPlayed = pChannel->nSamplesBase + pChannel->nSamplesAdjust;
    }

    return SamplesPlayed;
}

//==============================================================================

void audio_backend::InitChannelStreamed( channel* pChannel )
{
    hot_sample*     pHotSample  = pChannel->Sample.pHotSample;
    xbool           IsLooped    = (pChannel->Sample.pHotSample->LoopEnd != 0);

    // Set the current position
    pChannel->CurrBufferPosition = 0;
    pChannel->PrevBufferPosition = 0;
    pChannel->StreamData.PreviousPosition = 0;

    if( Runtime().Decoders.UsesRuntimeDecode( (compression_types)pHotSample->CompressionType ) )
    {
        x_memset( (void*)pHotSample->AudioRam, 0, STREAM_BUFFER_SIZE * 2 );
    }

    // MUST be looped!
    ASSERT( IsLooped );

    // Set the mid point, clear loop stop.
    pChannel->MidPoint                 = STREAM_BUFFER_SIZE / 2;
    pChannel->Backend.CurrentPosition = 0;
    pChannel->Backend.BasePosition    = 0;
    pChannel->PrevBufferPosition       = 0;

    // Init.
    pChannel->nSamplesAdjust  =
    pChannel->nSamplesBase    =
    pChannel->PlayPosition    =
    pChannel->ReleasePosition = 0;

}

//==============================================================================

xbool audio_backend::StopStreamLoop( channel* pChannel, u32 EndFrame )
{
    ASSERT( pChannel );

    if( (pChannel == NULL) || (pChannel->Backend.hChannel == 0) )
    {
        return FALSE;
    }

    audio_render_command Command;
    x_memset( &Command, 0, sizeof(Command) );
    Command.Type                 = AUDIO_RENDER_COMMAND_STOP_LOOP;
    Command.StopLoop.hChannel    = pChannel->Backend.hChannel;
    Command.StopLoop.nSamples    = EndFrame;
    return QueueRenderCommand( Command );
}

//==============================================================================

void audio_backend::UpdateStream( channel* pChannel )
{
    // Error check.
    ASSERT( pChannel );
    ASSERT( pChannel->StreamData.pStream );

    // Cold, active, running channel?
    if( pChannel && (pChannel->Type == COLD_SAMPLE) && (pChannel->State == STATE_RUNNING) )
    { 
        if( Runtime().Decoders.UsesRuntimeDecode( (compression_types)pChannel->Sample.pHotSample->CompressionType ) )
        {
            UpdateStreamDecoded( Runtime(), pChannel );
        }
        else
        {
            // What kind of compression?
            switch( pChannel->Sample.pHotSample->CompressionType )
            {
                // PCM?
                case PCM:
                {
                    UpdateStreamPCM( Runtime(), pChannel );
                    break;
                }

                default:
                {
                    ASSERT( 0 );
                    break;
                }
            }
        }
    }
}
