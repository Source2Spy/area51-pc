//==============================================================================
//
//  audio_mp3_stream_decoder.hpp
//
//  runtime minimp3 stream decoder.
//
//==============================================================================

#ifndef AUDIO_MP3_STREAM_DECODER_HPP
#define AUDIO_MP3_STREAM_DECODER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_stream_decoder.hpp"
#include "Audio/audio_types.hpp"
#include "audio_mp3_stream_decoder_state.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_stream;
class io_fs;

//==============================================================================
//  AUDIO MP3 STREAM DECODER CLASS
//==============================================================================

class audio_mp3_stream_decoder : public audio_stream_decoder
{
public:
                            audio_mp3_stream_decoder( io_fs&         FileSystem,
                                                      audio_stream*  pStream );

                            virtual ~audio_mp3_stream_decoder( void );

                            xbool         IsValid( void ) const;
                            virtual xbool RequestStop( void );

                            virtual audio_decode_result Decode( audio_stream* pStream,
                                                                s16*          pBufferL,
                                                                s16*          pBufferR,
                                                                s32           nSamples );

                            s32  Read( audio_stream* pStream,
                                       void*         pBuffer,
                                       s32           nBytes );
                            s32  Refill( audio_stream* pStream );
                            s32  DecodeFrame( audio_stream* pStream );
                            void CopySamples( stream_type Type,
                                              s16*&       pOutL,
                                              s16*&       pOutR,
                                              s32         nSamples );

                        private:
                            enum
                            {
                                DECODE_QUEUE_SIZE = 16
                            };

                            struct decoded_chunk
                            {
                                audio_decode_result Result;
                                s16                 Left[audio_mp3_stream_decoder_state::DECODE_SAMPLES];
                                s16                 Right[audio_mp3_stream_decoder_state::DECODE_SAMPLES];
                            };

                            static void         ServiceEntry( void* pData );
                            void                ServiceLoop( void );
                            audio_decode_result DecodeNow( audio_stream* pStream, s16* pBufferL,
                                                           s16* pBufferR, s32 nSamples );

                            io_fs&                         m_FileSystem;
                            audio_mp3_stream_decoder_state m_State;
                            audio_stream                   m_Stream;
                            x_worker_service               m_Service;
                            xsema                          m_Wake;
                            x_atomic_u32                   m_Stop;
                            x_atomic_u32                   m_ReadCursor;
                            x_atomic_u32                   m_WriteCursor;
                            s32                            m_ChunkOffset;
                            xbool                          m_DecodeComplete;
                            decoded_chunk                  m_Chunks[DECODE_QUEUE_SIZE];
};

//==============================================================================
#endif // AUDIO_MP3_STREAM_DECODER_HPP
//==============================================================================
