//==============================================================================
//
//  audio_stream_voice_binder.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_stream_voice_binder.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/audio_stream_decoder_factory.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_stream_mgr.hpp"
#include "Audio/audio_stream_runtime.hpp"
#include "Audio/audio_voice_mgr.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_stream_voice_binder::audio_stream_voice_binder( void )
{
    m_pRuntime = NULL;
}

//==============================================================================

audio_stream_voice_binder::~audio_stream_voice_binder( void )
{
}

//==============================================================================

void audio_stream_voice_binder::Init( audio_runtime& Runtime )
{
    m_pRuntime = &Runtime;
}

//==============================================================================

void audio_stream_voice_binder::Kill( void )
{
    m_pRuntime = NULL;
}

//==============================================================================

voice* audio_stream_voice_binder::UpdateCheckStreams( audio_voice_mgr& Voices, voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_stream_voice_binder::UpdateCheckStreams" );

    element* pHead;
    element* pElement;

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;
    while( pHead != pElement )
    {
        pElement->bProcessed = FALSE;
        pElement = pElement->Link.pNext;
    }

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;
    while( pHead != pElement )
    {
        if( pElement->bProcessed == FALSE )
        {
            pElement->bProcessed = TRUE;

            // Element need to warm up?
            ASSERT( Voices.IsValidElement( pElement ) );
            if( pElement->State == ELEMENT_NEEDS_TO_LOAD )
            {
                ASSERT( pElement->Type == COLD_SAMPLE );

                // Time to warm it up?
                if( (pVoice->CursorTime + 1.5f) >= pElement->DeltaTime )
                {
                    audio_stream* pStream        = NULL;
                    element*      pLeftElement   = NULL;
                    element*      pRightElement  = NULL;
                    channel*      pLeftChannel   = NULL;
                    channel*      pRightChannel  = NULL;

                    // Attempt to acquire a hardware channel.
                    if( pElement->pChannel == NULL )
                    {
                        pLeftElement = pElement;
                        pLeftElement->Params.Priority = 255;

                        if( Runtime().Channels.Acquire( pLeftElement ) )
                        {
                            // Get left channel
                            pLeftChannel = pLeftElement->pChannel;

                            if( (pRightElement = pElement->pStereoElement) != NULL )
                            {
                                pRightElement->Params.Priority = 255;
                                if( Runtime().Channels.Acquire( pRightElement ) )
                                {
                                    // Get right channel
                                    pRightChannel = pRightElement->pChannel;

                                    // Mark it processed
                                    pRightElement->bProcessed = TRUE;
                                }
                                else
                                {
                                    Runtime().Channels.Release( pLeftElement->pChannel );
                                    pLeftElement->pChannel = NULL;
                                    return NULL;
                                    // Should NEVER get here!
                                    ASSERT( 0 );
                                }
                            }
                        }
                        else
                        {
                            return NULL;
                            BREAK;
                            // Should NEVER get here!
                            ASSERT( 0 );
                        }
                    }
                    else
                    {
                        pLeftElement = pElement;
                        pLeftChannel = pLeftElement->pChannel;
                        if( (pRightElement = pElement->pStereoElement) != NULL )
                        {
                            pRightChannel = pRightElement->pChannel;
                        }
                    }

                    // Try to acquire a stream.
                    pStream = Runtime().Streams.AcquireStream( pElement->Sample.pColdSample->WaveformOffset,
                                                            pElement->Sample.pColdSample->WaveformLength,
                                                            pLeftChannel, pRightChannel );

                    if( pStream == COOLING_STREAM )
                    {
                        // Its cooling, so just chill out for a bit...
                    }
                    else if( pStream == NULL )
                    {
                        // Nuke the voice, put it in the freelist if the stream can't be started.
                        // TODO: Fix this so it just removes the elements.
                        if( Voices.FreeVoice( pVoice, TRUE ) )
                        {
                            return NULL;
                        }

                        return pVoice;
                    }
                    else
                    {
                        // AHA! A stream is available!!!
                        ASSERT( pStream->pChannel[ LEFT_CHANNEL ] );
                        if( pStream->pChannel[ LEFT_CHANNEL ] )
                        {
                            // Instantiate the sample.
                            InstantiateStreamSample( pStream, LEFT_CHANNEL );

                            // Mark left as loading, set the aram
                            pLeftElement->State                        = ELEMENT_LOADING;
                            pLeftChannel->StreamData.pStream           = pStream;
                            pLeftChannel->StreamData.StreamControl     = TRUE;
                            pLeftChannel->Sample.pHotSample->AudioRam  = pStream->ARAM[LEFT_CHANNEL][0];
                            pLeftChannel->Sample.pHotSample->LoopStart = 0;
                            pLeftChannel->Sample.pHotSample->LoopEnd   = STREAM_BUFFER_SIZE * 2;

                            // Init the channel.
                            Runtime().Backend.InitChannelStreamed( pLeftChannel );
                        }
                        else
                        {
                            if( Voices.FreeVoice( pVoice, TRUE ) )
                            {
                                return NULL;
                            }

                            return pVoice;
                        }

                        /// Stereo?
                        if( pRightElement )
                        {
                            // Right channel will be the control.
                            // *** This important cause the right channel is operated on   ***
                            // *** last in the update.  The last channel to be operated on ***
                            // *** MUST be the control!!!!                                 ***
                            pLeftChannel->StreamData.StreamControl = FALSE;

                            // Instantiate the sample.
                            ASSERT( pStream->pChannel[RIGHT_CHANNEL] );
                            if( pStream->pChannel[RIGHT_CHANNEL] )
                            {
                                InstantiateStreamSample( pStream, RIGHT_CHANNEL );

                                // Mark right channel as loading, set the aram
                                pRightElement->State                        = ELEMENT_LOADING;
                                pRightChannel->StreamData.pStream           = pStream;
                                pRightChannel->StreamData.StreamControl     = TRUE;
                                pRightChannel->Sample.pHotSample->AudioRam  = pStream->ARAM[RIGHT_CHANNEL][0];
                                pRightChannel->Sample.pHotSample->LoopStart = 0;
                                pRightChannel->Sample.pHotSample->LoopEnd   = STREAM_BUFFER_SIZE * 2;

                                // Init the channel.
                                Runtime().Backend.InitChannelStreamed( pRightChannel );
                            }
                            else
                            {
                                if( Voices.FreeVoice( pVoice, TRUE ) )
                                {
                                    return NULL;
                                }

                                return pVoice;
                            }
                        }

                        // Warm it up!
                        Runtime().Streams.QueueStreamOpen( pStream );
                    }
                }
            }
            // Finished loading?
            else if( pElement->State == ELEMENT_LOADED )
            {
                // Start loading the the second buffer.
                ASSERT( pElement->pChannel->StreamData.pStream );
                if( (!pElement->pChannel) ||
                    (!pElement->pChannel->StreamData.pStream) )
                {
                    if( Voices.FreeVoice( pVoice, TRUE ) )
                    {
                        return NULL;
                    }

                    return pVoice;
                }
                if( !Runtime().Decoders.UsesRuntimeDecode( pElement->pChannel->StreamData.pStream ) &&
                    !pElement->pChannel->StreamData.pStream->StreamDone )
                {
                    Runtime().StreamRuntime.Read( pElement->pChannel->StreamData.pStream );
                }

                // Mark it as ready.
                pElement->State = ELEMENT_READY;

                // Stereo? If so, mark stereo element as ready.
                if( pElement->pStereoElement )
                {
                    pElement->pStereoElement->State = ELEMENT_READY;
                }

                // Element has changed.
                pVoice->Dirty |= audio_voice_mgr::VOICE_DB_ELEMENT_CHANGE;

            }
        }

        // Walk the list.
        pElement = pElement->Link.pNext;
    }

    // Its all good!
    return pVoice;
}

