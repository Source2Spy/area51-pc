//==============================================================================
//
//  audio_output_mixer.cpp
//
//  Software channel mixer for audio output backends.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "x_debug.hpp"

#include "Audio/backend/audio_output_mixer.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_output_mixer::audio_output_mixer( void )
{
    Init();
}

//==============================================================================

audio_output_mixer::~audio_output_mixer( void )
{
}

//==============================================================================

void audio_output_mixer::MixChannel( output_channel* pChannel, s32* pL, s32* pR, s32 nDstSamples )
{
    f32 VolumeL = pChannel->VolumeL;
    f32 VolumeR = pChannel->VolumeR;
    s32 NewVolL = (s32)(32767.0f * MINMAX(-1.0f, VolumeL, 1.0f));
    s32 NewVolR = (s32)(32767.0f * MINMAX(-1.0f, VolumeR, 1.0f));
    s32 VolL    = pChannel->MixedVolL;
    s32 VolR    = pChannel->MixedVolR;
    s32 dVolL   = (NewVolL - VolL) / nDstSamples;
    s32 dVolR   = (NewVolR - VolR) / nDstSamples;

    // Mix the samples
    s32 iDst = 0;
    while( (nDstSamples > 0) && (pChannel->State == AUDIO_OUTPUT_PLAY) )
    {
        s32 SrcStop;
        s32 SrcLoopTo;
        xbool StopAtSrcStop = FALSE;

        // Get the stop point in the source samples
        if( pChannel->Looped )
        {
            s32 LoopLength = pChannel->LoopEnd - pChannel->LoopStart;
            if( LoopLength <= 0 )
            {
                pChannel->State = AUDIO_OUTPUT_DONE;
                break;
            }

            s32 StopLocal = pChannel->StopSample - pChannel->LoopBase;
            if( pChannel->StopLoop && (StopLocal <= pChannel->Cursor) )
            {
                pChannel->Looped   = FALSE;
                pChannel->StopLoop = FALSE;
                pChannel->State    = AUDIO_OUTPUT_DONE;
                break;
            }

            if( pChannel->StopLoop && (StopLocal <= pChannel->LoopEnd) )
            {
                SrcStop       = StopLocal;
                SrcLoopTo     = SrcStop - 1;
                StopAtSrcStop = TRUE;

                if( SrcLoopTo < 0 )
                {
                    SrcLoopTo = 0;
                }
            }
            else
            {
                SrcStop   = pChannel->LoopEnd;
                SrcLoopTo = pChannel->LoopStart;
            }
        }
        else
        {
            SrcStop   = pChannel->nSamples;
            SrcLoopTo = pChannel->nSamples-1;
        }

        s32 Ratio  = (s32)(65536.0f * pChannel->SampleRate * pChannel->Pitch / AUDIO_OUTPUT_SAMPLE_RATE);
        s32 RatioI = Ratio >> 16;
        s32 RatioF = Ratio & 65535;

        // Mix the samples into the mix buffers
        s32 iSrc     = pChannel->Cursor;
        s32 iSrcFrac = pChannel->Fraction;
        while( (nDstSamples > 0) && (iSrc < SrcStop) )
        {
//            ASSERT( iSrc >= 0 );
//            ASSERT( iSrc < pChannel->nSamples );
//            ASSERT( iDst < AUDIO_OUTPUT_SAMPLES_PER_FRAME );
//            ASSERT( !pChannel->Looped || (iSrc < pChannel->LoopEnd) );

            s32 iSrc2 = iSrc+1;
            if( iSrc2 >= SrcStop )
            {
                iSrc2 = SrcLoopTo;
            }

            s32 s1 = (pChannel->pData[iSrc ] * (65535-iSrcFrac)) >> 16;
            s32 s2 = (pChannel->pData[iSrc2] * iSrcFrac) >> 16;
            s32 s  = s1 + s2;

            pL[iDst] += (s * VolL) >> 15;
            pR[iDst] += (s * VolR) >> 15;

            iSrc     += RatioI;
            iSrcFrac += RatioF;
            iSrc     += iSrcFrac >> 16;
            iSrcFrac &= 65535;

            iDst++;
            nDstSamples--;

            VolL += dVolL;
            VolR += dVolR;
        }

        // At the end of the sample or the end of the loop?
        bool AtStopEnd  = StopAtSrcStop && (iSrc >= SrcStop);
        bool AtEnd      = !pChannel->Looped && (iSrc >= pChannel->nSamples);
        bool AtLoopEnd  = pChannel->Looped && !StopAtSrcStop && (iSrc >= pChannel->LoopEnd);
        if( AtStopEnd )
        {
            pChannel->Cursor   = SrcStop;
            pChannel->Fraction = 0;
            pChannel->Looped   = FALSE;
            pChannel->StopLoop = FALSE;
            pChannel->State    = AUDIO_OUTPUT_DONE;
        }
        else if( AtLoopEnd )
        {
            s32 LoopLength = pChannel->LoopEnd - pChannel->LoopStart;
            while( iSrc >= pChannel->LoopEnd )
            {
                iSrc -= LoopLength;
                pChannel->LoopBase += LoopLength;
            }

            pChannel->Cursor    = iSrc;
            pChannel->Fraction  = iSrcFrac;
        }
        else if( AtEnd )
        {
            pChannel->Cursor   = pChannel->nSamples;
            pChannel->Fraction = 0;
            pChannel->State    = AUDIO_OUTPUT_DONE;
        }
        else
        {
            pChannel->Cursor   = iSrc;
            pChannel->Fraction = iSrcFrac;
        }
    }

    pChannel->MixedVolL = NewVolL;
    pChannel->MixedVolR = NewVolR;
}

