//==============================================================================
//
//  e_Audio.hpp
//
//==============================================================================

#ifndef AUDIO_MGR_HPP
#define AUDIO_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Audio/audio_spatial_types.hpp"

// Public audio handles. The bit layout is private to the audio implementation.
typedef s32 voice_id;
typedef s32 ear_id;

class  audio_package;
class  io_request;
struct audio_runtime;
struct music_intensity;
struct uncompressed_parameters;
struct voice;

//==============================================================================
//  AUDIO MGR CLASS
//==============================================================================

struct audio_request;

struct audio_voice_state
{
    xbool Valid;
    xbool Ready;
    xbool Pending;
    f32   Length;
    f32   PlayTime;
};

//------------------------------------------------------------------------------

struct audio_debug_stats
{
    s32 ActiveChannels;
    s32 ActiveStreams;
    s32 RunningStreams;
    s32 OpeningStreams;
    s32 ReservedStreams;
};

class audio_mgr
{
friend class audio_stream_controller;
friend class audio_stream_runtime;
friend struct audio_runtime;

public:

//------------------------------------------------------------------------------
// Lifecycle.

            xbool           GetDebugStats           ( audio_debug_stats& Stats );

                            audio_mgr               ( void );
                           ~audio_mgr               ( void );

            void            Init                    ( s32 MemSize );
            void            Kill                    ( void );
            void            ResizeMemory            ( s32 NewMemSize );

//------------------------------------------------------------------------------
// Packages.

      const char*           GetLocalizedName        ( const char* pFileName ) const;
            void            SetLanguage             ( x_language Language );
            x_language      GetLanguage             ( void ) const;

            xbool           LoadPackage             ( const char*       pFilename );
            xbool           UnloadPackage           ( const char*       pFilename );
            xbool           IsPackageLoaded         ( const char*       pFilename );
            xbool           LoadPackageStrings      ( const char*       pFilename,
                                                      xarray<xstring>&  Strings );
            void            UnloadAllPackages       ( void );
            void            GetLoadedPackages       ( xarray<xstring>&  Packages );
            void            GetLoadedPackageLookupNames
                                                    ( xarray<xstring>&  Packages );
            void            DisplayPackages         ( void );
            s32             GetPackageARAM          ( const char*       pPackage );
            char*           GetMusicType            ( const char*       pFilename );
            s32             GetMusicIntensity       ( const char*       pFilename,
                                                      music_intensity* &Intensity );

//------------------------------------------------------------------------------
// Descriptors.

            u16*            FindDescriptorByName    ( const char*       pName,
                                                      audio_package**   pPackageResult,
                                                      char* &           DescriptorName );
            xbool           IsValidDescriptor       ( const char*       pIdentifier );
            void            ReMergeIdentifierTables ( void );
            s32             IsCold                  ( char*             pIdentifier );
            u32             GetUserData             ( const char*       pIdentifier );
            s32             GetPriority             ( const char*       pIdentifier );
            f32             GetFarFalloff           ( const char*       pIdentifier );
            f32             GetNearFalloff          ( const char*       pIdentifier );
            f32             GetLengthSeconds        ( const char*       pIdentifier );

//------------------------------------------------------------------------------
// Playback.

            voice_id        Play                    ( const char*       pIdentifier,
                                                      xbool             AutoStart = TRUE );
            voice_id        Play                    ( const char*       pIdentifier,
                                                      const vector3&    Position,
                                                      s32               Zone,
                                                      xbool             AutoStart );
            voice_id        Play                    ( const char*       pIdentifier,
                                                      const vector3&    Position,
                                                      s32               Zone,
                                                      xbool             AutoStart,
                                                      xbool             VolumeClip );

            voice_id        PlayVolumeClipped       ( const char*       pIdentifier,
                                                      const vector3&    Position,
                                                      s32               Zone,
                                                      xbool             AutoStart );

//------------------------------------------------------------------------------
// Queued playback and published state.

