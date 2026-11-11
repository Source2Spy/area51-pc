//==============================================================================
//
//  audio_stream_decoder_factory.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_stream_decoder_factory.hpp"
#include "Audio/audio_types.hpp"

#include "Audio/codecs/mp3/audio_mp3_stream_decoder.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_stream_decoder_factory::audio_stream_decoder_factory( io_fs& FileSystem ) :
    m_FileSystem( FileSystem )
{
}

//==============================================================================

audio_stream_decoder_factory::~audio_stream_decoder_factory( void )
{
}

//==============================================================================

void audio_stream_decoder_factory::Init( audio_runtime& Runtime )
{
    (void)Runtime;
}

//==============================================================================

void audio_stream_decoder_factory::Kill( void )
{
}

//==============================================================================

xbool audio_stream_decoder_factory::Open( audio_stream* pStream )
{
    ASSERT( pStream );

    if( pStream == NULL )
    {
        return FALSE;
    }

    ASSERT( pStream->pDecoder == NULL );

    if( pStream->pDecoder != NULL )
    {
        return FALSE;
    }

    xbool Result = FALSE;

    switch( pStream->CompressionType )
    {
        case MP3:
        {
            pStream->WaveformCursor    = 0;
            pStream->StreamDone        = FALSE;
            pStream->DecodeWriteCursor = 0;
            pStream->DecodedFrames     = 0;
            pStream->DecodedEndFrame   = 0;

            if( (pStream->FileHandle == NULL) || (pStream->WaveformLength == 0) )
            {
                Result = FALSE;
            }
            else
            {
                audio_mp3_stream_decoder* pDecoder = new audio_mp3_stream_decoder( m_FileSystem, pStream );
                Result                             = pDecoder && pDecoder->IsValid();

                if( Result )
                {
                    pStream->pDecoder = pDecoder;
                }
                else
                {
                    delete pDecoder;
                }
            }

            break;
        }

        case PCM:
        case ADPCM:
        {
            Result = TRUE;
            break;
        }

        default:
        {
            ASSERT( 0 );
            Result = FALSE;
            break;
        }
    }

    return Result;
}

//==============================================================================

void audio_stream_decoder_factory::Close( audio_stream_decoder* pDecoder )
{
    if( pDecoder == NULL )
    {
        return;
    }

    delete pDecoder;
}

//==============================================================================

xbool audio_stream_decoder_factory::IsOpen( const audio_stream* pStream ) const
{
    return (pStream != NULL) && (pStream->pDecoder != NULL);
}

//==============================================================================

xbool audio_stream_decoder_factory::UsesRuntimeDecode( compression_types CompressionType ) const
{
    switch( CompressionType )
    {
        case MP3: return TRUE; break;
        default:  return FALSE; break;
    }
}

//==============================================================================

xbool audio_stream_decoder_factory::UsesRuntimeDecode( const audio_stream* pStream ) const
{
    if( pStream == NULL )
    {
        return FALSE;
    }

    return UsesRuntimeDecode( pStream->CompressionType );
}