//==============================================================================

s32 audio_output_mixer::HandleToIndex( audio_output_hchannel hChannel )
{
    s32 Index       = (hChannel & 65535) - 1;
    s32 Sequence    = hChannel >> 16;

    if( Index < 0 )
    {
        Index = MAX_CHANNELS;
    }
    else if( Index >= MAX_CHANNELS )
    {
        Index = MAX_CHANNELS;
    }
    else if( m_OutputChannels[Index].Sequence != Sequence )
    {
        Index = MAX_CHANNELS;
    }

    return Index;
}

//==============================================================================

s32 audio_output_mixer::HandleToSnapshotIndex( audio_output_hchannel hChannel ) const
{
    s32 Index = (hChannel & 65535) - 1;

    if( Index < 0 )
    {
        return MAX_CHANNELS;
    }

    if( Index >= MAX_CHANNELS )
    {
        return MAX_CHANNELS;
    }

    if( x_AtomicLoadAcquire( &m_ChannelSnapshots[Index].hChannel ) != hChannel )
    {
        return MAX_CHANNELS;
    }

    return Index;
}

//==============================================================================

void audio_output_mixer::PublishChannelSnapshot( s32 Index )
{
    if( (Index < 0) || (Index >= MAX_CHANNELS) )
    {
        return;
    }

    output_channel& Channel = m_OutputChannels[Index];
    audio_output_hchannel hChannel = (Channel.Sequence << 16) + Index + 1;

    x_AtomicStoreRelease( &m_ChannelSnapshots[Index].Position,    Channel.Cursor );
    x_AtomicStoreRelease( &m_ChannelSnapshots[Index].State,       (s32)Channel.State );

    if( Channel.Allocated )
    {
        x_AtomicStoreRelease( &m_ChannelSnapshots[Index].hChannel, hChannel );
    }
    else
    {
        x_AtomicStoreRelease( &m_ChannelSnapshots[Index].hChannel, 0 );
    }

    x_AtomicStoreRelease( &m_ChannelSnapshots[Index].StartSerial, Channel.StartSerial );
}

//==============================================================================

void audio_output_mixer::Init( void )
{
    x_memset( &m_OutputChannels, 0, sizeof(m_OutputChannels) );

    for( s32 i=0 ; i<MAX_CHANNELS ; i++ )
    {
        x_AtomicStoreRelaxed( &m_ChannelSnapshots[i].hChannel, 0 );
        x_AtomicStoreRelaxed( &m_ChannelSnapshots[i].StartSerial, 0 );
        x_AtomicStoreRelaxed( &m_ChannelSnapshots[i].Position, 0 );
        x_AtomicStoreRelaxed( &m_ChannelSnapshots[i].State,    (s32)AUDIO_OUTPUT_DONE );
    }

    x_AtomicStoreRelaxed( &m_OutputLevel, 0 );
}

//==============================================================================