            voice_id        PlayConfigured          ( const char*       pIdentifier,
                                                      const vector3&    Position,
                                                      s32               ZoneID,
                                                      xbool             IsPositional,
                                                      f32               Volume,
                                                      f32               FarClipScale );
            xbool           IsVoicePending          ( voice_id          VoiceID ) const;
            xbool           GetVoiceStateCached     ( voice_id          VoiceID,
                                                      audio_voice_state& State );
            f32             GetCurrentPlayTimeCached( voice_id          VoiceID );
            f32             GetLengthSecondsCached  ( voice_id          VoiceID );
            f32             GetVolumeCached         ( voice_id          VoiceID );
            s32             GetPriorityCached       ( voice_id          VoiceID );
            xbool           GetVoiceDescriptorCached( voice_id          VoiceID,
                                                      char*             pName,
                                                      s32               NameSize );
            audio_request*  RequestCreateEar        ( const matrix4&    W2V,
                                                      const vector3&    Position,
                                                      s32               ZoneID,
                                                      f32               Volume );
            xbool           PollCreateEar           ( audio_request*&   pRequest,
                                                      ear_id&           EarID );
            audio_request*  RequestBreakPoints      ( voice_id          VoiceID );
            xbool           PollBreakPoints         ( audio_request*&   pRequest,
                                                      xarray<f32>&      Values );
            void            CancelRequest           ( audio_request*&   pRequest );

//------------------------------------------------------------------------------
// Voice commands.

            xbool           Start                   ( voice_id          VoiceID );

            xbool           Segue                   ( voice_id          VoiceID,
                                                      voice_id          VoiceToQ );

            void            Pause                   ( voice_id          VoiceID );
            void            PauseAll                ( void );
            void            Resume                  ( voice_id          VoiceID );
            void            ResumeAll               ( void );
            void            Release                 ( voice_id          VoiceID,
                                                      f32               Time );
            void            ReleaseAll              ( void );

            xbool           SetReleaseTime          ( voice_id          VoiceID,
                                                      f32               Time );
            xbool           IsReleasing             ( voice_id          VoiceID );
            xbool           GetVoiceState           ( voice_id VoiceID,
                                                      audio_voice_state& State );
            xbool           IsValidVoiceId          ( voice_id          VoiceID );
            // Reads published validity without waiting for queued commands.
            xbool           IsValidVoiceIdCached    ( voice_id          VoiceID );
            xbool           IsVoiceReady            ( voice_id          VoiceID );
            // Reads readiness published by the audio service without waiting.
            xbool           IsVoiceReadyCached      ( voice_id          VoiceID );
            void            SetPitchLock            ( voice_id          VoiceID,
                                                      xbool             bPitchLock );

//------------------------------------------------------------------------------
// Voice queries and properties.

            f32             GetLengthSeconds        ( voice_id          VoiceID );
            const char*     GetVoiceDescriptor      ( voice_id          VoiceID );
            f32             GetVolume               ( voice_id          VoiceID );
            xbool           SetVolume               ( voice_id          VoiceID,
                                                      f32               Volume );
            f32             GetPan                  ( voice_id          VoiceID );
            xbool           SetPan                  ( voice_id          VoiceID,
                                                      f32               Pan );
            f32             GetPitch                ( voice_id          VoiceID );
            xbool           SetPitch                ( voice_id          VoiceID,
                                                      f32               Pitch );
            xbool           GetPosition             ( voice_id          VoiceID,
                                                      vector3&          Position,
                                                      s32&              ZoneID );
            xbool           SetPosition             ( voice_id          VoiceID,
                                                      const vector3&    Position,
                                                      s32               ZoneID );
            xbool           SetFalloff              ( voice_id          VoiceID,
                                                      f32               Near,
                                                      f32               Far );
            f32             GetEffectSend           ( voice_id          VoiceID );
            xbool           SetEffectSend           ( voice_id          VoiceID,
                                                      f32               EffectSend );
            xbool           HasLipSync              ( voice_id          VoiceID );
            f32             GetLipSync              ( voice_id          VoiceID );
            s32             GetBreakPoints          ( voice_id          VoiceID,
                                                      f32* &            BreakPoints );
            xbool           GetIsReady              ( voice_id          VoiceID );
            f32             GetCurrentPlayTime      ( voice_id          VoiceID );
            s32             GetPriority             ( voice_id          VoiceID );
            void            SetVoiceEar             ( voice_id          VoiceID,
                                                      ear_id            EarId );

//------------------------------------------------------------------------------
// Spatial and ears.

