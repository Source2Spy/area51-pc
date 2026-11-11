#ifndef AUDIO_STREAM_MGR_HPP
#define AUDIO_STREAM_MGR_HPP

#include "IOManager/io_mgr.hpp"
#include "Audio/audio_types.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "IOManager/io_request.hpp"

struct audio_runtime;
struct audio_debug_stats;

class audio_stream_mgr
{

//------------------------------------------------------------------------------
// Public functions.

public:

                            audio_stream_mgr        ( void );
                           ~audio_stream_mgr        ( void );
                                                    
            void            Init                    ( audio_runtime& Runtime );
            void            Kill                    ( void );

            void            Update                  ( void );
            void            GetDebugStats           ( audio_debug_stats& Stats ) const;
            void            QueueStreamOpen         ( audio_stream* pStream );

            audio_stream*   AcquireStream           ( u32           WaveformOffset,
                                                      u32           WaveformLength,
                                                      channel*      pLeft,
                                                      channel*      pRight );
            void            ReleaseStream           ( audio_stream* pStream );
            xbool           ReserveStreams          ( s32           nStreams );
            xbool           UnReserveStreams        ( s32           nStreams );

//------------------------------------------------------------------------------

private:

inline      audio_runtime&  Runtime                 ( void ) { ASSERT( m_pRuntime ); return *m_pRuntime; }

            audio_stream    m_AudioStreams[ MAX_AUDIO_STREAMS ];
            audio_runtime*  m_pRuntime;
            uaddr           m_ARAM;
            s32             m_nReservedStreams;
};

//------------------------------------------------------------------------------

#endif // AUDIO_STREAM_MGR_HPP