//==============================================================================

void audio_stream_voice_binder::InstantiateStreamSample( audio_stream* pStream, s32 WhichChannel )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_stream_voice_binder::InstantiateStreamSample" );

    switch( WhichChannel )
    {
        case LEFT_CHANNEL:
        {
                ASSERT( pStream->pChannel[LEFT_CHANNEL] );

                // Make a copy of the sample
                pStream->Samples[LEFT_CHANNEL].Sample = *(pStream->pChannel[LEFT_CHANNEL]->Sample.pHotSample);

                // Point channel to the copy now
                pStream->pChannel[LEFT_CHANNEL]->Sample.pHotSample = &pStream->Samples[LEFT_CHANNEL].Sample;
                break;
        }

        case RIGHT_CHANNEL:
        {
                ASSERT( pStream->pChannel[RIGHT_CHANNEL] );

                if( pStream->pChannel[RIGHT_CHANNEL] )
                {
                    // Make a copy of the sample
                    pStream->Samples[RIGHT_CHANNEL].Sample = *(pStream->pChannel[RIGHT_CHANNEL]->Sample.pHotSample);

                    // Point channel to the copy now
                    pStream->pChannel[RIGHT_CHANNEL]->Sample.pHotSample = &pStream->Samples[RIGHT_CHANNEL].Sample;
                }
                break;
        }

        default:
        {
            // Should never get here.
            ASSERT( 0 );
            break;
        }
    }
}