void audio_output_mixer::ApplyRenderCommand( const audio_render_command& Command )
{
    switch( Command.Type )
    {
    case AUDIO_RENDER_COMMAND_START_CHANNEL:
    {
        s32 Index = HandleToIndex( Command.Start.hChannel );
        if( Index < MAX_CHANNELS )
        {
            output_channel& Channel = m_OutputChannels[Index];

            Channel.StartSerial = Command.Start.StartSerial;
            Channel.pData       = (s16*)Command.Start.pData;
            Channel.Cursor      = 0;
            Channel.Fraction    = 0;
            Channel.Looped      = (Command.Start.LoopCount != 0);
            Channel.LoopStart   = Command.Start.LoopStart;
            Channel.LoopEnd     = Command.Start.LoopEnd;
            Channel.LoopBase    = 0;
            Channel.StopLoop    = FALSE;
            Channel.StopSample  = 0;
            Channel.SampleRate  = Command.Start.SampleRate;
            Channel.nSamples    = Command.Start.nSamples;
            Channel.State       = AUDIO_OUTPUT_PLAY;
            Channel.VolumeL     = Command.Start.VolumeL;
            Channel.VolumeR     = Command.Start.VolumeR;
            Channel.Pitch       = Command.Start.Pitch;
            Channel.MixedVolL   = (s32)(32767.0f * MINMAX(-1.0f, Command.Start.VolumeL, 1.0f));
            Channel.MixedVolR   = (s32)(32767.0f * MINMAX(-1.0f, Command.Start.VolumeR, 1.0f));

            PublishChannelSnapshot( Index );
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_STOP_CHANNEL:
    {
        s32 Index = HandleToIndex( Command.Channel.hChannel );
        if( Index < MAX_CHANNELS )
        {
            m_OutputChannels[Index].State = AUDIO_OUTPUT_STOP;
            PublishChannelSnapshot( Index );
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_PAUSE_CHANNEL:
    {
        s32 Index = HandleToIndex( Command.Channel.hChannel );
        if( (Index < MAX_CHANNELS) && (m_OutputChannels[Index].State == AUDIO_OUTPUT_PLAY) )
        {
            m_OutputChannels[Index].State = AUDIO_OUTPUT_PAUSED;
            PublishChannelSnapshot( Index );
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_RESUME_CHANNEL:
    {
        s32 Index = HandleToIndex( Command.Channel.hChannel );
        if( (Index < MAX_CHANNELS) && (m_OutputChannels[Index].State == AUDIO_OUTPUT_PAUSED) )
        {
            m_OutputChannels[Index].State = AUDIO_OUTPUT_PLAY;
            PublishChannelSnapshot( Index );
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_END_CHANNEL:
    {
        s32 Index = HandleToIndex( Command.Channel.hChannel );
        if( Index < MAX_CHANNELS )
        {
            m_OutputChannels[Index].State = AUDIO_OUTPUT_DONE;
            PublishChannelSnapshot( Index );
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_SET_VOLUME:
    {
        s32 Index = HandleToIndex( Command.Volume.hChannel );
        if( Index < MAX_CHANNELS )
        {
            m_OutputChannels[Index].VolumeL = Command.Volume.VolumeL;
            m_OutputChannels[Index].VolumeR = Command.Volume.VolumeR;
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_SET_PITCH:
    {
        s32 Index = HandleToIndex( Command.Pitch.hChannel );
        if( Index < MAX_CHANNELS )
        {
            m_OutputChannels[Index].Pitch = Command.Pitch.Pitch;
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_STOP_LOOP:
    {
        s32 Index = HandleToIndex( Command.StopLoop.hChannel );
        if( (Index < MAX_CHANNELS) && m_OutputChannels[Index].Looped )
        {
            s32 nSamples = Command.StopLoop.nSamples;
            if( nSamples < 0 )
            {
                nSamples = 0;
            }

            m_OutputChannels[Index].StopLoop   = TRUE;
            m_OutputChannels[Index].StopSample = nSamples;
        }
        break;
    }

    case AUDIO_RENDER_COMMAND_NONE:
    default:
    {
        break;
    }
    }
}

//==============================================================================

s32 audio_output_mixer::MixFrame( s16* pOutput, s32 nSamples )
{
    s32 iChannel;
    s32 i;
    s32 OutputLevel;

    if( !pOutput || (nSamples <= 0) )
    {
        return 0;
    }

    if( nSamples > AUDIO_OUTPUT_SAMPLES_PER_FRAME )
    {
        nSamples = AUDIO_OUTPUT_SAMPLES_PER_FRAME;
    }

    x_memset( m_MixL, 0, nSamples*sizeof(s32) );
    x_memset( m_MixR, 0, nSamples*sizeof(s32) );
    OutputLevel = 0;

    for( iChannel=0 ; iChannel<MAX_CHANNELS ; iChannel++ )
    {
        if( m_OutputChannels[iChannel].State == AUDIO_OUTPUT_PLAY )
        {
            MixChannel( &m_OutputChannels[iChannel], m_MixL, m_MixR, nSamples );
            PublishChannelSnapshot( iChannel );
        }
    }

    for( i=0 ; i<nSamples ; i++ )
    {
        s32 s;

        s = m_MixL[i];
        if( s < -32768 )
        {
            s = -32768;
        }
        if( s >  32767 )
        {
            s =  32767;
        }
        pOutput[i*2] = (s16)s;
        OutputLevel = x_max( OutputLevel, x_abs(s) );

        s = m_MixR[i];
        if( s < -32768 )
        {
            s = -32768;
        }
        if( s >  32767 )
        {
            s =  32767;
        }
        pOutput[i*2+1] = (s16)s;
        OutputLevel = x_max( OutputLevel, x_abs(s) );
    }

    x_AtomicStoreRelease( &m_OutputLevel, OutputLevel );

    return nSamples * sizeof(s16) * 2;
}

//==============================================================================

s32 audio_output_mixer::GetOutputLevel( void )
{
    return x_AtomicLoadAcquire( &m_OutputLevel );
}

//==============================================================================

audio_output_hchannel audio_output_mixer::AllocateChannel( void )
{
    for( s32 i=0 ; i<MAX_CHANNELS ; i++ )
    {
        if( !m_OutputChannels[i].Allocated )
        {
            m_OutputChannels[i].Allocated = TRUE;
            m_OutputChannels[i].Sequence++;
            audio_output_hchannel hChannel = (m_OutputChannels[i].Sequence << 16) + i + 1;
            PublishChannelSnapshot( i );
            CLOG_MESSAGE( AUDIO_OUTPUT_LOGGING_ENABLED, "AudioOutput", "audio_output_mixer::AllocateChannel() = %d", i + 1 );
            return hChannel;
        }
    }

    CLOG_MESSAGE( AUDIO_OUTPUT_LOGGING_ENABLED, "AudioOutput", "audio_output_mixer::AllocateChannel() = %d", 0 );

    return 0;
}

//==============================================================================

u32 audio_output_mixer::ChannelStartSerial( audio_output_hchannel hChannel )
{
    s32 Index = HandleToSnapshotIndex( hChannel );
    if( Index >= MAX_CHANNELS )
    {
        return 0;
    }

    return x_AtomicLoadAcquire( &m_ChannelSnapshots[Index].StartSerial );
}

//==============================================================================

audio_output_state audio_output_mixer::ChannelStatus( audio_output_hchannel hChannel )
{
    s32 Index = HandleToSnapshotIndex( hChannel );
    if( Index >= MAX_CHANNELS )
    {
        return AUDIO_OUTPUT_DONE;
    }

    audio_output_state State = (audio_output_state)x_AtomicLoadAcquire( &m_ChannelSnapshots[Index].State );

    CLOG_MESSAGE( AUDIO_OUTPUT_LOGGING_ENABLED, "AudioOutput", "audio_output_mixer::ChannelStatus( %d ) = %d", Index, State );

    return State;
}

//==============================================================================

s32 audio_output_mixer::ChannelPosition( audio_output_hchannel hChannel )
{
    s32 Index = HandleToSnapshotIndex( hChannel );
    if( Index >= MAX_CHANNELS )
    {
        return 0;
    }

    s32 Position = x_AtomicLoadAcquire( &m_ChannelSnapshots[Index].Position );

    CLOG_MESSAGE( AUDIO_OUTPUT_LOGGING_ENABLED, "AudioOutput", "audio_output_mixer::ChannelPosition( %d )", Index, Position );

    return Position;
}

//==============================================================================
