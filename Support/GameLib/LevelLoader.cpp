//=============================================================================
//
//  LevelLoader.cpp
//
//=============================================================================
//  INCLUDES
//=============================================================================

#include "Entropy.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Render.hpp"
#include "Objects/Player/Player.hpp"
#include "Objects/Actor/Actor.hpp"
#include "Objects/Corpse.hpp"
#include "Objects/LevelSettings.hpp"
#include "Objects/PlaySurface.hpp"
#include "Objects/Render/PostEffectMgr.hpp"
#include "GameLib/Level.hpp"
#include "GameLib/BinLevel.hpp"
#include "GameLib/Link.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "IOManager/io_mgr.hpp"
#include "Audio/audio_stream_mgr.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "x_files/x_profile.hpp"
#include "Auxiliary/Bitmap/aux_Bitmap.hpp"
#include "../Support/TriggerEx/TriggerEx_Manager.hpp"
#include "../Support/Tracers/TracerMgr.hpp"
#include "../Support/Render/Pipeline/RenderPipeline.hpp"
#include "../Support/Render/RigidColor.hpp"
#include "Navigation/Nav_Map.hpp"
#include "Navigation/ng_connection2.hpp"
#include "Navigation/ng_node2.hpp"
#include "ZoneMgr/ZoneMgr.hpp"
#include "PlaySurfaceMgr/PlaySurfaceMgr.hpp"
#include "GameLib/StatsMgr.hpp"
#include "Objects/ParticleEmiter.hpp"
#include "Menu/DebugMenu2.hpp"
#include "Music_mgr/music_mgr.hpp"
#include "MusicStateMgr/MusicStateMgr.hpp"
#include "ConversationMgr/ConversationMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "GameTextMgr/GameTextMgr.hpp"
#include "Objects/SpawnPoint.hpp"
#include "CollisionMgr/PolyCache.hpp"
#include "TemplateMgr/TemplateMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Decals/DecalMgr.hpp"
#include "GameLib/RenderContext.hpp"
#include "DataVault/DataVault.hpp"
#include "TweakMgr/TweakMgr.hpp"
#include "PainMgr/PainMgr.hpp"
#include "PhysicsMgr/PhysicsMgr.hpp"
#include "Objects/AlienGlob.hpp"
#include "NetworkMgr/Messages/MsgMgr.hpp"
#include "Debris/debris_mgr.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"
#include "LevelLoader.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "StateMgr/StateMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "OccluderMgr/OccluderMgr.hpp"
#include "Dialogs/dlg_LoadGame.hpp"
#include "Render/PipelinePrewarm.hpp"
#include "Render/RigidGeom.hpp"
#include "Render/SkinGeom.hpp"
#include "Decals/DecalPackage.hpp"
#include "FX/fx_Mgr.hpp"

#include "StateMgr/MapList.hpp"

//=============================================================================

//  STORAGE
//=============================================================================

level_loader g_LevelLoader;
xbool        g_level_loading = FALSE;
extern u32   g_nLogicFramesAfterLoad;

#ifndef X_RETAIL
void* g_pBallast = NULL;
#endif

static const f32 LEVEL_LOAD_OBJECT_TIME_BUDGET = 0.004f;

//=============================================================================
//  PIPELINE REQUIREMENTS
//=============================================================================

namespace
{
struct level_pipeline_requirements
{
    render::PipelinePrewarmContext Context;
    xbool                          Types[object::TYPE_END_OF_LIST];

