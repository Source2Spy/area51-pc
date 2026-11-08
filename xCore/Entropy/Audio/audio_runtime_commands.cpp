//==============================================================================
//
//  audio_runtime_commands.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Audio.hpp"
#include "Audio/audio_types.hpp"
#include "Audio/audio_package_format.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/audio_helpers.hpp"

//==============================================================================
//  ASYNCHRONOUS REQUESTS
//==============================================================================

struct audio_request
{
    enum
    {
        PENDING,
        COMPLETE,
        CANCELLED
    };

    x_atomic_s32 State;
    x_atomic_s32 References;
    ear_id       EarID;

    enum request_type
    {
        BREAK_POINTS,
        CREATE_EAR
    };

    request_type Type;
    xarray<f32>  Values;

    audio_request( request_type RequestType ) : EarID( 0 ), Type( RequestType )
    {
        x_AtomicInit( &State, PENDING );
        // One reference belongs to the caller and one to the command.
        x_AtomicInit( &References, 2 );
    }

    void ReleaseReference( void )
    {
        s32 Count = x_AtomicLoadAcquire( &References );

        for( ;; )
        {
            s32 Previous = x_AtomicCompareExchange( &References, Count - 1, Count );

            if( Previous == Count )
            {
                break;
            }
            Count = Previous;
        }

        if( Count == 1 )
        {
            delete this;
        }
    }
};

//==============================================================================
//  HELPERS
//==============================================================================

static
void SetCommandText( audio_command& Command, const char* pText )
{
    x_strncpy( Command.Text, pText, AUDIO_PACKAGE_FILENAME_LENGTH - 1 );
    Command.Text[AUDIO_PACKAGE_FILENAME_LENGTH - 1] = '\0';
}

//==============================================================================
//  COMMAND SUBMISSION
//==============================================================================

xbool audio_runtime::SubmitCommand( audio_command& Command, audio_command_result* pResult, xbool WaitForCompletion )
{
    if( !WaitForCompletion )
    {
        pResult = NULL;
    }

    // Calls on the owner thread execute directly, avoiding a wait on that same thread.
    if( Audio.IsServiceThread() || Service.IsNull() || !ServiceRunning )
    {
        Command.pCompletion = NULL;
        Command.pResult     = pResult;
        ExecuteCommand( Command );
        return TRUE;
    }

    if( !WaitForCompletion )
    {
        Command.pCompletion = NULL;
        Command.pResult     = NULL;
        return Commands.Push( Command );
    }

    // These stack objects remain alive until the owner signals completion.
    audio_command_result LocalResult;
    xmesgq               Completion( 1 );

    if( pResult == NULL )
    {
        pResult = &LocalResult;
    }

    Command.pCompletion = &Completion;
    Command.pResult     = pResult;

    if( !Commands.Push( Command ) )
    {
        return FALSE;
    }

    Completion.Recv( MQ_BLOCK );

    return TRUE;
}

//==============================================================================

xbool audio_runtime::SubmitCommandAsync( audio_command& Command )
{
    return SubmitCommand( Command, NULL, FALSE );
}

//==============================================================================

xbool audio_runtime::SubmitCommandSync( audio_command& Command, audio_command_result* pResult )
{
    return SubmitCommand( Command, pResult, TRUE );
}

//==============================================================================