            void            SetClip                 ( f32               NearClip,
                                                      f32               FarClip );
            void            GetClip                 ( f32&              NearClip,
                                                      f32&              FarClip );
            void            Calculate3dVolume       ( f32               NearClip,
                                                      f32               FarClip,
                                                      s32               VolumeRolloff,
                                                      const vector3&    WorldPosition,
                                                      s32               ZoneID,
                                                      f32&              Volume );
            ear_id          CreateEar               ( void );
            void            DestroyEar              ( ear_id            EarId );
            void            GetEar                  ( ear_id            EarID,
                                                      matrix4&          W2V,
                                                      vector3&          Position,
                                                      s32&              ZoneID,
                                                      f32&              Volume );
            void            SetEar                  ( ear_id            EarID,
                                                      const matrix4&    W2V,
                                                      const vector3&    Position,
                                                      s32               ZoneID,
                                                      f32               Volume );
            void            UpdateEarZoneVolume     ( ear_id            EarID,
                                                      s32               ZoneID,
                                                      f32               Volume );
            void            UpdateEarZoneVolumes    ( ear_id            EarID,
                                                      f32*              pVolumes );
            xbool           IsEarInBox              ( const bbox& Box );
            ear_id          GetFirstEar             ( void );
            ear_id          GetNextEar              ( void );
            void            ResetCurrentEar         ( void );
            void            SetSpeakerConfig        ( s32               SpeakerConfig );
            s32             GetSpeakerConfig        ( void );

//------------------------------------------------------------------------------
// Global mix settings.

            void            SetMasterVolume         ( f32               Volume );
            void            SetMusicVolume          ( f32               Volume );
            void            SetSFXVolume            ( f32               Volume );
            void            SetVoiceVolume          ( f32               Volume );
            void            SetPitchFactor          ( f32               PitchFactor );
            void            EnableAudioDucking      ( void );
            void            DisableAudioDucking     ( void );
            xbool           IsAudioDuckingEnabled   ( void );
            s32             GetAudioLevel           ( void );
            f32             GetAudioTime            ( void );

//------------------------------------------------------------------------------
// Ticks.

            void            Update                  ( f32               DeltaTime );
            void            PeriodicUpdate          ( void );

private:

//------------------------------------------------------------------------------
// Handle conversion.

            voice*          IdToVoice               ( voice_id VoiceID );
            voice_id        VoiceToId               ( voice* pVoice );

//------------------------------------------------------------------------------
// Service thread.

static      void            ServiceEntry            ( void* pData );
            void            ServiceLoop             ( void );
            xbool           StartService            ( void );
            void            StopService             ( void );
            xbool           IsServiceThread         ( void ) const;
            void            UpdateNow               ( f32 DeltaTime );
            void            PeriodicUpdateNow       ( void );

//------------------------------------------------------------------------------
// Playback implementation.

            voice_id        PlayInternal            ( const char*       pIdentifier,
                                                      xbool             AutoStart,
                                                      const vector3&    Position,
                                                      s32               Zone,
                                                      xbool             IsPositional,
                                                      xbool             bVolumeClip,
                                                      xbool             UseReservedStream = FALSE,
                                                      voice_id          ReplaceVoiceID = 0 );
            voice_id        PlayInternalNow         ( const char*       pIdentifier,
                                                      xbool             AutoStart,
                                                      const vector3&    Position,
                                                      s32               Zone,
                                                      xbool             IsPositional,
                                                      xbool             bVolumeClip,
                                                      xbool             UseReservedStream );

//------------------------------------------------------------------------------
// Stream integration.

            void            QueueStreamReadComplete ( io_request* pRequest );
            void            QueueStreamWarmComplete ( io_request* pRequest );
            xbool           ReserveStreams          ( s32 nStreams );
            xbool           UnReserveStreams        ( s32 nStreams );

//------------------------------------------------------------------------------
// Runtime access.

            audio_runtime&   Runtime                ( void );
      const audio_runtime&   Runtime                ( void ) const;

//------------------------------------------------------------------------------
// Data.

            audio_runtime*           m_pRuntime;
            x_language               m_Language;
};

extern audio_mgr g_AudioMgr;

//==============================================================================
#endif // AUDIO_MGR_HPP
//==============================================================================