    level_pipeline_requirements( void )
    {
        x_memset( Types, 0, sizeof( Types ) );
    }
};

//=============================================================================

xbool CollectResourcePipelines( const char*, const char* pExtension, const void* pData, void* pContext )
{
    render::PipelinePrewarmContext& Context = static_cast<level_pipeline_requirements*>( pContext )->Context;

    if( x_stricmp( pExtension, ".rigidgeom" ) == 0 )
        return Context.QueueGeometry( *static_cast<const rigid_geom*>( pData ), FALSE );

    if( x_stricmp( pExtension, ".skingeom" ) == 0 )
        return Context.QueueGeometry( *static_cast<const skin_geom*>( pData ), TRUE );

    if( x_stricmp( pExtension, ".decalpkg" ) == 0 )
    {
        const decal_package& Package = *static_cast<const decal_package*>( pData );

        for( s32 i = 0; i < Package.GetNDecalDefs(); ++i )
        {
            const decal_definition& Def   = Package.GetDecalDef( i );
            u32 const               Flags =
                ( ( Def.m_Flags & decal_definition::DECAL_FLAG_ADD_GLOW ) ? render::DECAL_DRAW_FLAG_GLOW : 0 ) |
                ( ( Def.m_Flags & decal_definition::DECAL_FLAG_ENV_MAPPED ) ? render::DECAL_DRAW_FLAG_ENV_MAPPED : 0 );

            if( !Context.QueueDecal( static_cast<render::decal_blend_mode>( Def.m_BlendMode ), Flags ) )
                return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

xbool CollectObjectPipelines( const char* pTypeName, void* pContext )
{
    const object_desc* pDesc = g_ObjMgr.GetDescFromName( pTypeName );

    if( !pDesc )
        return FALSE;

    level_pipeline_requirements& Requirements = *static_cast<level_pipeline_requirements*>( pContext );

    if( Requirements.Types[pDesc->GetType()] )
        return TRUE;

    // Visit each object type once, even when it appears in several sources.
    Requirements.Types[pDesc->GetType()] = TRUE;

    return pDesc->CollectRenderPipelineRequirements( Requirements.Context );
}
}

//=============================================================================

static
xbool CollectLevelPipelines( level_pipeline_requirements& Requirements )
{
    // Templates may describe objects that have not been instantiated yet.
    if( !g_TemplateMgr.VisitObjectTypes( CollectObjectPipelines, &Requirements ) )
        return FALSE;

    for( s32 i = 0; i < object::TYPE_END_OF_LIST; ++i )
    {
        object::type const Type = static_cast<object::type>( i );

        if( g_ObjMgr.GetFirst( Type ) != SLOT_NULL &&
            !CollectObjectPipelines( g_ObjMgr.GetTypeDesc( Type )->GetTypeName(), &Requirements ) )
            return FALSE;
    }

    // Include loaded resources and effects after the object requirements.
    return g_RscMgr.VisitLoadedResources( CollectResourcePipelines, &Requirements ) &&
           FXMgr.QueueLoadedPipelines( Requirements.Context );
}

//=============================================================================

xbool level_loader::BeginPipelinePrewarm( void )
{
    const render::RenderPipeline& Pipeline = render::GetRenderPipeline();

    // Remember the render settings used to prepare this level's pipelines.
    m_PipelineConfiguration =
        Pipeline.GetSampleCount() |
        ( static_cast<u32>( Pipeline.GetShadowMapAtlas().GetShadowMode() ) << 8 ) |
        ( static_cast<u32>( Pipeline.GetShadowMapAtlas().GetShadowFilterType() ) << 16 );

    render::GraphicsPipelineCache::BeginPrewarm();

    level_pipeline_requirements Requirements;

    if( !render::GetRenderPipeline().QueueCommonPipelines( render::RenderScopeDesc() ) ||
        !CollectLevelPipelines( Requirements ) || !Requirements.Context.QueueBuiltins() )
        return FALSE;

    // PIP captures use their own single-sample configuration without post effects.
    if( Requirements.Types[object::TYPE_PIP] )
    {
        render::RenderScopeDesc Capture;
        Capture.Purpose              = render::RENDER_VIEW_SCENE_CAPTURE;
        Capture.SampleCount          = 1;
        Capture.EnableShadows        = FALSE;
        Capture.EnablePostProcessing = FALSE;

        Requirements.Context = render::PipelinePrewarmContext( Capture );
        x_memset( Requirements.Types, 0, sizeof( Requirements.Types ) );

        if( !CollectLevelPipelines( Requirements ) || !Requirements.Context.QueueBuiltins() )
            return FALSE;
    }

    return UpdatePipelinePrewarm();
}

//=============================================================================

xbool level_loader::UpdatePipelinePrewarm( void )
{
    return render::GraphicsPipelineCache::UpdatePrewarm();
}

//=============================================================================

xbool level_loader::IsPipelinePrewarmComplete( void ) const
{
    return render::GraphicsPipelineCache::IsPrewarmComplete();
}

//=============================================================================

xbool level_loader::NeedsPipelinePrewarm( void ) const
{
    const render::RenderPipeline& Pipeline = render::GetRenderPipeline();

    u32 const Configuration =
        Pipeline.GetSampleCount() |
        ( static_cast<u32>( Pipeline.GetShadowMapAtlas().GetShadowMode() ) << 8 ) |
        ( static_cast<u32>( Pipeline.GetShadowMapAtlas().GetShadowFilterType() ) << 16 );

    return Configuration != m_PipelineConfiguration;
}

//=============================================================================

static
void ReleaseLevelPipelines( void )
{
    render::RenderPipeline& Pipeline = render::GetRenderPipeline();
    Pipeline.ReleaseCommonPipelines();
    Pipeline.GetBackend().GetFrameCompiler().ReleaseLevelPipelines();
    Pipeline.GetPrimitiveStorage().ReleaseLevelPipelines();
    Pipeline.GetDecalEncoder().ReleaseLevelPipelines();
    Pipeline.GetShadowFeatureRef().Backend().ReleaseLevelPipelines();
    render_TrimPipelineCache();
}

//=============================================================================

level_loader::level_loader( void )
{
    m_LevelLoadStage         = LEVEL_LOAD_IDLE;
    m_PipelineConfiguration  = 0;
    m_pMapEntry              = NULL;
    m_pLoadScript            = NULL;
    m_LoadScriptCommand      = 0;
    m_LoadScriptCommandCount = 0;
    m_ScriptDFSFileSystem    = -1;
    m_ScriptDFSFile          = 0;
    m_ScriptDFSFileCount     = 0;
    m_NextPlayerSlot         = SLOT_NULL;
    m_VoiceID                = 0;
    m_bFullLoad              = FALSE;
    m_SlideshowPrepared      = FALSE;
    m_LevelFileSystemMounted = FALSE;
}

//=============================================================================

level_loader::~level_loader( void )
{
    ASSERT( m_pLoadScript == NULL );
}

//=============================================================================

void level_loader::LoadInfo( const char* pPath )
{
    if( pPath )
    {
        text_in InfoTextIn;
        InfoTextIn.OpenFile( pPath );

        while( InfoTextIn.ReadHeader() )
        {
            if( x_stricmp( InfoTextIn.GetHeaderName(), "Info" ) == 0 )
            {
                bbox WorldBBox;
                WorldBBox.Clear();
                InfoTextIn.ReadFields();
                InfoTextIn.GetBBox( "WorldBBox", WorldBBox );
                g_ObjMgr.SetSafeBBox( WorldBBox );
            }

            if( x_stricmp( InfoTextIn.GetHeaderName(), "PlayerInfo" ) == 0 )
            {
                vector3 Position;
                radian  Pitch;
                radian  Yaw;
                s32     Zone;
                guid    Guid;
                InfoTextIn.ReadFields();
                InfoTextIn.GetVector3( "Position", Position );
                InfoTextIn.GetF32( "Pitch", Pitch );
                InfoTextIn.GetF32( "Yaw", Yaw );
                InfoTextIn.GetS32( "Zone", Zone );
                InfoTextIn.GetGuid( "PlayerGuid", Guid );

                pGameLogic->SetPlayerSpawnInfo( Position, Pitch, Yaw, Zone, Guid );
            }
        }

        InfoTextIn.CloseFile();
    }
}

//=============================================================================

void level_loader::LoadDFS( const char* pDFS )
{
    s32 iFileSystem = g_IOFSMgr.GetFileSystemIndex( pDFS );
    s32 nFiles      = g_IOFSMgr.GetNFilesInFileSystem( iFileSystem );

    for( s32 i = 0; i < nFiles; i++ )
    {
        LoadDFSResource( iFileSystem, i );
    }
}

//=============================================================================

void level_loader::LoadResourceList( const char* pFileName )
{
    text_in TextIn;

    if( !TextIn.OpenFile( pFileName ) )
        return;

    if( TextIn.ReadHeader() )
    {
        s32 nResources = TextIn.GetHeaderCount();

        for( s32 i = 0; i < nResources; i++ )
        {
            if( !TextIn.ReadFields() )
                break;

            char ResourceName[128];
            TextIn.GetString( "name", ResourceName );
            g_RscMgr.Load( ResourceName );
        }
    }

    TextIn.CloseFile();
}

//=============================================================================

void level_loader::LoadDFSResource( s32 FileSystem, s32 FileIndex )
{
    static const char* s_SupportedExtensions[] =
        {
            ".xbmp",
            ".rigidgeom",
            ".skingeom",
            ".anim",
            ".decalpkg",
            ".envmap",
            ".rigidcolor",
            ".string",
            ".fxo",
            ".audiopkg",
            ".font" };

    char FilePath[256];
    char Extension[32];

    g_IOFSMgr.GetFileNameInFileSystem( FileSystem, FileIndex, FilePath );
    x_splitpath( FilePath, NULL, NULL, NULL, Extension );

    for( s32 i = 0; i < (s32)( sizeof( s_SupportedExtensions ) / sizeof( s_SupportedExtensions[0] ) ); i++ )
    {
        if( x_stricmp( Extension, s_SupportedExtensions[i] ) == 0 )
        {
            // The mounted package can satisfy dependencies missing during preload.
            g_RscMgr.Load( FilePath );
            return;
        }
    }
}

//=============================================================================

void RedirectTextureAllocator( void )
{
}

//=============================================================================

void RestoreTextureAllocator( void )
{
}

//=============================================================================

void level_loader::PrepareSlideshow( const char* pSlideShowScriptFile )
{
    ASSERT( !m_SlideshowPrepared );

    // if we need to do a slide show for the campaign mode, handle that now
    if( g_StateMgr.GetState() == SM_SINGLE_PLAYER_LOAD_MISSION )
    {
        // grab a pointer to the load screen...we'll need that in a moment
        dlg_load_game* pLoadScreen = (dlg_load_game*)g_UiMgr->GetTopmostDialog( g_UiUserID );
        ASSERT( pLoadScreen );
        pLoadScreen->StartLoadingProcess();

        // load up the script and let the loading screen know about the
        // details of it
        text_in TextIn;
        TextIn.OpenFile( pSlideShowScriptFile );

        // load in the audio descriptor
        char AudioDescriptor[256];
        TextIn.ReadHeader();
        TextIn.ReadFields();
        TextIn.GetString( "descriptor", AudioDescriptor );

        // load in the text slide times
        f32 StartTextAnim;
        TextIn.ReadHeader();
        TextIn.ReadFields();
        TextIn.GetF32( "start_text_anim", StartTextAnim );
        pLoadScreen->SetTextAnimInfo( StartTextAnim );

        // load in the slide information
        TextIn.ReadHeader();
        s32 nSlides = TextIn.GetHeaderCount();

        RedirectTextureAllocator();
        {
            s32 i;
            pLoadScreen->SetNSlides( nSlides );

            for( i = 0; i < nSlides; i++ )
            {
                char   TextureName[256];
                f32    StartFadeIn;
                f32    EndFadeIn;
                f32    StartFadeOut;
                f32    EndFadeOut;
                xcolor SlideColor;

                TextIn.ReadFields();
                TextIn.GetString( "texture", TextureName );
                TextIn.GetF32( "start_fade_in", StartFadeIn );
                TextIn.GetF32( "end_fade_in", EndFadeIn );
                TextIn.GetF32( "start_fade_out", StartFadeOut );
                TextIn.GetF32( "end_fade_out", EndFadeOut );
                TextIn.GetColor( "slide_color", SlideColor );

                pLoadScreen->SetSlideInfo( i, TextureName, StartFadeIn, EndFadeIn, StartFadeOut, EndFadeOut, SlideColor );
            }
        }

        RestoreTextureAllocator();

        // make sure the loading audio package is loaded
        rhandle_base Handle;
        Handle.SetName( "DX_Loading.audiopkg" );
        Handle.GetPointer();
        m_VoiceID = 0;

        // prepare the voice
        g_AudioMgr.ReleaseAll();
        m_VoiceID = g_AudioMgr.Play( AudioDescriptor, FALSE );

        global_settings& Settings = g_StateMgr.GetActiveSettings();
        g_AudioMgr.SetVoiceVolume( Settings.GetVolume( VOLUME_SPEECH ) / 100.0f );

        pLoadScreen->SetVoiceID( m_VoiceID );
        m_SlideshowPrepared = TRUE;
    }
}

//=============================================================================

void level_loader::StartSlideshow( void )
{
    if( !m_SlideshowPrepared )
    {
        return;
    }

    dlg_load_game* pLoadScreen = (dlg_load_game*)g_UiMgr->GetTopmostDialog( g_UiUserID );
    ASSERT( pLoadScreen );

    if( g_AudioMgr.IsValidVoiceIdCached( m_VoiceID ) )
    {
        g_AudioMgr.Start( m_VoiceID );
    }

    pLoadScreen->StartSlideshow();
    m_SlideshowPrepared = FALSE;
}

//=============================================================================

void level_loader::BeginLevelLoad( xbool bFullLoad )
{
    ASSERT( ( m_LevelLoadStage == LEVEL_LOAD_IDLE ) ||
            ( m_LevelLoadStage == LEVEL_LOAD_COMPLETE ) ||
            ( m_LevelLoadStage == LEVEL_LOAD_FAILED ) );
    ASSERT( m_pLoadScript == NULL );

    if( bFullLoad )
    {
        player::s_bPlayerDied = FALSE;
    }

    m_LevelLoadStage  = LEVEL_LOAD_FIND_MAP;
    m_pMapEntry       = NULL;
    m_LevelFileSystem = xfs( "levels/%s/level", g_ActiveConfig.GetLevelPath() );
#if defined( TARGET_POSIX )
    // Fix uppercase asset directory issue in Linus stuff.
    char LinuxLevelFileSystem[X_MAX_PATH];
    x_strcpy( LinuxLevelFileSystem, m_LevelFileSystem );
    x_strtoupper( LinuxLevelFileSystem );
    m_LevelFileSystem = LinuxLevelFileSystem;
#endif
    m_ScriptDFS              = "";
    m_LoadScriptCommand      = 0;
    m_LoadScriptCommandCount = 0;
    m_ScriptDFSFileSystem    = -1;
    m_ScriptDFSFile          = 0;
    m_ScriptDFSFileCount     = 0;
    m_NextPlayerSlot         = SLOT_NULL;
    m_VoiceID                = 0;
    m_bFullLoad              = bFullLoad;
    m_SlideshowPrepared      = FALSE;
    m_LevelFileSystemMounted = FALSE;

    g_PerceptionMgr.Init();
    g_level_loading = TRUE;
}

//=============================================================================

xbool level_loader::IsLevelLoadComplete( void ) const
{
    return ( m_LevelLoadStage == LEVEL_LOAD_COMPLETE ) ||
           ( m_LevelLoadStage == LEVEL_LOAD_FAILED );
}

//=============================================================================

void level_loader::CloseLoadScript( void )
{
    if( m_pLoadScript )
    {
        m_pLoadScript->CloseFile();
        delete m_pLoadScript;
        m_pLoadScript = NULL;
    }
}

//=============================================================================

void level_loader::CloseLevelLoad( void )
{
    CloseLoadScript();

    if( m_ScriptDFS.GetLength() > 0 )
    {
        g_IOFSMgr.UnmountFileSystem( m_ScriptDFS );
        m_ScriptDFS = "";
    }

    if( m_LevelFileSystemMounted )
    {
        g_IOFSMgr.UnmountFileSystem( m_LevelFileSystem );
        m_LevelFileSystemMounted = FALSE;
    }

    if( g_AudioMgr.IsValidVoiceIdCached( m_VoiceID ) )
    {
        g_AudioMgr.Release( m_VoiceID, 0.0f );
    }

    m_SlideshowPrepared = FALSE;
    g_level_loading     = FALSE;
    m_LevelLoadStage    = LEVEL_LOAD_IDLE;
}

//=============================================================================

void level_loader::FailLevelLoad( void )
{
    ReleaseLevelPipelines();
    CloseLevelLoad();

    g_ActiveConfig.SetExitReason(
        ( g_StateMgr.GetState() == SM_SINGLE_PLAYER_LOAD_MISSION )
            ? GAME_EXIT_INVALID_CAMPAIGN_MISSION
            : GAME_EXIT_INVALID_MISSION );
    m_LevelLoadStage = LEVEL_LOAD_FAILED;
}

//=============================================================================

void level_loader::BeginScriptDFSLoad( const char* pDFS )
{
    ASSERT( m_ScriptDFS.GetLength() == 0 );

    if( !g_IOFSMgr.MountFileSystem( pDFS, 3 ) )
    {
        // load_dfs is a preload hint. Some PC data sets use resources from an
        // already mounted default filesystem and do not contain the original
        // platform-specific preload DFS (for example STRINGS\STRINGS).
        LOG_MESSAGE( "LoadLevel",
                     "Optional preload filesystem '%s' is unavailable; continuing.",
                     pDFS );
        m_LevelLoadStage = LEVEL_LOAD_EXECUTE_SCRIPT;
        return;
    }

    m_ScriptDFSFileSystem = g_IOFSMgr.GetFileSystemIndex( pDFS );

    if( m_ScriptDFSFileSystem < 0 )
    {
        g_IOFSMgr.UnmountFileSystem( pDFS );
        LOG_WARNING( "LoadLevel",
                     "Mounted preload filesystem '%s' could not be enumerated; continuing.",
                     pDFS );
        m_LevelLoadStage = LEVEL_LOAD_EXECUTE_SCRIPT;
        return;
    }

    m_ScriptDFS          = pDFS;
    m_ScriptDFSFile      = 0;
    m_ScriptDFSFileCount = g_IOFSMgr.GetNFilesInFileSystem( m_ScriptDFSFileSystem );
    m_LevelLoadStage     = LEVEL_LOAD_SCRIPT_DFS;
}

//=============================================================================

void level_loader::UpdateScriptDFSLoad( void )
{
    ASSERT( m_ScriptDFS.GetLength() > 0 );

    if( m_ScriptDFSFile < m_ScriptDFSFileCount )
    {
        LoadDFSResource( m_ScriptDFSFileSystem, m_ScriptDFSFile );
        m_ScriptDFSFile++;
        return;
    }

    g_IOFSMgr.UnmountFileSystem( m_ScriptDFS );
    m_ScriptDFS           = "";
    m_ScriptDFSFileSystem = -1;
    m_LevelLoadStage      = LEVEL_LOAD_EXECUTE_SCRIPT;
}

//=============================================================================

void level_loader::UpdateLevelLoad( f32 TimeBudgetSeconds )
{
    MEMORY_OWNER( "LOADLEVEL" );
    ASSERT( TimeBudgetSeconds >= 0.0f );

    char   Path[256];
    char   Path2[256];
    xtimer TimeBudget;
    TimeBudget.Start();

    do
    {
        switch( m_LevelLoadStage )
        {
            case LEVEL_LOAD_IDLE:
            case LEVEL_LOAD_COMPLETE:
            case LEVEL_LOAD_FAILED:
                return;

            case LEVEL_LOAD_FIND_MAP:
                m_pMapEntry = g_MapList.Find( g_ActiveConfig.GetLevelID(),
                                              g_ActiveConfig.GetGameTypeID() );
                if( !m_pMapEntry )
                {
                    FailLevelLoad();
                    return;
                }

                m_LevelLoadStage = LEVEL_LOAD_MOUNT_FILESYSTEM;
                break;

            case LEVEL_LOAD_MOUNT_FILESYSTEM:
                ASSERT( m_pMapEntry );

                GameMgr.SetZoneMinimum( m_pMapEntry->GetMinPlayers() );

#ifndef X_RETAIL
                if( m_bFullLoad )
                {
                    extern s32 GetMemoryBallastForLevel( const char* pLevelName );
                    g_pBallast = x_malloc( GetMemoryBallastForLevel( g_ActiveConfig.GetLevelPath() ) );
                }
#endif

                LOG_MESSAGE( "LoadLevel",
                             "BEGIN! Level:%s, Memory Free:%d bytes",
                             m_pMapEntry->GetDisplayName(),
                             x_MemGetFree() );

                if( !g_IOFSMgr.MountFileSystem( m_LevelFileSystem, 2 ) )
                {
                    LOG_WARNING( "LoadLevel",
                                 "Unable to mount level filesystem '%s'.",
                                 (const char*)m_LevelFileSystem );
                    FailLevelLoad();
                    return;
                }

                m_LevelFileSystemMounted = TRUE;
                m_LevelLoadStage         = m_bFullLoad ? LEVEL_LOAD_INITIALIZE_RENDER
                                                       : LEVEL_LOAD_CREATE_OBJECTS;
                break;

            case LEVEL_LOAD_INITIALIZE_RENDER:
                anim_event::Init();
                PrepareSlideshow( "SlideShowScript.txt" );
                StartSlideshow();
                m_LevelLoadStage = LEVEL_LOAD_OPEN_SCRIPT;
                break;

            case LEVEL_LOAD_OPEN_SCRIPT:
                ASSERT( m_pLoadScript == NULL );
                m_pLoadScript = new text_in;

                if( !m_pLoadScript->OpenFile( "LoadScript.txt" ) ||
                    !m_pLoadScript->ReadHeader() )
                {
                    FailLevelLoad();
                    return;
                }

                m_LoadScriptCommand      = 0;
                m_LoadScriptCommandCount = m_pLoadScript->GetHeaderCount();
                m_LevelLoadStage         = LEVEL_LOAD_EXECUTE_SCRIPT;
                break;

            case LEVEL_LOAD_EXECUTE_SCRIPT:
                if( m_LoadScriptCommand >= m_LoadScriptCommandCount )
                {
                    CloseLoadScript();
                    m_LevelLoadStage = LEVEL_LOAD_INITIALIZE_DATA;
                    break;
                }
                else
                {
                    char Command[256];
                    char Arguments[256];
                    char FileSystem[256];

                    if( !m_pLoadScript->ReadFields() )
                    {
                        FailLevelLoad();
                        return;
                    }

                    m_pLoadScript->GetString( "command", Command );
                    m_pLoadScript->GetString( "arguments", Arguments );
                    m_LoadScriptCommand++;

                    x_strcpy( FileSystem, Arguments );

                    if( x_strncmp( m_LevelFileSystem, "HDD:", 4 ) == 0 )
                    {
                        x_sprintf( FileSystem, "HDD:%s", Arguments );
                    }

                    if( x_strcmp( Command, "load_dfs" ) == 0 )
                    {
                        BeginScriptDFSLoad( FileSystem );
                    }
                    else if( x_strcmp( Command, "load_resource" ) == 0 )
                    {
                        g_RscMgr.Load( Arguments );
                    }
                    else if( x_strcmp( Command, "mount_dfs" ) == 0 )
                    {
                        if( !g_IOFSMgr.MountFileSystem( FileSystem, 3 ) )
                        {
                            FailLevelLoad();
                            return;
                        }
                    }
                    else if( x_strcmp( Command, "unmount_dfs" ) == 0 )
                    {
                        g_IOFSMgr.UnmountFileSystem( FileSystem );
                    }
                    else
                    {
                        x_DebugMsg( "Unknown level load command '%s'.\n", Command );
                    }
                }

                break;

            case LEVEL_LOAD_SCRIPT_DFS:
                UpdateScriptDFSLoad();
                break;

            case LEVEL_LOAD_INITIALIZE_DATA:
                g_DataVault.Init();
                m_LevelLoadStage = LEVEL_LOAD_LOAD_TWEAKS;
                break;

            case LEVEL_LOAD_LOAD_TWEAKS:
            {
                if( !LoadTweaks( "" ) )
                {
                    LOG_ERROR( "LoadLevel", "Unable to load tweak data." );
                    FailLevelLoad();
                    return;
                }

                m_LevelLoadStage = LEVEL_LOAD_LOAD_PAIN;
                break;
            }

            case LEVEL_LOAD_LOAD_PAIN:
            {
                if( !LoadPain( "" ) )
                {
                    LOG_ERROR( "LoadLevel", "Unable to load pain data." );
                    FailLevelLoad();
                    return;
                }

                m_LevelLoadStage = LEVEL_LOAD_CREATE_OBJECTS;
                break;
            }

            case LEVEL_LOAD_CREATE_OBJECTS:
                g_ObjMgr.CreateObject( "god" );

                if( !m_bFullLoad )
                {
                    g_PlaySurfaceMgr.CreateProxyPlaySurfaceObject();
                }

                m_LevelLoadStage = LEVEL_LOAD_NAV_MAP;
                break;

            case LEVEL_LOAD_NAV_MAP:
                x_makepath( Path, NULL, "", "level_data", ".nmp" );
                g_NavMap.Load( Path );
                m_LevelLoadStage = LEVEL_LOAD_GLOBALS;
                break;

            case LEVEL_LOAD_GLOBALS:
                x_makepath( Path, NULL, "", "level_data", ".glb" );
                {
                    MEMORY_OWNER( "GLOBAL VARIABLE DATA" );
                    g_VarMgr.LoadGlobals( Path );
                }

                m_LevelLoadStage = m_bFullLoad ? LEVEL_LOAD_PRELOAD_RIGID_COLORS
                                               : LEVEL_LOAD_BEGIN_BINARY_LEVEL;
                break;

            case LEVEL_LOAD_PRELOAD_RIGID_COLORS:
                x_makepath( Path, NULL, "", "level_data", ".rigidcolor" );
                {
                    rhandle_base Handle;
                    Handle.SetName( Path );
                    Handle.GetPointer();
                }

                m_LevelLoadStage = LEVEL_LOAD_INFO;
                break;

            case LEVEL_LOAD_INFO:
                x_makepath( Path, NULL, "", "level_data", ".info" );
                LoadInfo( Path );
                m_LevelLoadStage = LEVEL_LOAD_BEGIN_BINARY_LEVEL;
                break;

            case LEVEL_LOAD_BEGIN_BINARY_LEVEL:
                x_makepath( Path, NULL, "", "level_data", ".bin_level" );
                x_makepath( Path2, NULL, "", "level_data", ".lev_dict" );

                if( !g_BinLevelMgr.BeginLevelLoad( Path, Path2 ) )
                {
                    FailLevelLoad();
                    return;
                }

                m_LevelLoadStage = LEVEL_LOAD_BINARY_LEVEL;
                break;

            case LEVEL_LOAD_BINARY_LEVEL:
                if( !g_BinLevelMgr.IsLevelLoadComplete() )
                {
                    g_BinLevelMgr.UpdateLevelLoad( LEVEL_LOAD_OBJECT_TIME_BUDGET );
                }

                if( !g_BinLevelMgr.IsLevelLoadComplete() )
                    break;

                if( m_bFullLoad )
                {
                    m_LevelLoadStage = LEVEL_LOAD_RIGID_COLORS;
                }
                else
                {
                    m_NextPlayerSlot = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
                    m_LevelLoadStage = LEVEL_LOAD_PLAYER_ZONES;
                }

                break;

            case LEVEL_LOAD_RIGID_COLORS:
                x_makepath( Path, NULL, "", "level_data", ".rigidcolor" );
                {
                    rhandle<RigidColorData> RigidColors;
                    RigidColors.SetName( Path );
                    RigidColors.GetPointer();
                }

                m_LevelLoadStage = LEVEL_LOAD_TEMPLATES;
                break;

            case LEVEL_LOAD_TEMPLATES:
            {
                MEMORY_OWNER( "TEMPLATE DATA" );
                x_makepath( Path, NULL, "", "level_data", ".templates" );
                x_makepath( Path2, NULL, "", "level_data", ".tmpl_dct" );
                g_TemplateMgr.LoadData( Path, Path2 );
            }

                m_LevelLoadStage = LEVEL_LOAD_ZONES;
                break;

            case LEVEL_LOAD_ZONES:
            {
                MEMORY_OWNER( "ZONE DATA" );
                x_makepath( Path, NULL, "", "level_data", ".zone" );
                g_ZoneMgr.Load( Path );
            }

                m_LevelLoadStage = LEVEL_LOAD_BEGIN_PLAY_SURFACES;
                break;

            case LEVEL_LOAD_BEGIN_PLAY_SURFACES:
            {
                MEMORY_OWNER( "PLAYSURFACE DATA" );
                x_makepath( Path, NULL, "", "level_data", ".playsurface" );
                g_PlaySurfaceMgr.OpenFile( Path, TRUE );
                g_PlaySurfaceMgr.BeginLoadAllZones();
            }

                m_LevelLoadStage = LEVEL_LOAD_PLAY_SURFACES;
                break;

            case LEVEL_LOAD_PLAY_SURFACES:
            {
                MEMORY_OWNER( "PLAYSURFACE DATA" );

                if( !g_PlaySurfaceMgr.UpdateLoadAllZones() )
                    break;

                g_PlaySurfaceMgr.CloseFile();
            }

                m_LevelLoadStage = LEVEL_LOAD_DECALS;
                break;

            case LEVEL_LOAD_DECALS:
            {
                MEMORY_OWNER( "STATIC DECAL DATA" );
                x_makepath( Path, NULL, "", "level_data", ".decals" );
                g_DecalMgr.LoadStaticDecals( Path );
            }

                m_NextPlayerSlot = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
                m_LevelLoadStage = LEVEL_LOAD_PLAYER_ZONES;
                break;

            case LEVEL_LOAD_PLAYER_ZONES:
                if( m_NextPlayerSlot == SLOT_NULL )
                {
                    // Zone data is loaded after the binary level objects. Players
                    // used to be the only objects that needed an explicit rebase,
                    // but actors also have a zone tracker now.
                    for( s32 Type = 0; Type < object::TYPE_END_OF_LIST; Type++ )
                    {
                        slot_id Slot = g_ObjMgr.GetFirst( (object::type)Type );

                        while( Slot != SLOT_NULL )
                        {
                            object* pObject = g_ObjMgr.GetObjectBySlot( Slot );
                            Slot            = g_ObjMgr.GetNext( Slot );

                            if( pObject &&
                                ( pObject->GetType() != object::TYPE_PLAYER ) &&
                                pObject->IsKindOf( actor::GetRTTI() ) )
                            {
                                static_cast<actor*>( pObject )->InitZoneTracking();
                            }
                        }
                    }

                    m_LevelLoadStage = LEVEL_LOAD_FINALIZE_CORE;
                }
                else
                {
                    const slot_id PlayerSlot = m_NextPlayerSlot;
                    m_NextPlayerSlot         = g_ObjMgr.GetNext( PlayerSlot );

                    object_ptr<player> PlayerObject( g_ObjMgr.GetObjectBySlot( PlayerSlot ) );

                    if( PlayerObject.IsValid() )
                    {
                        PlayerObject.m_pObject->InitZoneTracking();
                    }
                }

                break;

            case LEVEL_LOAD_FINALIZE_CORE:
                g_PolyCache.InvalidateAllCells();
                x_makepath( Path, NULL, "", "level_data", ".rigidcolor" );
                g_BinLevelMgr.SetRigidColor( Path );
                g_PostEffectMgr.StartScreenFade( xcolor( 0, 0, 0, 0 ), 0.0f );
                g_AudioMgr.SetMasterVolume( 1.0f );
                m_LevelLoadStage = LEVEL_LOAD_FINALIZE_RUNTIME;
                break;

            case LEVEL_LOAD_FINALIZE_RUNTIME:
                MsgMgr.Init();
                g_MusicMgr.Init();
                g_DecalMgr.ResetDynamicDecals();
                g_ObjMgr.InflateSafeBBox( 1000.0f );
                g_nLogicFramesAfterLoad = 0;
                m_LevelLoadStage        = LEVEL_LOAD_FINALIZE_OCCLUDERS;
                break;

            case LEVEL_LOAD_FINALIZE_OCCLUDERS:
                g_OccluderMgr.Init();
                g_OccluderMgr.GatherOccluders();
                m_LevelLoadStage = LEVEL_LOAD_BEGIN_PREWARM;
                break;

            case LEVEL_LOAD_BEGIN_PREWARM:
                if( !BeginPipelinePrewarm() )
                {
                    FailLevelLoad();
                    break;
                }

                m_LevelLoadStage = LEVEL_LOAD_PREWARM;
                return;

            case LEVEL_LOAD_PREWARM:
                if( !UpdatePipelinePrewarm() )
                {
                    FailLevelLoad();
                    break;
                }

                if( !IsPipelinePrewarmComplete() )
                    return;

                m_LevelLoadStage = LEVEL_LOAD_UNMOUNT_FILESYSTEM;
                break;

            case LEVEL_LOAD_UNMOUNT_FILESYSTEM:
                ASSERT( m_LevelFileSystemMounted );
                g_IOFSMgr.UnmountFileSystem( m_LevelFileSystem );
                m_LevelFileSystemMounted = FALSE;
                eng_SavePipelineCache();
                g_level_loading  = FALSE;
                m_LevelLoadStage = LEVEL_LOAD_COMPLETE;
                break;
        }
    } while( !IsLevelLoadComplete() && ( TimeBudget.ReadSec() < TimeBudgetSeconds ) );
}

//=============================================================================

void level_loader::LoadLevelFinish( void )
{
    // kill the slideshow audio
    if( m_bFullLoad && g_AudioMgr.IsValidVoiceIdCached( m_VoiceID ) )
    {
        g_AudioMgr.Release( m_VoiceID, 0.0f );
    }

    // Initialize ragdolls now that all geometry and collision is loaded.

    slot_id SlotID = g_ObjMgr.GetFirst( object::TYPE_CORPSE );

    while( SlotID != SLOT_NULL )
    {
        // Lookup corpse
        object* pObj    = g_ObjMgr.GetObjectBySlot( SlotID );
        corpse* pCorpse = &corpse::GetSafeType( *pObj );

        // Initialize
        pCorpse->InitializeEditorPlaced();

        // Get next corpse
        SlotID = g_ObjMgr.GetNext( SlotID );
    }

    // initialize audio volumes from global settings
    global_settings& Settings = g_StateMgr.GetActiveSettings();
    Settings.CommitAudio();

    LOG_MESSAGE( "LoadLevel", "Done!" );
}

//=============================================================================

void level_loader::UnloadLevel( xbool bFullUnload )
{
    CloseLevelLoad();

    g_GameTextMgr.Reset();
    g_MusicStateMgr.Init();
    g_MusicMgr.Kill();

    // The show must go on ....................................................

    LOG_MESSAGE( "level_loader::UnloadLevel", "Unload level started." );

    // Reset the perception mgr.
    g_PerceptionMgr.Init();

    xtimer t;

    t.Start();

    if( bFullUnload )
    {
        UnloadTweaks();
        UnloadPain();

        g_DataVault.Kill();
    }
    else
    {
#ifndef X_EDITOR
        // clear all the network object pointers
        NetObjMgr.Clear();
#endif
        g_DecalMgr.ResetDynamicDecals();
        g_AudioMgr.ReleaseAll();
        g_VarMgr.ClearData(); // Clear the global variables.
    }

    // This should shutdown all systems that are inited in LoadLevel
    g_ObjMgr.Clear();
    FXMgr.EndOfFrame(); // Call FXMgr EndOfFrame to flush any deferred deletes - must be after ObjMgr.Clear
    FXMgr.EndOfFrame();
    FXMgr.EndOfFrame();
    FXMgr.EndOfFrame();

    if( bFullUnload )
    {
        g_DecalMgr.UnloadStaticDecals();
        ReleaseLevelPipelines();
        g_DecalMgr.ResetDynamicDecals();
        g_PlaySurfaceMgr.Reset();
#ifdef RSC_MGR_COLLECT_STATS
        g_RscMgr.DumpStats();
#endif // RSC_MGR_COLLECT_STATS
        g_RscMgr.UnloadAll( TRUE );
#ifdef RSC_MGR_COLLECT_STATS
        g_RscMgr.DumpStats();
#endif // RSC_MGR_COLLECT_STATS
        g_AudioMgr.DisplayPackages();
        g_StringMgr.Reset();
        g_TemplateStringMgr.Reset();
        g_ZoneMgr.Reset();
        anim_group::CompactEventData();

        MsgMgr.Reset();
        g_BinLevelMgr.ClearData( TRUE );
        g_TemplateMgr.ClearData();
        g_VarMgr.ClearData();
        debris_mgr::ClearData();
        g_SpatialDBase.Clear();

        const char* LevelDFS = g_ActiveConfig.GetLevelPath();
        g_IOFSMgr.UnmountFileSystem( LevelDFS );

        // Notify checkpoint manager that the next checkpoint should be at a level start.
        g_CheckPointMgr.SetCheckPointIndex( 0 );
    }

    g_PolyCache.InvalidateAllCells();

    if( bFullUnload )
    {
        extern xstring g_SkinPropSurfaceStringList;
        extern xstring g_AlienSpawnTubeStringList;
        extern xstring g_AnimSurfaceStringList;
        extern xstring g_ReactiveSurfaceStringList;
        extern xstring g_SuperDestructibleStringList;
        extern xstring g_SuperDestructiblePlayAnimStringList;

        g_SkinPropSurfaceStringList.Clear();
        g_AlienSpawnTubeStringList.Clear();
        g_AnimSurfaceStringList.Clear();
        g_ReactiveSurfaceStringList.Clear();
        g_SuperDestructibleStringList.Clear();
        g_SuperDestructiblePlayAnimStringList.Clear();

        g_SkinPropSurfaceStringList.FreeExtra();
        g_AlienSpawnTubeStringList.FreeExtra();
        g_AnimSurfaceStringList.FreeExtra();
        g_ReactiveSurfaceStringList.FreeExtra();
        g_SuperDestructibleStringList.FreeExtra();
        g_SuperDestructiblePlayAnimStringList.FreeExtra();

        // This is last because it does an x_free followed by x_malloc
        g_NavMap.Reset();

#ifndef X_RETAIL
        if( g_pBallast )
        {
            x_free( g_pBallast );
            g_pBallast = NULL;
        }
#endif
    }
    else
    {
        g_NavMap.Reset();
    }

    // Clear OccluderMgr, even on partial unload
    g_OccluderMgr.Kill();

    // Move the identifier tables down low in memory.
    g_AudioMgr.ReMergeIdentifierTables();

    if( GameMgr.IsGameOnline() )
    {
        if( g_NetworkMgr.IsServer() )
            GameMgr.UpdatePlayerStats();

        g_StateMgr.SaveMatchStats( GameMgr.GetPlayerStats() );
        GameMgr.ResetPlayerStats();
    }

    eng_SavePipelineCache();

    LOG_MESSAGE( "level_loader::UnloadLevel", "Unload complete. Memory Free:%d bytes, took %2.02fms", x_MemGetFree(), t.ReadMs() );
    x_DebugMsg( "Unload level tool %2.02fms", t.ReadMs() );
}

//=============================================================================
// Please make sure that you add the unmount call if you add another filesystem
// to the list. This will break the IOP reboot if it is not performed.

void level_loader::MountDefaultFilesystems( void )
{
    g_IOFSMgr.MountFileSystem( "PRELOAD", 1 );
    g_IOFSMgr.MountFileSystem( "UI", 1 );
    g_IOFSMgr.MountFileSystem( "AUDIO\\MUSIC", 10 );
    g_IOFSMgr.MountFileSystem( "AUDIO\\VOICE", 11 );
    g_IOFSMgr.MountFileSystem( "AUDIO\\HOT", 12 );
    g_IOFSMgr.MountFileSystem( "AUDIO\\AMBIENT", 12 );
    g_IOFSMgr.MountFileSystem( "STRINGS", 13 );
    g_IOFSMgr.MountFileSystem( "MOVIES", 15 );
    g_IOFSMgr.MountFileSystem( "SHADERS", 16 );
    g_IOFSMgr.MountFileSystem( "FONTS", 17 );
    g_IOFSMgr.MountFileSystem( "TEXTURES", 18 );
    g_IOFSMgr.MountFileSystem( "ANIMS", 19 );
    g_IOFSMgr.MountFileSystem( "MODELS", 20 );
    g_IOFSMgr.MountFileSystem( "EFFECTS", 21 );
    g_IOFSMgr.MountFileSystem( "DECALS", 22 );
}

//=============================================================================

void level_loader::UnmountDefaultFilesystems( void )
{
    g_IOFSMgr.UnmountFileSystem( "DECALS" );
    g_IOFSMgr.UnmountFileSystem( "EFFECTS" );
    g_IOFSMgr.UnmountFileSystem( "MODELS" );
    g_IOFSMgr.UnmountFileSystem( "ANIMS" );
    g_IOFSMgr.UnmountFileSystem( "TEXTURES" );
    g_IOFSMgr.UnmountFileSystem( "FONTS" );
    g_IOFSMgr.UnmountFileSystem( "SHADERS" );
    g_IOFSMgr.UnmountFileSystem( "MOVIES" );
    g_IOFSMgr.UnmountFileSystem( "STRINGS" );
    g_IOFSMgr.UnmountFileSystem( "AUDIO\\AMBIENT" );
    g_IOFSMgr.UnmountFileSystem( "AUDIO\\HOT" );
    g_IOFSMgr.UnmountFileSystem( "AUDIO\\VOICE" );
    g_IOFSMgr.UnmountFileSystem( "AUDIO\\MUSIC" );
    g_IOFSMgr.UnmountFileSystem( "UI" );
    g_IOFSMgr.UnmountFileSystem( "PRELOAD" );
}