xbool audio_runtime::SubmitSimpleCommandAsync( audio_command_type Type )
{
    audio_command Command;
    Command.Init( Type );
    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitSimpleCommandSync( audio_command_type Type, audio_command_result& Result )
{
    audio_command Command;
    Command.Init( Type );
    return SubmitCommandSync( Command, &Result );
}

//==============================================================================

xbool audio_runtime::SubmitFloatCommandAsync( audio_command_type Type, f32 Value )
{
    audio_command Command;
    Command.Init( Type );
    Command.F32A = Value;
    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitS32CommandAsync( audio_command_type Type, s32 Value )
{
    audio_command Command;
    Command.Init( Type );
    Command.S32A = Value;
    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitS32CommandSync( audio_command_type Type, s32 Value, audio_command_result& Result )
{
    audio_command Command;
    Command.Init( Type );
    Command.S32A = Value;
    return SubmitCommandSync( Command, &Result );
}

//==============================================================================

xbool audio_runtime::SubmitVoiceCommandAsync( audio_command_type Type, voice_id VoiceID )
{
    audio_command Command;
    Command.Init( Type );
    Command.VoiceID = VoiceID;
    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitVoiceCommandSync( audio_command_type Type, voice_id VoiceID, audio_command_result& Result )
{
    audio_command Command;
    Command.Init( Type );
    Command.VoiceID = VoiceID;
    return SubmitCommandSync( Command, &Result );
}

//==============================================================================

xbool audio_runtime::SubmitVoiceFloatCommandAsync( audio_command_type Type, voice_id VoiceID, f32 Value )
{
    audio_command Command;
    Command.Init( Type );
    Command.VoiceID = VoiceID;
    Command.F32A    = Value;
    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitVoiceBoolCommandAsync( audio_command_type Type, voice_id VoiceID, xbool Value )
{
    audio_command Command;
    Command.Init( Type );
    Command.VoiceID = VoiceID;
    Command.BoolA   = Value;
    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitTextCommandSync( audio_command_type Type, const char* pText, audio_command_result& Result )
{
    audio_command Command;
    Command.Init( Type );
    SetCommandText( Command, pText );
    return SubmitCommandSync( Command, &Result );
}

//==============================================================================
//  PLAYBACK REQUESTS
//==============================================================================

voice_id audio_runtime::SubmitPlayCommand( const char*                pIdentifier,
                                           xbool                      AutoStart,
                                           const vector3&             Position,
                                           s32                        ZoneID,
                                           xbool                      IsPositional,
                                           xbool                      VolumeClip,
                                           xbool                      UseReservedStream,
                                           voice_id                   ReplaceVoiceID,
                                           const audio_play_settings* pSettings )
{
    if( !pIdentifier )
    {
        return 0;
    }

    if( Audio.IsServiceThread() )
    {
        voice_id ID = Audio.PlayInternalNow( pIdentifier, AutoStart && !pSettings, Position, ZoneID,
                                             IsPositional, VolumeClip, UseReservedStream );
        if( ID && pSettings )
        {
            Audio.SetVolume( ID, pSettings->Volume );

            if( IsPositional && pSettings->FarClipScale != 0.0f )
            {
                Audio.SetFalloff( ID, 0.0f, pSettings->FarClipScale );
            }
            if( AutoStart )
            {
                Audio.Start( ID );
            }
        }

        if( ID && ReplaceVoiceID )
        {
            Audio.Release( ReplaceVoiceID, 0.0f );
        }
        return ID;
    }

    if( Service.IsNull() || !ServiceRunning )
    {
        return 0;
    }

    voice_id ID = DeferredVoices.Reserve( Voices );

    if( !ID )
    {
        return 0;
    }

    audio_command Command;
    Command.Init( AUDIO_CMD_PLAY );
    Command.VoiceID  = ID;
    Command.VoiceID2 = ReplaceVoiceID;
    Command.S32C     = UseReservedStream ? 1 : 0;
    Command.BoolA    = AutoStart && !pSettings;
    Command.S32D     = pSettings ? 1 : 0;
    Command.S32E     = AutoStart;

    if( pSettings )
    {
        Command.F32A = pSettings->Volume;
        Command.F32B = pSettings->FarClipScale;
    }

    Command.Position = Position;
    Command.S32A     = ZoneID;
    Command.BoolB    = IsPositional;
    Command.S32B     = VolumeClip ? 1 : 0;
    SetCommandText( Command, pIdentifier );

    if( !SubmitCommandAsync( Command ) )
    {
        DeferredVoices.Complete( ID, 0 );
        return 0;
    }

    return ID;
}

//==============================================================================

audio_request* audio_runtime::SubmitBreakPointsAsync( voice_id VoiceID )
{
    if( Service.IsNull() || !ServiceRunning )
    {
        return NULL;
    }

    audio_request* pRequest = new audio_request( audio_request::BREAK_POINTS );
    audio_command  Command;
    Command.Init( AUDIO_CMD_GET_BREAK_POINTS );
    Command.VoiceID  = VoiceID;
    Command.pRequest = pRequest;

    if( !SubmitCommandAsync( Command ) )
    {
        delete pRequest;
        return NULL;
    }

    return pRequest;
}

//==============================================================================

audio_request* audio_runtime::SubmitCreateEarAsync( const matrix4& W2V,
                                                    const vector3& Position,
                                                    s32            ZoneID,
                                                    f32            Volume )
{
    if( Service.IsNull() || !ServiceRunning )
    {
        return NULL;
    }

    audio_request* pRequest = new audio_request( audio_request::CREATE_EAR );
    audio_command  Command;
    Command.Init( AUDIO_CMD_CREATE_EAR );
    Command.Matrix   = W2V;
    Command.Position = Position;
    Command.S32A     = ZoneID;
    Command.F32A     = Volume;
    Command.pRequest = pRequest;

    if( !SubmitCommandAsync( Command ) )
    {
        delete pRequest;
        return NULL;
    }

    return pRequest;
}

//==============================================================================
//  REQUEST POLLING AND CANCELLATION
//==============================================================================

xbool audio_mgr::PollCreateEar( audio_request*& pRequest, ear_id& EarID )
{
    if( !pRequest || x_AtomicLoadAcquire( &pRequest->State ) != audio_request::COMPLETE )
    {
        return FALSE;
    }

    ASSERT( pRequest->Type == audio_request::CREATE_EAR );
    EarID = pRequest->EarID;
    pRequest->ReleaseReference();
    pRequest = NULL;
    return TRUE;
}

//==============================================================================

xbool audio_mgr::PollBreakPoints( audio_request*& pRequest, xarray<f32>& Values )
{
    if( !pRequest || x_AtomicLoadAcquire( &pRequest->State ) != audio_request::COMPLETE )
    {
        return FALSE;
    }

    ASSERT( pRequest->Type == audio_request::BREAK_POINTS );
    Values.SetCount( pRequest->Values.GetCount() );

    if( Values.GetCount() )
    {
        x_memcpy( Values.GetPtr(), pRequest->Values.GetPtr(), Values.GetCount() * sizeof( f32 ) );
    }
    pRequest->ReleaseReference();
    pRequest = NULL;
    return TRUE;
}

//==============================================================================

void audio_mgr::CancelRequest( audio_request*& pRequest )
{
    if( !pRequest )
    {
        return;
    }

    if( x_AtomicExchange( &pRequest->State, audio_request::CANCELLED ) == audio_request::COMPLETE )
    {
        if( pRequest->Type == audio_request::CREATE_EAR )
        {
            DestroyEar( pRequest->EarID );
        }
    }

    pRequest->ReleaseReference();
    pRequest = NULL;
}

//==============================================================================
//  POSITION AND SPATIAL COMMAND SUBMISSION
//==============================================================================

xbool audio_runtime::SubmitGetPositionCommandSync( voice_id VoiceID, vector3& Position, s32& ZoneID )
{
    audio_command        Command;
    audio_command_result Result;

    Command.Init( AUDIO_CMD_GET_POSITION );
    Command.VoiceID   = VoiceID;
    Command.pPosition = &Position;
    Command.pS32      = &ZoneID;

    return SubmitCommandSync( Command, &Result ) && Result.Bool;
}

//==============================================================================

xbool audio_runtime::SubmitSetPositionCommandAsync( voice_id VoiceID, const vector3& Position, s32 ZoneID )
{
    audio_command Command;

    Command.Init( AUDIO_CMD_SET_POSITION );
    Command.VoiceID  = VoiceID;
    Command.Position = Position;
    Command.S32A     = ZoneID;

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitVoiceFalloffCommandAsync( voice_id VoiceID, f32 Near, f32 Far )
{
    audio_command Command;

    Command.Init( AUDIO_CMD_SET_FALLOFF );
    Command.VoiceID = VoiceID;
    Command.F32A    = Near;
    Command.F32B    = Far;

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitVoiceEarCommandAsync( voice_id VoiceID, ear_id EarID )
{
    audio_command Command;

    Command.Init( AUDIO_CMD_SET_VOICE_EAR );
    Command.VoiceID = VoiceID;
    Command.EarID   = EarID;

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitClipCommandAsync( f32 NearClip, f32 FarClip )
{
    audio_command Command;

    Command.Init( AUDIO_CMD_SET_CLIP );
    Command.F32A = NearClip;
    Command.F32B = FarClip;

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitEarCommandAsync( audio_command_type Type, ear_id EarID )
{
    audio_command Command;

    Command.Init( Type );
    Command.EarID = EarID;

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitSetEarCommandAsync( ear_id         EarID,
                                               const matrix4& W2V,
                                               const vector3& Position,
                                               s32            ZoneID,
                                               f32            Volume )
{
    audio_command Command;

    Command.Init( AUDIO_CMD_SET_EAR );
    Command.EarID    = EarID;
    Command.Matrix   = W2V;
    Command.Position = Position;
    Command.S32A     = ZoneID;
    Command.F32A     = Volume;

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitEarZoneVolumeCommandAsync( ear_id EarID, s32 ZoneID, f32 Volume )
{
    audio_command Command;

    Command.Init( AUDIO_CMD_UPDATE_EAR_ZONE_VOLUME );
    Command.EarID = EarID;
    Command.S32A  = ZoneID;
    Command.F32A  = Volume;

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitEarZoneVolumesCommandAsync( ear_id EarID, f32* pVolumes )
{
    audio_command Command;

    Command.Init( AUDIO_CMD_UPDATE_EAR_ZONE_VOLUMES );
    Command.EarID = EarID;
    x_memcpy( Command.ZoneVolumes, pVolumes, sizeof( Command.ZoneVolumes ) );

    return SubmitCommandAsync( Command );
}

//==============================================================================

xbool audio_runtime::SubmitIoRequestCommandAsync( audio_command_type Type, io_request* pRequest )
{
    audio_command Command;

    Command.Init( Type );
    Command.pIoRequest = pRequest;

    return SubmitCommandAsync( Command );
}

//==============================================================================
//  COMPLETION AND DISPATCH
//==============================================================================

void audio_runtime::CompleteCommand( audio_command& Command, const audio_command_result& Result )
{
    if( Command.pRequest )
    {
        audio_request* pRequest = Command.pRequest;
        pRequest->EarID         = Result.S32;

        if( pRequest->Type == audio_request::BREAK_POINTS && Result.S32 > 0 && Result.pF32 )
        {
            pRequest->Values.SetCount( Result.S32 );
            x_memcpy( pRequest->Values.GetPtr(), Result.pF32, Result.S32 * sizeof( f32 ) );
        }

        if( x_AtomicCompareExchange( &pRequest->State, audio_request::COMPLETE,
                                     audio_request::PENDING ) != audio_request::PENDING )
        {
            if( pRequest->Type == audio_request::CREATE_EAR )
            {
                Audio.DestroyEar( Result.S32 );
            }
        }

        pRequest->ReleaseReference();
    }

    // Publish the result before waking a synchronous caller.
    if( Command.pResult )
    {
        *Command.pResult = Result;
    }

    if( Command.pCompletion )
    {
        Command.pCompletion->Send( (void*)&Audio, MQ_BLOCK );
    }
}

//==============================================================================

void audio_runtime::ExecuteCommand( audio_command& Command )
{
    audio_command_result Result;
    xbool                Handled;

    x_memset( &Result.VoiceState, 0, sizeof( Result.VoiceState ) );

    Result.Bool    = TRUE;
    Result.VoiceID = 0;
    Result.S32     = 0;
    Result.pText   = NULL;
    Result.pF32    = NULL;
    Result.F32     = 0.0f;

    // PLAY carries a deferred handle; later commands resolve it to a real voice.
    if( Command.Type != AUDIO_CMD_PLAY )
    {
        xbool Pending;
        Command.VoiceID  = DeferredVoices.Resolve( Command.VoiceID, Pending );
        Command.VoiceID2 = DeferredVoices.Resolve( Command.VoiceID2, Pending );
    }

    Handled = ExecuteServiceCommand( Command, Result );

    if( !Handled )
    {
        Handled = ExecuteStreamCommand( Command, Result );
    }
    if( !Handled )
    {
        Handled = ExecutePackageCommand( Command, Result );
    }
    if( !Handled )
    {
        Handled = ExecuteVoiceCommand( Command, Result );
    }
    if( !Handled )
    {
        Handled = ExecuteSpatialCommand( Command, Result );
    }

    if( !Handled )
    {
        ASSERT( 0 );
        Result.Bool = FALSE;
    }

    CompleteCommand( Command, Result );
}

//==============================================================================
//  SERVICE COMMANDS
//==============================================================================

xbool audio_runtime::ExecuteServiceCommand( audio_command& Command, audio_command_result& Result )
{
    switch( Command.Type )
    {
        case AUDIO_CMD_GET_DEBUG_STATS:
        {
            x_memset( &Result.DebugStats, 0, sizeof( Result.DebugStats ) );
            Streams.GetDebugStats( Result.DebugStats );
            channel* pHead = Channels.UsedList();

            for( channel* pChannel = pHead->Link.pNext; pChannel != pHead; pChannel = pChannel->Link.pNext )
            {
                Result.DebugStats.ActiveChannels++;
            }
            Result.Bool = TRUE;
            break;
        }

        case AUDIO_CMD_UPDATE:
        {
            Audio.UpdateNow( Command.F32A );
            break;
        }

        case AUDIO_CMD_PERIODIC_UPDATE:
        {
            Audio.PeriodicUpdateNow();
            break;
        }

        case AUDIO_CMD_SET_PITCH_FACTOR:
        {
            Backend.SetPitchFactor( Command.F32A );
            break;
        }

        default:
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================
//  STREAM COMMANDS
//==============================================================================

xbool audio_runtime::ExecuteStreamCommand( audio_command& Command, audio_command_result& Result )
{
    switch( Command.Type )
    {
        case AUDIO_CMD_STREAM_READ_COMPLETE:
        {
            StreamRuntime.ReadComplete( Command.pIoRequest );
            break;
        }

        case AUDIO_CMD_STREAM_WARM_COMPLETE:
        {
            StreamRuntime.WarmComplete( Command.pIoRequest );
            break;
        }

        case AUDIO_CMD_RESIZE_MEMORY:
        {
            Streams.Kill();
            Backend.ResizeMemory( Command.S32A );
            Streams.Init( *this );
            break;
        }

        case AUDIO_CMD_RESERVE_STREAMS:
        {
            Result.Bool = Streams.ReserveStreams( Command.S32A );
            break;
        }

        case AUDIO_CMD_UNRESERVE_STREAMS:
        {
            Result.Bool = Streams.UnReserveStreams( Command.S32A );
            break;
        }

        default:
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================
//  PACKAGE COMMANDS
//==============================================================================

xbool audio_runtime::ExecutePackageCommand( audio_command& Command, audio_command_result& Result )
{
    switch( Command.Type )
    {
        case AUDIO_CMD_LOAD_PACKAGE:
        {
            char LocalizedName[X_MAX_PATH];
            x_strcpy( LocalizedName, Audio.GetLocalizedName( Command.Text ) );
            Result.Bool = Packages.LoadPackage( Command.Text, LocalizedName );
            break;
        }

        case AUDIO_CMD_UNLOAD_PACKAGE:
        {
            Result.Bool = Packages.UnloadPackage( Command.Text );
            break;
        }

        case AUDIO_CMD_UNLOAD_ALL_PACKAGES:
        {
            Packages.UnloadAllPackages();
            break;
        }

        case AUDIO_CMD_SET_MASTER_VOLUME:
        {
            Packages.SetMasterVolume( Command.F32A );
            break;
        }

        case AUDIO_CMD_SET_MUSIC_VOLUME:
        {
            Packages.SetMusicVolume( Command.F32A );
            break;
        }

        case AUDIO_CMD_SET_SFX_VOLUME:
        {
            Packages.SetSFXVolume( Command.F32A );
            break;
        }

        case AUDIO_CMD_SET_VOICE_VOLUME:
        {
            Packages.SetVoiceVolume( Command.F32A );
            break;
        }

        case AUDIO_CMD_ENABLE_DUCKING:
        {
            AudioDuckLevel++;

            if( AudioDuckLevel == 1 )
            {
                Packages.ComputePackageVolumes();
            }
            break;
        }

        case AUDIO_CMD_DISABLE_DUCKING:
        {
            AudioDuckLevel--;
#if defined( rbrannon )
            ASSERTS( ( AudioDuckLevel >= 0 ), "You have called DisableAudioDucking too many times.  Match them with the calls to EnableAudioDucking" );
#endif
            if( AudioDuckLevel < 0 )
            {
                AudioDuckLevel = 0;
            }
            else if( AudioDuckLevel == 0 )
            {
                Packages.ComputePackageVolumes();
            }
            break;
        }

        case AUDIO_CMD_REMERGE_IDENTIFIER_TABLES:
        {
            Packages.ReMergeIdentifierTables();
            break;
        }

        default:
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================
//  VOICE COMMANDS
//==============================================================================

xbool audio_runtime::ExecuteVoiceCommand( audio_command& Command, audio_command_result& Result )
{
    switch( Command.Type )
    {
        case AUDIO_CMD_PLAY:
        {
            Result.VoiceID = Audio.PlayInternalNow( Command.Text,
                                                    Command.BoolA,
                                                    Command.Position,
                                                    Command.S32A,
                                                    Command.BoolB,
                                                    Command.S32B != 0,
                                                    Command.S32C != 0 );
            if( Result.VoiceID && Command.VoiceID2 )
            {
                Audio.Release( Command.VoiceID2, 0.0f );
            }
            Result.Bool = ( Result.VoiceID != 0 );

            if( Command.S32D && Result.VoiceID )
            {
                Audio.SetVolume( Result.VoiceID, Command.F32A );

                if( Command.BoolB && Command.F32B != 0.0f )
                {
                    Audio.SetFalloff( Result.VoiceID, 0.0f, Command.F32B );
                }
                if( Command.S32E )
                {
                    Audio.Start( Result.VoiceID );
                }
            }

            if( audio_deferred_voice_table::IsDeferred( Command.VoiceID ) &&
                !DeferredVoices.Complete( Command.VoiceID, Result.VoiceID ) )
            {
                Audio.Release( Result.VoiceID, 0.0f );
            }
            break;
        }

        case AUDIO_CMD_START:
        {
            voice* pVoice = Audio.IdToVoice( Command.VoiceID );
            Voices.StartVoice( pVoice );
            Result.Bool = ( pVoice != NULL );
            break;
        }

        case AUDIO_CMD_SEGUE:
        {
            Result.Bool = Voices.Segue( Audio.IdToVoice( Command.VoiceID ), Audio.IdToVoice( Command.VoiceID2 ) );
            break;
        }

        case AUDIO_CMD_SET_RELEASE_TIME:
        {
            Result.Bool = Voices.SetReleaseTime( Audio.IdToVoice( Command.VoiceID ), Command.F32A );
            break;
        }

        case AUDIO_CMD_IS_RELEASING:
        {
            Result.Bool = Voices.IsVoiceReleasing( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_VOICE_STATE:
        {
            voice* pVoice = Audio.IdToVoice( Command.VoiceID );

            if( pVoice )
            {
                Result.VoiceState.Valid    = TRUE;
                Result.VoiceState.Ready    = Voices.GetIsReady( pVoice );
                Result.VoiceState.Length   = Voices.GetVoiceTime( pVoice );
                Result.VoiceState.PlayTime = Voices.GetCurrentPlayTime( pVoice );
            }

            break;
        }

        case AUDIO_CMD_IS_VALID_VOICE_ID:
        {
            Result.Bool = ( Audio.IdToVoice( Command.VoiceID ) != NULL );
            break;
        }

        case AUDIO_CMD_IS_VOICE_READY:
        {
            Result.Bool = Voices.IsVoiceReady( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_VOICE_DESCRIPTOR:
        {
            Result.pText = Voices.GetVoiceDescriptor( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_LENGTH_SECONDS_VOICE:
        {
            Result.F32 = Voices.GetVoiceTime( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_VOLUME:
        {
            Result.F32 = Voices.GetVoiceUserVolume( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_PAN:
        {
            Result.F32 = Voices.GetVoicePan( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_PITCH:
        {
            Result.F32 = Voices.GetVoiceUserPitch( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_POSITION:
        {
            Result.Bool = Voices.GetVoicePosition( Audio.IdToVoice( Command.VoiceID ), *Command.pPosition, *Command.pS32 );
            break;
        }

        case AUDIO_CMD_GET_EFFECT_SEND:
        {
            Result.F32 = Voices.GetVoiceUserEffectSend( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_HAS_LIP_SYNC:
        {
            Result.Bool = Voices.HasLipSync( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_LIP_SYNC:
        {
            Result.F32 = Voices.GetLipSync( Audio.IdToVoice( Command.VoiceID ) ) * 2.0f;
            break;
        }

        case AUDIO_CMD_GET_BREAK_POINTS:
        {
            Result.S32 = Voices.GetBreakPoints( Audio.IdToVoice( Command.VoiceID ), Result.pF32 );
            break;
        }

        case AUDIO_CMD_GET_IS_READY:
        {
            Result.Bool = Voices.GetIsReady( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_CURRENT_PLAY_TIME:
        {
            Result.F32 = Voices.GetCurrentPlayTime( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_GET_PRIORITY:
        {
            voice* pVoice = Audio.IdToVoice( Command.VoiceID );
            Result.S32    = pVoice ? pVoice->Params.Priority : 0;
            break;
        }

        case AUDIO_CMD_PAUSE:
        {
            Voices.PauseVoice( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_PAUSE_ALL:
        {
            Voices.PauseAllVoices();
            Audio.UpdateNow( 0.0f );
            break;
        }

        case AUDIO_CMD_RESUME:
        {
            Voices.ResumeVoice( Audio.IdToVoice( Command.VoiceID ) );
            break;
        }

        case AUDIO_CMD_RESUME_ALL:
        {
            Voices.ResumeAllVoices();
            Audio.UpdateNow( 0.0f );
            break;
        }

        case AUDIO_CMD_RELEASE:
        {
            Voices.ReleaseVoice( Audio.IdToVoice( Command.VoiceID ), Command.F32A );
            break;
        }

        case AUDIO_CMD_RELEASE_ALL:
        {
            Voices.ReleaseAllVoices();
            Audio.UpdateNow( 0.0f );
            break;
        }

        case AUDIO_CMD_SET_VOLUME:
        {
            Result.Bool = Voices.SetVoiceUserVolume( Audio.IdToVoice( Command.VoiceID ), Command.F32A );
            break;
        }

        case AUDIO_CMD_SET_PAN:
        {
            Result.Bool = Voices.SetVoicePan( Audio.IdToVoice( Command.VoiceID ), Command.F32A );
            break;
        }

        case AUDIO_CMD_SET_PITCH:
        {
            Result.Bool = Voices.SetVoiceUserPitch( Audio.IdToVoice( Command.VoiceID ), Command.F32A );
            break;
        }

        case AUDIO_CMD_SET_POSITION:
        {
            Result.Bool = Voices.SetVoicePosition( Audio.IdToVoice( Command.VoiceID ), Command.Position, Command.S32A );
            break;
        }

        case AUDIO_CMD_SET_FALLOFF:
        {
            Result.Bool = Voices.SetVoiceUserFalloff( Audio.IdToVoice( Command.VoiceID ), Command.F32A, Command.F32B );
            break;
        }

        case AUDIO_CMD_SET_EFFECT_SEND:
        {
            Result.Bool = Voices.SetVoiceUserEffectSend( Audio.IdToVoice( Command.VoiceID ), Command.F32A );
            break;
        }

        case AUDIO_CMD_SET_PITCH_LOCK:
        {
            voice* pVoice = Audio.IdToVoice( Command.VoiceID );

            if( pVoice )
            {
                Voices.SetPitchLock( pVoice, Command.BoolA );
            }
            Result.Bool = ( pVoice != NULL );
            break;
        }

        default:
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================
//  SPATIAL COMMANDS
//==============================================================================

xbool audio_runtime::ExecuteSpatialCommand( audio_command& Command, audio_command_result& Result )
{
    switch( Command.Type )
    {
        case AUDIO_CMD_SET_EAR:
        {
            Spatial.SetEar( Command.EarID, Command.Matrix, Command.Position, Command.S32A, Command.F32A );
            break;
        }

        case AUDIO_CMD_CREATE_EAR:
        {
            Result.S32 = Spatial.CreateEar();

            if( Result.S32 && Command.pRequest )
            {
                Spatial.SetEar( Result.S32, Command.Matrix, Command.Position, Command.S32A, Command.F32A );
            }
            break;
        }

        case AUDIO_CMD_GET_FIRST_EAR:
        {
            Result.S32 = Spatial.GetFirstEar();
            break;
        }

        case AUDIO_CMD_GET_NEXT_EAR:
        {
            Result.S32 = Spatial.GetNextEar();
            break;
        }

        case AUDIO_CMD_DESTROY_EAR:
        {
            Spatial.DestroyEar( Command.EarID );
            break;
        }

        case AUDIO_CMD_RESET_CURRENT_EAR:
        {
            Spatial.ResetCurrentEar();
            break;
        }

        case AUDIO_CMD_SET_SPEAKER_CONFIG:
        {
            Spatial.SetSpeakerConfig( Command.S32A );
            break;
        }

        case AUDIO_CMD_SET_VOICE_EAR:
        {
            voice* pVoice = Audio.IdToVoice( Command.VoiceID );

            if( pVoice )
            {
                pVoice->EarID = Command.EarID;
            }
            Result.Bool = ( pVoice != NULL );
            break;
        }

        case AUDIO_CMD_SET_CLIP:
        {
            Spatial.SetClip( Command.F32A, Command.F32B );
            break;
        }

        case AUDIO_CMD_UPDATE_EAR_ZONE_VOLUME:
        {
            Spatial.UpdateEarZoneVolume( Command.EarID, Command.S32A, Command.F32A );
            break;
        }

        case AUDIO_CMD_UPDATE_EAR_ZONE_VOLUMES:
        {
            Spatial.UpdateEarZoneVolumes( Command.EarID, Command.ZoneVolumes );
            break;
        }

        default:
        {
            return FALSE;
        }
    }

    return TRUE;
}
