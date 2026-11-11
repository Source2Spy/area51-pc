//==============================================================================
//
//  audio_backend.hpp
//
//==============================================================================

#ifndef AUDIO_BACKEND_HPP
#define AUDIO_BACKEND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"
#include "Audio/backend/audio_render_command_queue.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_runtime;
class audio_output_mixer;
class sdl_audio_device;

//==============================================================================
//  AUDIO BACKEND CLASS
//==============================================================================

class audio_backend
{
public:
                            audio_backend       ( void );
                           ~audio_backend       ( void );

            s32             NumChannels         ( void );
            channel*        GetChannelBuffer    ( void );
inline      xbool           IsValidChannel      ( channel*      pChannel ) { return (pChannel >= m_FirstChannel) && (pChannel <= m_LastChannel); }
inline      f32             GetTicks            ( void )        { return (f32)m_TickCount * m_TickTime; }
inline      void            Tick                ( void )        { m_TickCount++; }

            void            Init                ( audio_runtime& Runtime,
                                                  s32            MemSize );
            void            Kill                ( void );
            void            ResizeMemory        ( s32 MemSize );
            void            FlushRenderCommands ( void );

            void            Update              ( void );
inline      xbool           GetDoBackendUpdate  ( void )       { return m_bDoBackendUpdate; }
inline      void            SetDoBackendUpdate  ( void )       { m_bDoBackendUpdate = TRUE; }
inline      void            ClearDoBackendUpdate( void )       { m_bDoBackendUpdate = FALSE; }

            void*           AllocAudioRam       ( s32 nBytes );
            void            FreeAudioRam        ( void* pBuffer );
            s32             GetAudioRamFree     ( void );

            xbool           AcquireChannel      ( channel*      pChannel );
            xbool           ReleaseChannel      ( channel*      pChannel );
            void            ClearChannel        ( channel*      pChannel );
            xbool           IsChannelActive     ( channel*      pChannel );
            void            InitChannel         ( channel*      pChannel );
            xbool           StartChannel        ( channel*      pChannel );
            xbool           StopChannel         ( channel*      pChannel );
            xbool           PauseChannel        ( channel*      pChannel );
            xbool           ResumeChannel       ( channel*      pChannel );
            u32             GetSamplesPlayed    ( channel*      pChannel );

            void            SetPitchFactor( f32 PitchFactor );
            void            SetVolumeFactor     ( f32           VolumeFactor )  { m_VolumeFactor = VolumeFactor; }
            f32             GetPitchFactor      ( void )        { return m_PitchFactor; }
            f32             GetVolumeFactor     ( void )        { return m_VolumeFactor; }
            u32             GetRenderCommandOverflowCount( void ) const { return m_RenderCommandOverflowCount; }
            s32             GetAudioLevel       ( void );

            void            InitChannelStreamed ( channel*      pChannel );
            xbool           StopStreamLoop      ( channel*      pChannel,
                                                  u32           EndFrame );
            void            UpdateStream        ( channel*      pChannel );

private:
    enum
    {
        MAX_RENDER_COMMANDS = 1024
    };

inline      audio_runtime&  Runtime                      ( void ) { ASSERT( m_pRuntime ); return *m_pRuntime; }
                                                         
static      s32             RenderCallback               ( void* pContext,
                                                           s16*  pOutput,
                                                           s32   nSamples );
            s32             Render                       ( s16*  pOutput,
                                                           s32   nSamples );
            void            DrainRenderCommands          ( void );
            xbool           QueueRenderCommand           ( const audio_render_command& Command );
            xbool           IsRenderCommandProducerThread( void ) const;

            audio_output_mixer*                             m_pOutputMixer;
            sdl_audio_device*                               m_pOutputDevice;
            audio_render_command_queue<MAX_RENDER_COMMANDS> m_RenderCommands;
            audio_runtime*                                  m_pRuntime;
volatile    u32                                             m_RenderCommandOverflowCount;
volatile    u32                                             m_TickCount;
            f32                                             m_TickTime;
            f32                                             m_PitchFactor;
            f32                                             m_VolumeFactor;
            channel*                                        m_FirstChannel;
            channel*                                        m_LastChannel;
volatile    xbool                                           m_bDoBackendUpdate;
};

//==============================================================================
#endif // AUDIO_BACKEND_HPP
//==============================================================================
