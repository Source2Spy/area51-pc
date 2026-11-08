//=========================================================================
//
//  DecalEncoder.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "DecalEncoder.hpp"
#include "a51_shader_catalog.hpp"
#include "../../ShaderContract.hpp"

#include "../../LightSystem.hpp"
#include "../../ProjectionTextureRegistry.hpp"
#include "../../Features/PostProcess/PostProcessFeature.hpp"
#include "../Resource/ProjectionAtlas.hpp"
#include "../../ShadowMapAtlas.hpp"
#include "../FrameStorage.hpp"

#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

namespace
{

//=========================================================================

xbool DecalStagingFits( s32 Count, u32 ElementSize )
{
    return ( Count > 0 ) && ( ElementSize > 0 ) &&
           ( static_cast<u64>( Count ) * static_cast<u64>( ElementSize ) <= FrameStorage::MAX_STREAM_BYTES );
}

//=========================================================================

template <typename T>
xbool ReserveDecalArray( xarray<T>& Storage, s32 RequiredCount )
{
    const u32 Maximum = FrameStorage::MAX_STREAM_BYTES / sizeof( T );

    if( RequiredCount < 0 || static_cast<u32>( RequiredCount ) > Maximum )
    {
        return FALSE;
    }

    if( Storage.GetCapacity() >= RequiredCount )
    {
        return TRUE;
    }

    u32 Capacity = MAX( static_cast<u32>( Storage.GetCapacity() ), 128u );

    while( Capacity < static_cast<u32>( RequiredCount ) )
    {
        Capacity = Capacity > Maximum / 2u ? Maximum : Capacity * 2u;
    }

    static xprofile_counter Reallocations =
        x_GetProfiler().RegisterCounter( "DecalStagingReallocations", "RenderCounter" );
    Reallocations.Add();
    Storage.SetCapacity( static_cast<s32>( MIN( Capacity, Maximum ) ) );

    return TRUE;
}

//=========================================================================

xbool CreateBlackCubeTexture( vram_texture& Texture )
{
    vram_texture_desc Desc;
    Desc.Type       = VRAM_TEXTURE_TYPE_CUBE;
    Desc.Width      = 1;
    Desc.Height     = 1;
    Desc.LayerCount = 6;
    Desc.MipCount   = 1;
    Desc.Format     = VRAM_TEXTURE_FORMAT_RGBA8;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.pDebugName = "DecalBlackCube";

    if( !vram_CreateTexture( Texture, Desc ) )
    {
        return FALSE;
    }

    const u32 Black = 0xff000000u;

    for( u32 Layer = 0; Layer < Desc.LayerCount; ++Layer )
    {
        vram_texture_upload_desc Upload;
        Upload.Region.Layer  = Layer;
        Upload.Region.Width  = 1;
        Upload.Region.Height = 1;
        Upload.pData         = &Black;
        Upload.Size          = sizeof( Black );
        Upload.RowPitch      = sizeof( Black );
        Upload.SlicePitch    = sizeof( Black );

        if( !vram_UploadTexture( Texture, Upload ) )
        {
            vram_DestroyTexture( Texture );
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool EnsureLightingBuffer( rbuffer& Buffer,
                            u32&     Capacity,
                            u32      RequiredCount,
                            u32      Stride = sizeof( GeomLightingConstants ) )
{
    const u32 MaximumCapacity = FrameStorage::MAX_STREAM_BYTES / Stride;

    if( ( MaximumCapacity == 0 ) || ( RequiredCount > MaximumCapacity ) )
    {
        return FALSE;
    }

    u32 DesiredCapacity = MAX( Capacity, 1u );

    while( DesiredCapacity < RequiredCount )
    {
        if( DesiredCapacity > ( MaximumCapacity / 2u ) )
        {
            DesiredCapacity = MaximumCapacity;
            break;
        }

        DesiredCapacity *= 2u;
    }

    DesiredCapacity = MIN( DesiredCapacity, MaximumCapacity );

    if( Buffer && ( Capacity >= DesiredCapacity ) )
    {
        return TRUE;
    }

    rbuffer_desc Desc;
    Desc.Size       = DesiredCapacity * Stride;
    Desc.Stride     = Stride;
    Desc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    Desc.pDebugName = "DecalLighting";

    if( !rbuffer_Create( Buffer, Desc ) )
    {
        return FALSE;
    }

    Capacity = DesiredCapacity;

    return TRUE;
}
} // namespace

//=========================================================================
//  TYPES
//=========================================================================

DecalEncoder::FrameConstants::FrameConstants( void )
{
    WorldToClip.Identity();
    WorldToView.Identity();
    CameraPosition.Set( 0, 0, 0, 1 );
    EnvParams.Set( 0, 0, 0, 1 );
}

//=========================================================================

DecalEncoder::ShaderBindings::ShaderBindings( void )
    : FrameConstantsVertex( 0xFFFFFFFFu ),
      FrameConstantsPixel( 0xFFFFFFFFu ),
      ProjectionConstantsPixel( 0xFFFFFFFFu ),
      DecalTexturePixel( 0xFFFFFFFFu ),
      EnvironmentCubeTexturePixel( 0xFFFFFFFFu ),
      FaceShadowTexturePixel( 0xFFFFFFFFu ),
      FaceShadowDepthTexturePixel( 0xFFFFFFFFu ),
      ProjectionAtlasTexturePixel( 0xFFFFFFFFu ),
      LightingDataPixel( 0xFFFFFFFFu ),
      ProjectionLightingDataPixel( 0xFFFFFFFFu ),
      AmbientDataPixel( 0xFFFFFFFFu ),
      ShadowMatricesPixel( 0xFFFFFFFFu ),
      ShadowDataPixel( 0xFFFFFFFFu ),
      FogConstantsPixel( 0xFFFFFFFFu ),
      FogPalettePixel( 0xFFFFFFFFu )
{
}

//=========================================================================

DecalEncoder::QueuedDraw::QueuedDraw( void )
    : Constants(),
      pTexture( NULL ),
      pEnvironmentCube( NULL ),
      Blend( render::DECAL_BLEND_ALPHA ),
      Flags( render::DECAL_DRAW_FLAG_NONE ),
      StartIndex( 0 ),
      BaseVertex( 0 ),
      IndexCount( 0 ),
      VertexCount( 0 ),
      LightingMode( LocalLightingMode::Ambient )
{
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

DecalEncoder::DecalEncoder( void )
    : m_VertexBuffer(),
      m_Pipelines(),
      m_BlackCubeTexture(),
      m_LightingBuffer(),
      m_LightingCapacity( 0 ),
      m_ProjectionConstants(),
      m_FrameLighting(),
      m_QueuedVertices(),
      m_QueuedIndices(),
      m_QueuedDraws(),
      m_QueuedLighting(),
      m_RequestedVertices( 0 ),
      m_RequestedIndices( 0 ),
      m_RequestedDraws( 0 ),
      m_areQueuedDrawsUploaded( FALSE ),
      m_isSubmissionOpen( FALSE ),
      m_isInitialized( FALSE ),
      m_pContext( NULL )
{
    x_memset( &m_ProjectionConstants, 0, sizeof( m_ProjectionConstants ) );
}

//=========================================================================

void DecalEncoder::Init( const RenderBackendContext& Context )
{
    if( m_isInitialized )
    {
        return;
    }

    if( !Context.IsComplete() )
    {
        RENDER_THROW( "DecalEncoder", "backend services were not configured" );
        return;
    }

    m_pContext = &Context;

    x_try;

        const char* pFailedStage = NULL;

        if( !m_VertexBuffer.Init( sizeof( GpuVertex ) ) )
        {
            pFailedStage = "StaticGeometryStorage";
        }
        else if( !LoadShaders() )
        {
            pFailedStage = "LoadShaders";
        }
        else if( !CreateSamplers() )
        {
            pFailedStage = "CreateSamplers";
        }
        else if( !CreateBlackCube() )
        {
            pFailedStage = "CreateBlackCube";
        }

        if( pFailedStage )
        {
            Kill();
            RENDER_THROW( "DecalEncoder", "initialization failed at stage '%s'", pFailedStage );
            return;
        }

        m_isInitialized = TRUE;

        if( !Reserve( MAX( m_RequestedVertices, 8192 ), MAX( m_RequestedIndices, 24576 ),
                      MAX( m_RequestedDraws, 2048 ) ) )
        {
            Kill();
            RENDER_THROW( "DecalEncoder", "initialization failed while preparing submission buffers or pipelines" );
        }

    x_catch_begin;

        Kill();

    x_catch_end_ret;
}

//=========================================================================

void DecalEncoder::Kill( void )
{
    DiscardQueuedDraws();
    m_FrameLighting.Clear();
    m_ProjectionLighting.Clear();
    m_AmbientLighting.Clear();
    m_QueuedVertices.Clear();
    m_QueuedIndices.Clear();
    m_QueuedDraws.Clear();
    m_QueuedLighting.Clear();

    rbuffer_Destroy( m_LightingBuffer );
    rbuffer_Destroy( m_ProjectionLightingBuffer );
    rbuffer_Destroy( m_AmbientBuffer );
    m_ProjectionLightingCapacity = m_AmbientCapacity = 0;
    m_LightingCapacity                               = 0;
    vram_DestroyTexture( m_BlackCubeTexture );
    DestroyPipelines();
    DestroySamplers();

    for( auto& Blends : m_Programs )
    {
        for( auto& Environments : Blends )
        {
            for( auto& Glows : Environments )
            {
                for( auto& Fogs : Glows )
                {
                    for( auto& Modes : Fogs )
                    {
                        for( auto& Program : Modes )
                        {
                            shader_Destroy( Program.Pixel );
                            Program = DecalProgram();
                        }
                    }
                }
            }
        }
    }

    for( auto& Lighting : m_Vertices )
    {
        for( auto& Environments : Lighting )
        {
            for( auto& Vertex : Environments )
            {
                shader_Destroy( Vertex );
            }
        }
    }

    m_pActiveProgram = NULL;
    m_VertexBuffer.Kill();

    m_RequestedVertices = 0;
    m_RequestedIndices  = 0;
    m_RequestedDraws    = 0;
    m_isInitialized     = FALSE;
}

//=========================================================================

void DecalEncoder::BeginRender( void )
{
    if( m_QueuedDraws.GetCount() || m_areQueuedDrawsUploaded )
    {
        RENDER_THROW( "DecalEncoder", "stale frame submission draws=%d uploaded=%d", m_QueuedDraws.GetCount(),
                      m_areQueuedDrawsUploaded );
    }

    ASSERTS( m_QueuedDraws.GetCount() == 0, "Decal draw queue survived the previous frame" );
    ASSERTS( !m_areQueuedDrawsUploaded, "Uploaded decal queue survived the previous frame" );
    DiscardQueuedDraws();
    m_isSubmissionOpen = TRUE;
}

//=========================================================================

xbool DecalEncoder::BeginSubmission( void )
{
    if( m_isSubmissionOpen )
    {
        return FALSE;
    }

    if( !m_isInitialized )
    {
        return FALSE;
    }

    m_isSubmissionOpen = TRUE;

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::EndSubmission( void )
{
    if( !m_isSubmissionOpen )
    {
        return FALSE;
    }

    m_isSubmissionOpen = FALSE;
    return TRUE;
}

//=========================================================================

xbool DecalEncoder::Reserve( s32 nVertices, s32 nIndices, s32 nDraws )
{
    if( !DecalStagingFits( nVertices, sizeof( GpuVertex ) ) || !DecalStagingFits( nIndices, sizeof( u16 ) ) ||
        !DecalStagingFits( nDraws, sizeof( QueuedDraw ) ) || !DecalStagingFits( nDraws, sizeof( cb_geom_lighting ) ) ||
        ( nDraws > ( 0x7fffffff - 1 ) ) || !DecalStagingFits( nDraws + 1, sizeof( GeomLightingConstants ) ) )
    {
        return FALSE;
    }

    m_RequestedVertices = MAX( m_RequestedVertices, nVertices );
    m_RequestedIndices  = MAX( m_RequestedIndices, nIndices );
    m_RequestedDraws    = MAX( m_RequestedDraws, nDraws );

    if( !ReserveDecalArray( m_QueuedVertices, nVertices ) || !ReserveDecalArray( m_QueuedIndices, nIndices ) ||
        !ReserveDecalArray( m_QueuedDraws, nDraws ) || !ReserveDecalArray( m_QueuedLighting, nDraws ) ||
        !ReserveDecalArray( m_FrameLighting, nDraws + 1 ) || !ReserveDecalArray( m_ProjectionLighting, nDraws + 1 ) ||
        !ReserveDecalArray( m_AmbientLighting, nDraws + 1 ) )
    {
        return FALSE;
    }

    if( !m_isInitialized )
    {
        return TRUE;
    }

    return m_VertexBuffer.Reserve( nVertices, nIndices ) &&
           EnsureLightingBuffer( m_LightingBuffer, m_LightingCapacity, static_cast<u32>( nDraws + 1 ) ) &&
           EnsureLightingBuffer( m_ProjectionLightingBuffer, m_ProjectionLightingCapacity, nDraws + 1,
                                 sizeof( GeomProjectionLightingConstants ) ) &&
           EnsureLightingBuffer( m_AmbientBuffer, m_AmbientCapacity, nDraws + 1, sizeof( GeomAmbientConstants ) );
}

//=========================================================================
//  SHADER RESOURCES
//=========================================================================

xbool DecalEncoder::LoadShaders( void )
{
    return TRUE;
}

//=========================================================================

DecalEncoder::DecalProgram* DecalEncoder::GetProgram( const PipelineDesc& Desc, xbool IsPrewarm )
{
    if( Desc.SourceBlend < 0 || Desc.SourceBlend >= DECAL_BLEND_COUNT )
    {
        return NULL;
    }

    const u32   Environment        = ( Desc.ShaderFlags & DECAL_DRAW_FLAG_ENV_MAPPED ) != 0;
    const u32   Glow               = ( Desc.ShaderFlags & DECAL_DRAW_FLAG_GLOW ) != 0;
    const u32   Fog                = Desc.Pass.SceneFogMode != RENDER_FOG_NONE || Desc.Pass.BloomFogMode != RENDER_FOG_NONE;
    const xbool Shadows            = Desc.Pass.EnableShadows && m_pContext->pShadowAtlas->GetShadowMode() != ShadowMode::Disabled;
    const ShaderProgramKey Key     = ResolveDecalProgram( ( ShaderBlend )( Desc.SourceBlend + 1 ), Environment, Glow, Fog, Shadows, Desc.LightingMode );
    const u32              Mode    = Key.Lighting == LightingFunctions::Ambient ? 0 : Key.Cookies ? 2 : 1;
    DecalProgram&          Program = m_Programs[Desc.SourceBlend][Environment][Glow][Fog][!!Key.Shadows][Mode];

    if( Program.Pixel )
    {
        return &Program;
    }

    const A51RecipeProgramDesc* pRecipe = A51FindRecipeProgram( Key.Hash() );

    if( !pRecipe )
    {
        RENDER_THROW( "ShaderCatalog", "decal program %016llx is absent from catalog",
                      ( unsigned long long )Key.Hash() );
        return NULL;
    }

    if( !IsPrewarm && Program.Pixel.pLoad )
    {
        RENDER_THROW( "ShaderCatalog", "program used before bytecode prewarm completed" );
        return NULL;
    }

    const u32 Lit    = Desc.SourceBlend == DECAL_BLEND_ALPHA || Desc.SourceBlend == DECAL_BLEND_SUBTRACTIVE;
    shader&   Vertex = m_Vertices[Lit][Environment][Fog];

    if( !Vertex && !( IsPrewarm ? shader_RequestComboStage( Vertex, pRecipe->PackageFile, SHADER_COMBO_KEYED,
                                                            pRecipe->Key, SHADER_STAGE_VERTEX )
                                : shader_LoadComboStage( Vertex, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                                         SHADER_STAGE_VERTEX ) ) )
    {
        return NULL;
    }

    if( !( IsPrewarm ? shader_RequestComboStage( Program.Pixel, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                                 SHADER_STAGE_PIXEL )
                     : shader_LoadComboStage( Program.Pixel, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                              SHADER_STAGE_PIXEL ) ) )
    {
        return NULL;
    }

    if( Vertex.ProgramId != pRecipe->VertexId || Program.Pixel.ProgramId != pRecipe->PixelId )
    {
        shader_Destroy( Program.Pixel );
        RENDER_THROW( "ShaderCatalog", "decal stage IDs do not match the selected recipe" );
        return NULL;
    }

    Program.pVertex = &Vertex;

    if( !ResolveBindings( Vertex, Program.Pixel, Program.Bindings ) )
    {
        shader_Destroy( Program.Pixel );
        return NULL;
    }

    return &Program;
}

//=========================================================================

xbool DecalEncoder::ResolveBindings( const shader& Vertex, const shader& Pixel, ShaderBindings& Bindings )
{
    Bindings.FrameConstantsVertex        = Vertex.ResourceSlots[A51Resource_cbDecalFrame];
    Bindings.DecalTexturePixel           = Pixel.ResourceSlots[A51Resource_txDecal];
    Bindings.FrameConstantsPixel         = Pixel.ResourceSlots[A51Resource_cbDecalFrame];
    Bindings.ProjectionConstantsPixel    = Pixel.ResourceSlots[A51Resource_cbProjTextures];
    Bindings.EnvironmentCubeTexturePixel = Pixel.ResourceSlots[A51Resource_txEnvironmentCube];
    Bindings.FaceShadowTexturePixel      = Pixel.ResourceSlots[A51Resource_txFaceShadowAtlas];
    Bindings.FaceShadowDepthTexturePixel = Pixel.ResourceSlots[A51Resource_txFaceShadowDepthAtlas];
    Bindings.ProjectionAtlasTexturePixel = Pixel.ResourceSlots[A51Resource_txProjectionAtlas];
    Bindings.LightingDataPixel           = Pixel.ResourceSlots[A51Resource_GeomLighting];
    Bindings.ProjectionLightingDataPixel = Pixel.ResourceSlots[A51Resource_GeomProjectionLighting];
    Bindings.AmbientDataPixel            = Pixel.ResourceSlots[A51Resource_GeomAmbient];
    Bindings.ShadowMatricesPixel         = Pixel.ResourceSlots[A51Resource_FaceShadowMatrices];
    Bindings.ShadowDataPixel             = Pixel.ResourceSlots[A51Resource_ShadowMapData];
    Bindings.FogConstantsPixel           = Pixel.ResourceSlots[A51Resource_cbFogConstants];
    Bindings.FogPalettePixel             = Pixel.ResourceSlots[A51Resource_FogPalette];

    if( Bindings.FrameConstantsVertex == 0xFFFFFFFFu || Bindings.DecalTexturePixel == 0xFFFFFFFFu )
    {
        RENDER_THROW( "ShaderCatalog", "decal program lacks required resources" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::CreateSamplers( void )
{
    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; ++i )
    {
        if( !rstate_CreateSampler( m_Samplers[i], static_cast<rstate_sampler_preset>( i ), "DecalSampler" ) )
        {
            DestroySamplers();
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void DecalEncoder::DestroySamplers( void )
{
    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; ++i )
    {
        rstate_DestroySampler( m_Samplers[i] );
    }
}

//=========================================================================

xbool DecalEncoder::CreateBlackCube( void )
{
    return CreateBlackCubeTexture( m_BlackCubeTexture );
}

//=========================================================================

void DecalEncoder::DestroyPipelines( void )
{
    m_Pipelines.Reset();
}

//=========================================================================

xbool DecalEncoder::QueuePipeline( decal_blend_mode Blend, u32 Flags, const PassDesc& Pass )
{
    for( u32 Mode = 0; Mode < 3; ++Mode )
    {
        PipelineDesc Desc;

        if( !BuildPipelineState( Desc, Blend, Flags, Pass, static_cast<LocalLightingMode>( Mode ) ) ||
            !GetOrCreatePipeline( Desc, TRUE ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void DecalEncoder::ReleaseLevelPipelines( void )
{
    m_Pipelines.Reset();

    for( auto& Blends : m_Programs )
    {
        for( auto& Environments : Blends )
        {
            for( auto& Glows : Environments )
            {
                for( auto& Fogs : Glows )
                {
                    for( auto& Modes : Fogs )
                    {
                        for( auto& Program : Modes )
                        {
                            shader_Destroy( Program.Pixel );
                            Program = DecalProgram();
                        }
                    }
                }
            }
        }
    }

    for( auto& Lighting : m_Vertices )
    {
        for( auto& Environments : Lighting )
        {
            for( auto& Vertex : Environments )
            {
                shader_Destroy( Vertex );
            }
        }
    }

    m_pActiveProgram = NULL;
}

//=========================================================================
//  PIPELINE STATE
//=========================================================================

xbool DecalEncoder::BuildPipelineState( PipelineDesc&     Pipeline,
                                        decal_blend_mode  Blend,
                                        u32               Flags,
                                        const PassDesc&   PassData,
                                        LocalLightingMode Lighting )
{
    Pipeline.Pass         = PassData;
    Pipeline.SourceBlend  = Blend;
    Pipeline.ShaderFlags  = Flags;
    Pipeline.LightingMode = Lighting;
    Pipeline.Depth        = ( PassData.DepthFormat == RTARGET_FORMAT_COUNT ) ? RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE
                                                                             : RSTATE_DEPTH_PRESET_NO_WRITE;
    Pipeline.Raster       = RSTATE_RASTER_PRESET_DEPTH_BIASED;
    Pipeline.pDebugName   = "ForwardDecalPipeline";

    if( !BlendPresetFromMode( Blend, Pipeline.Blend ) )
    {
        RENDER_THROW( "DecalEncoder", "invalid blend mode=%d", static_cast<s32>( Blend ) );
        return FALSE;
    }

    if( ( Flags & render::DECAL_DRAW_FLAG_GLOW ) == 0 )
    {
        switch( Pipeline.Blend )
        {
            case RSTATE_BLEND_PRESET_PREMULT_ALPHA:
            {
                Pipeline.Blend = RSTATE_BLEND_PRESET_PREMULT_ALPHA_RGB;
                break;
            }
            case RSTATE_BLEND_PRESET_PREMULT_ADD:
            {
                Pipeline.Blend = RSTATE_BLEND_PRESET_PREMULT_ADD_RGB;
                break;
            }
            case RSTATE_BLEND_PRESET_PREMULT_SUB:
            {
                Pipeline.Blend = RSTATE_BLEND_PRESET_PREMULT_SUB_RGB;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::BlendPresetFromMode( render::decal_blend_mode BlendData, rstate_blend_preset& Preset )
{
    switch( BlendData )
    {
        case render::DECAL_BLEND_ALPHA:
        {
            Preset = RSTATE_BLEND_PRESET_PREMULT_ALPHA;
            return TRUE;
        }

        case render::DECAL_BLEND_ADDITIVE:
        {
            Preset = RSTATE_BLEND_PRESET_PREMULT_ADD;
            return TRUE;
        }

        case render::DECAL_BLEND_SUBTRACTIVE:
        {
            Preset = RSTATE_BLEND_PRESET_PREMULT_SUB;
            return TRUE;
        }

        case render::DECAL_BLEND_INTENSITY:
        {
            Preset = RSTATE_BLEND_PRESET_INTENSITY;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//=========================================================================
//  LIGHTING AND SUBMISSION
//=========================================================================

xbool DecalEncoder::BuildSceneLighting( cb_geom_lighting& LightingData, const bbox& Bounds, xcolor Ambient )
{
    static xprofile_counter LightingQueries =
        x_GetProfiler().RegisterCounter( "DecalLightingQueries", "RenderCounter" );
    static xprofile_counter CollectedLights = x_GetProfiler().RegisterCounter( "DecalLights", "RenderCounter" );

    x_memset( &LightingData, 0, sizeof( LightingData ) );
    LightingData.AmbCol.Set( static_cast<f32>( Ambient.R ) / 255.0f, static_cast<f32>( Ambient.G ) / 255.0f,
                             static_cast<f32>( Ambient.B ) / 255.0f, 1.0f );

    const s32 LightCount = m_pContext->pLights->CollectLights( Bounds, MAX_GEOM_LIGHTS );
    LightingQueries.Add( 1 );
    CollectedLights.Add( LightCount );
    LightingData.LightCount = LightCount;

    for( s32 i = 0; i < LightCount; ++i )
    {
        vector3 Position;
        f32     Radius;
        xcolor  Color;
        f32     Falloff;
        s32     Shape;
        vector3 Direction;
        f32     InnerConeCos;
        f32     OuterConeCos;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;

        m_pContext->pLights->GetCollectedLightInfo( i, Position, Radius, Color, Falloff, Shape, Direction, InnerConeCos,
                                                    OuterConeCos );
        m_pContext->pLights->GetCollectedLightCookie( i, CookieIndex, CookieU, CookieV );

        LightingData.LightVec[i].Set( Position.GetX(), Position.GetY(), Position.GetZ(), Radius );
        LightingData.LightCol[i].Set( static_cast<f32>( Color.R ) / 255.0f, static_cast<f32>( Color.G ) / 255.0f,
                                      static_cast<f32>( Color.B ) / 255.0f, Falloff );
        LightingData.LightDir[i].Set( Direction.GetX(), Direction.GetY(), Direction.GetZ(),
                                      ( Shape == LightSystem::LIGHT_SHAPE_SPOT ) ? 1.0f : 0.0f );
        LightingData.LightCone[i].Set( InnerConeCos, OuterConeCos, 0.0f, 0.0f );
        LightingData.LightCookieU[i].Set( CookieU.GetX(), CookieU.GetY(), CookieU.GetZ(),
                                          ( CookieIndex >= 0 ) ? static_cast<f32>( CookieIndex + 1 ) : 0.0f );
        LightingData.LightCookieV[i].Set( CookieV.GetX(), CookieV.GetY(), CookieV.GetZ(), 0.0f );
        LightingData.DynamicLightIndex[i] = m_pContext->pLights->GetCollectedDynamicLightIndex( i );
    }

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::Submit( const render::decal_draw_desc& Desc,
                            const cubemap*                 pCubeMap,
                            const view&                    RenderView,
                            const Vertex*                  pVertices,
                            s32                            nVertices,
                            const u16*                     pIndices,
                            s32                            nIndices )
{
    if( !m_isInitialized || !m_isSubmissionOpen || m_areQueuedDrawsUploaded || !pVertices || !pIndices ||
        !Desc.pTexture || ( Desc.Blend < render::DECAL_BLEND_ALPHA ) || ( Desc.Blend >= render::DECAL_BLEND_COUNT ) ||
        ( nVertices < 3 ) || ( nIndices < 3 ) || ( ( nIndices % 3 ) != 0 ) )
    {
        RENDER_THROW( "DecalEncoder", "invalid decal submission state or geometry vertices=%d indices=%d", nVertices,
                      nIndices );
        return FALSE;
    }

    if( ( m_QueuedVertices.GetCount() > ( m_QueuedVertices.GetCapacity() - nVertices ) ) ||
        ( m_QueuedIndices.GetCount() > ( m_QueuedIndices.GetCapacity() - nIndices ) ) ||
        ( m_QueuedDraws.GetCount() >= m_QueuedDraws.GetCapacity() ) ||
        ( m_QueuedLighting.GetCount() >= m_QueuedLighting.GetCapacity() ) )
    {
        RENDER_THROW( "DecalEncoder", "reserved submission capacity exhausted" );
        return FALSE;
    }

    for( s32 i = 0; i < nIndices; ++i )
    {
        if( pIndices[i] >= nVertices )
        {
            RENDER_THROW( "DecalEncoder", "decal index %d references vertex %u/%d", i, pIndices[i], nVertices );
            return FALSE;
        }
    }

    const shader_resource* pTexture = Desc.pTexture->GetShaderResource();

    if( !pTexture || !*pTexture )
    {
        RENDER_THROW( "DecalEncoder", "decal texture has no GPU resource" );
        return FALSE;
    }

    const shader_resource* pEnvironmentCube = NULL;

    if( ( Desc.Flags & render::DECAL_DRAW_FLAG_ENV_MAPPED ) && pCubeMap )
    {
        pEnvironmentCube = pCubeMap->GetShaderResource();

        if( !pEnvironmentCube || !*pEnvironmentCube )
        {
            pEnvironmentCube = NULL;
        }
    }

    vector3 Normal = Desc.GeometricNormal;

    if( !Normal.SafeNormalize() )
    {
        RENDER_THROW( "DecalEncoder", "decal geometric normal is invalid" );
        return FALSE;
    }

    const s32 DrawIndex   = m_QueuedDraws.GetCount();
    const u64 VertexCount = static_cast<u64>( m_QueuedVertices.GetCount() ) + nVertices;
    const u64 IndexCount  = static_cast<u64>( m_QueuedIndices.GetCount() ) + nIndices;

    if( VertexCount > 0x7fffffffu || IndexCount > 0x7fffffffu || DrawIndex == 0x7fffffff ||
        !ReserveDecalArray( m_QueuedVertices, static_cast<s32>( VertexCount ) ) ||
        !ReserveDecalArray( m_QueuedIndices, static_cast<s32>( IndexCount ) ) ||
        !ReserveDecalArray( m_QueuedDraws, DrawIndex + 1 ) || !ReserveDecalArray( m_QueuedLighting, DrawIndex + 1 ) )
    {
        return FALSE;
    }

    m_QueuedDraws.SetCount( DrawIndex + 1 );
    m_QueuedLighting.SetCount( DrawIndex + 1 );

    QueuedDraw&       Draw           = m_QueuedDraws[DrawIndex];
    cb_geom_lighting& SourceLighting = m_QueuedLighting[DrawIndex];
    Draw.pTexture                    = pTexture;
    Draw.pEnvironmentCube            = pEnvironmentCube;
    Draw.Blend                       = Desc.Blend;
    Draw.Flags                       = Desc.Flags;
    Draw.StartIndex                  = m_QueuedIndices.GetCount();
    Draw.BaseVertex                  = m_QueuedVertices.GetCount();
    Draw.IndexCount                  = nIndices;
    Draw.VertexCount                 = nVertices;

    Draw.Constants.WorldToClip = RenderView.GetW2C();
    Draw.Constants.WorldToView = RenderView.GetW2V();

    const vector3 CameraPosition = RenderView.GetPosition();
    Draw.Constants.CameraPosition.Set( CameraPosition.GetX(), CameraPosition.GetY(), CameraPosition.GetZ(), 1.0f );
    Draw.Constants.EnvParams.Set( 0.0f, 0.0f, m_pContext->PhongShadingEnabled ? 1.0f : 0.0f, 1.0f );
    x_memset( &SourceLighting, 0, sizeof( SourceLighting ) );

    if( ( Desc.Blend == DECAL_BLEND_ALPHA || Desc.Blend == DECAL_BLEND_SUBTRACTIVE ) &&
        !BuildSceneLighting( SourceLighting, Desc.Bounds, Desc.Ambient ) )
    {
        m_QueuedDraws.SetCount( DrawIndex );
        m_QueuedLighting.SetCount( DrawIndex );
        return FALSE;
    }

    const s32 FirstVertex = m_QueuedVertices.GetCount();
    const s32 FirstIndex  = m_QueuedIndices.GetCount();
    m_QueuedVertices.SetCount( FirstVertex + nVertices );
    m_QueuedIndices.SetCount( FirstIndex + nIndices );

    for( s32 i = 0; i < nVertices; ++i )
    {
        GpuVertex& Vertex    = m_QueuedVertices[FirstVertex + i];
        // Legacy decals lie below thin render overlays (DREAMLND paint is ~1.03
        // units above the collision floor). Keep a fixed world-space clearance.
        const vector3p& Position = pVertices[i].Position;
        const f32 SurfaceOffset = 1.5f;
        Vertex.Position.Set( Position.X + Normal.GetX() * SurfaceOffset,
                             Position.Y + Normal.GetY() * SurfaceOffset,
                             Position.Z + Normal.GetZ() * SurfaceOffset );
        Vertex.Color         = pVertices[i].Color;
        Vertex.UV            = pVertices[i].UV;
        Vertex.Normal        = Normal;
        Vertex.LightingIndex = 0;
    }

    x_memcpy( &m_QueuedIndices[FirstIndex], pIndices, nIndices * sizeof( u16 ) );

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::RequestProjectionTextures( void ) const
{
    for( s32 i = 0; i < m_QueuedDraws.GetCount(); ++i )
    {
        const cb_geom_lighting& LightingData = m_QueuedLighting[i];
        const s32               LightCount   = MIN( LightingData.LightCount, MAX_GEOM_LIGHTS );

        for( s32 j = 0; j < LightCount; ++j )
        {
            const s32 CookieIndex = static_cast<s32>( LightingData.LightCookieU[j].GetW() ) - 1;

            if( CookieIndex < 0 )
            {
                continue;
            }

            if( CookieIndex >= m_pContext->pLights->GetLightCookieCount() )
            {
                RENDER_THROW( "DecalEncoder", "decal light cookie index %d is out of range", CookieIndex );
                return FALSE;
            }

            const texture::handle& Cookie = m_pContext->pLights->GetLightCookieHandle( CookieIndex );

            if( !Cookie.GetPointer() ||
                !m_pContext->pProjection->Request( Cookie, ProjectionAtlasEncoding::LuminanceAlpha ) )
            {
                RENDER_THROW( "DecalEncoder", "failed to request decal light cookie atlas entry" );
                return FALSE;
            }
        }
    }

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::CopyLightingData( GeomLightingConstants&           Destination,
                                      GeomProjectionLightingConstants& Projection,
                                      GeomAmbientConstants&            Ambient,
                                      const cb_geom_lighting&          Source,
                                      const FrameCompiler&             Geometry ) const
{
    x_memset( &Destination, 0, sizeof( Destination ) );
    x_memset( &Projection, 0, sizeof( Projection ) );
    x_memset( &Ambient, 0, sizeof( Ambient ) );

    for( s32 i = 0; i < MAX_GEOM_LIGHTS; ++i )
    {
        Projection.LightShadowIndex[i] = 0xFFFFFFFFu;
    }

    x_memcpy( Destination.LightVec, Source.LightVec, sizeof( Destination.LightVec ) );
    x_memcpy( Destination.LightCol, Source.LightCol, sizeof( Destination.LightCol ) );
    x_memcpy( Destination.LightDir, Source.LightDir, sizeof( Destination.LightDir ) );
    x_memcpy( Destination.LightCone, Source.LightCone, sizeof( Destination.LightCone ) );
    x_memcpy( Projection.LightCookieU, Source.LightCookieU, sizeof( Projection.LightCookieU ) );
    x_memcpy( Projection.LightCookieV, Source.LightCookieV, sizeof( Projection.LightCookieV ) );

    const s32 LightCount = MIN( Source.LightCount, MAX_GEOM_LIGHTS );

    for( s32 i = 0; i < LightCount; ++i )
    {
        const f32 Radius      = MAX( Source.LightVec[i].GetW(), 1.0e-4f );
        const f32 InnerRadius = Radius * ( 1.0f - x_clamp( Source.LightCol[i].GetW(), 0.0f, 1.0f ) );
        Destination.LightRadial[i].Set( InnerRadius, 1.0f / MAX( Radius - InnerRadius, 1.0e-4f ), 0.0f, 0.0f );

        const f32 CosOuter              = x_clamp( Source.LightCone[i].GetY(), 0.0f, 1.0f );
        Destination.LightCone[i].GetZ() = x_sqrt( MAX( 1.0f - CosOuter * CosOuter, 0.0f ) ) / MAX( CosOuter, 1.0e-4f );
        Destination.LightCone[i].GetW() =
            1.0f / MAX( Source.LightCone[i].GetX() - Source.LightCone[i].GetY(), 1.0e-4f );
        Projection.LightShadowIndex[i] = Geometry.GetDynamicLightShadowIndex( Source.DynamicLightIndex[i] );

        const s32 CookieIndex             = static_cast<s32>( Source.LightCookieU[i].GetW() ) - 1;
        Projection.LightCookieU[i].GetW() = 0.0f;
        Projection.LightCookieV[i].GetW() = 0.0f;

        if( CookieIndex < 0 )
        {
            continue;
        }

        if( CookieIndex >= m_pContext->pLights->GetLightCookieCount() )
        {
            RENDER_THROW( "DecalEncoder", "decal light cookie index %d is out of range", CookieIndex );
            return FALSE;
        }

        ProjectionAtlasRegion Region;

        if( !m_pContext->pProjection->GetRegion( m_pContext->pLights->GetLightCookieHandle( CookieIndex ),
                                                 ProjectionAtlasEncoding::LuminanceAlpha, Region ) )
        {
            RENDER_THROW( "DecalEncoder", "decal light cookie atlas region is unavailable" );
            return FALSE;
        }

        Projection.LightCookieAtlas[i]      = Region.UVScaleBias;
        Projection.LightCookieTileBounds[i] = Region.TileBounds;
        Projection.LightCookieInfo[i]       = Region.Sampling;
        Projection.LightCookieInfo[i].GetX() += 1.0f;
    }

    Ambient.LightAmbCol = Source.AmbCol;
    Ambient.LightCount  = LightCount;

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::BuildProjectionData( void )
{
    x_memset( &m_ProjectionConstants, 0, sizeof( m_ProjectionConstants ) );
    m_ProjectionConstants.ProjectionAtlasParams = m_pContext->pProjection->GetSamplingParams();

    const s32 LightCount = MIN( m_pContext->pProjections->GetProjLightCount(), ProjectionTextureRegistry::MaxLightProjectionCount );

    for( s32 i = 0; i < LightCount; ++i )
    {
        matrix4        Projection;
        const texture* pTexture = NULL;
        m_pContext->pProjections->GetProjLight( i, Projection, pTexture );

        if( !pTexture )
        {
            RENDER_THROW( "DecalEncoder", "projected light texture is unavailable index=%d", i );
            return FALSE;
        }

        ProjectionAtlasRegion Region;

        if( !m_pContext->pProjection->GetRegion( m_pContext->pProjections->GetProjLightTexture( i ), ProjectionAtlasEncoding::BlueMask, Region ) )
        {
            RENDER_THROW( "DecalEncoder", "projected light atlas region is unavailable index=%d", i );
            return FALSE;
        }

        const s32 Destination                                  = m_ProjectionConstants.ProjLightCount++;
        m_ProjectionConstants.ProjLightMatrix[Destination]     = Projection;
        m_ProjectionConstants.ProjLightAtlas[Destination]      = Region.UVScaleBias;
        m_ProjectionConstants.ProjLightTileBounds[Destination] = Region.TileBounds;
        m_ProjectionConstants.ProjLightInfo[Destination]       = Region.Sampling;
    }

    const s32 ShadowCount =
        MIN( m_pContext->pProjections->GetProjShadowCount(), ProjectionTextureRegistry::MaxShadowProjectionCount );

    for( s32 i = 0; i < ShadowCount; ++i )
    {
        matrix4        Projection;
        const texture* pTexture = NULL;
        m_pContext->pProjections->GetProjShadow( i, Projection, pTexture );

        if( !pTexture )
        {
            RENDER_THROW( "DecalEncoder", "projected shadow texture is unavailable index=%d", i );
            return FALSE;
        }

        ProjectionAtlasRegion Region;

        if( !m_pContext->pProjection->GetRegion( m_pContext->pProjections->GetProjShadowTexture( i ),
                                                 ProjectionAtlasEncoding::BlueMask, Region ) )
        {
            RENDER_THROW( "DecalEncoder", "projected shadow atlas region is unavailable index=%d", i );
            return FALSE;
        }

        const s32 Destination                                   = m_ProjectionConstants.ProjShadowCount++;
        m_ProjectionConstants.ProjShadowMatrix[Destination]     = Projection;
        m_ProjectionConstants.ProjShadowAtlas[Destination]      = Region.UVScaleBias;
        m_ProjectionConstants.ProjShadowTileBounds[Destination] = Region.TileBounds;
        m_ProjectionConstants.ProjShadowInfo[Destination]       = Region.Sampling;
    }

    return TRUE;
}

//=========================================================================
//  UPLOAD AND EXECUTION
//=========================================================================

xbool DecalEncoder::UploadQueuedDraws( const FrameCompiler& Geometry, FrameGraphBuilder& Graph )
{
    if( m_areQueuedDrawsUploaded )
    {
        RENDER_THROW( "DecalEncoder", "queued draws were uploaded more than once" );
        return FALSE;
    }

    if( m_QueuedDraws.GetCount() == 0 )
    {
        return TRUE;
    }

    if( ( m_FrameLighting.GetCapacity() < ( m_QueuedDraws.GetCount() + 1 ) ) ||
        ( m_LightingCapacity < static_cast<u32>( m_QueuedDraws.GetCount() + 1 ) ) )
    {
        RENDER_THROW( "DecalEncoder", "insufficient lighting capacity draws=%d cpu=%d gpu=%u", m_QueuedDraws.GetCount(),
                      m_FrameLighting.GetCapacity(), m_LightingCapacity );
        return FALSE;
    }

    m_FrameLighting.SetCount( 0 );
    m_ProjectionLighting.SetCount( 1 );
    m_AmbientLighting.SetCount( 1 );
    x_memset( &m_ProjectionLighting[0], 0, sizeof( GeomProjectionLightingConstants ) );
    x_memset( &m_AmbientLighting[0], 0, sizeof( GeomAmbientConstants ) );

    GeomLightingConstants& EmptyLighting = m_FrameLighting.Append();
    x_memset( &EmptyLighting, 0, sizeof( EmptyLighting ) );

    for( s32 i = 0; i < m_QueuedDraws.GetCount(); ++i )
    {
        QueuedDraw& Draw  = m_QueuedDraws[i];
        Draw.LightingMode = ResolveLocalLighting( &m_QueuedLighting[i] );

        if( Draw.Blend != DECAL_BLEND_ALPHA && Draw.Blend != DECAL_BLEND_SUBTRACTIVE )
        {
            continue;
        }

        GeomLightingConstants& LightingData = m_FrameLighting.Append();

        if( !CopyLightingData( LightingData, m_ProjectionLighting.Append(), m_AmbientLighting.Append(),
                               m_QueuedLighting[i], Geometry ) )
        {
            RENDER_THROW( "DecalEncoder", "failed to copy lighting data packet=%d ambient=%.3f,%.3f,%.3f", i,
                          m_QueuedLighting[i].AmbCol.GetX(), m_QueuedLighting[i].AmbCol.GetY(),
                          m_QueuedLighting[i].AmbCol.GetZ() );
            return FALSE;
        }

        for( s32 V = 0; V < Draw.VertexCount; ++V )
        {
            m_QueuedVertices[Draw.BaseVertex + V].LightingIndex = m_FrameLighting.GetCount() - 1;
        }
    }

    const s32 SpanCount  = m_QueuedDraws.GetCount();
    s32       BatchCount = 0;

    for( s32 i = 0; i < SpanCount; ++i )
    {
        const QueuedDraw Draw = m_QueuedDraws[i];

        if( BatchCount > 0 )
        {
            QueuedDraw& Batch        = m_QueuedDraws[BatchCount - 1];
            const s32   VertexOffset = Draw.BaseVertex - Batch.BaseVertex;

            if( Batch.pTexture == Draw.pTexture && Batch.pEnvironmentCube == Draw.pEnvironmentCube &&
                Batch.Blend == Draw.Blend && Batch.Flags == Draw.Flags && Batch.LightingMode == Draw.LightingMode &&
                x_memcmp( &Batch.Constants, &Draw.Constants, sizeof( FrameConstants ) ) == 0 &&
                Batch.StartIndex + Batch.IndexCount == Draw.StartIndex &&
                Batch.BaseVertex + Batch.VertexCount == Draw.BaseVertex && VertexOffset <= 65536 - Draw.VertexCount )
            {
                // Keep primitive order while rebasing the next span into this batch.
                for( s32 j = 0; j < Draw.IndexCount; ++j )
                {
                    m_QueuedIndices[Draw.StartIndex + j] =
                        static_cast<u16>( m_QueuedIndices[Draw.StartIndex + j] + VertexOffset );
                }

                Batch.IndexCount += Draw.IndexCount;
                Batch.VertexCount += Draw.VertexCount;
                continue;
            }
        }

        m_QueuedDraws[BatchCount++] = Draw;
    }

    m_QueuedDraws.SetCount( BatchCount );

    TransientGeometryStorage::Primitive PrimitiveData;
    PrimitiveData.pVertex   = m_QueuedVertices.GetPtr();
    PrimitiveData.pIndex    = m_QueuedIndices.GetPtr();
    PrimitiveData.nVertices = m_QueuedVertices.GetCount();
    PrimitiveData.nIndices  = m_QueuedIndices.GetCount();

    const u32 LightingBytes = static_cast<u32>( sizeof( GeomLightingConstants ) * m_FrameLighting.GetCount() );

    if( !BuildProjectionData() )
    {
        RENDER_THROW( "DecalEncoder", "failed to build projection data" );
        return FALSE;
    }

    if( !Graph.QueueUpload( m_LightingBuffer, m_FrameLighting.GetPtr(), LightingBytes, 0, TRUE ) ||
        !Graph.QueueUpload( m_ProjectionLightingBuffer, m_ProjectionLighting.GetPtr(),
                         m_ProjectionLighting.GetCount() * sizeof( GeomProjectionLightingConstants ), 0, TRUE ) ||
        !Graph.QueueUpload( m_AmbientBuffer, m_AmbientLighting.GetPtr(),
                         m_AmbientLighting.GetCount() * sizeof( GeomAmbientConstants ), 0, TRUE ) )
    {
        RENDER_THROW( "DecalEncoder", "failed to upload lighting buffer records=%d bytes=%u",
                      m_FrameLighting.GetCount(), LightingBytes );
        return FALSE;
    }

    if( !m_VertexBuffer.UploadPrimitive( PrimitiveData, Graph ) )
    {
        RENDER_THROW( "DecalEncoder", "failed to upload vertices=%d indices=%d", PrimitiveData.nVertices,
                      PrimitiveData.nIndices );
        return FALSE;
    }

    static xprofile_counter DecalDraws       = x_GetProfiler().RegisterCounter( "DecalDraws", "RenderCounter" );
    static xprofile_counter DecalSpans       = x_GetProfiler().RegisterCounter( "DecalSpans", "RenderCounter" );
    static xprofile_counter DecalUploadBytes = x_GetProfiler().RegisterCounter( "DecalUploadBytes", "RenderCounter" );
    DecalDraws.Add( m_QueuedDraws.GetCount() );
    DecalSpans.Add( SpanCount );
    DecalUploadBytes.Add( LightingBytes + m_ProjectionLighting.GetCount() * sizeof( GeomProjectionLightingConstants ) +
                          m_AmbientLighting.GetCount() * sizeof( GeomAmbientConstants ) +
                          static_cast<u32>( m_QueuedVertices.GetCount() * sizeof( GpuVertex ) ) +
                          static_cast<u32>( m_QueuedIndices.GetCount() * sizeof( u16 ) ) );

    m_areQueuedDrawsUploaded = TRUE;

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::ValidatePass( const PipelineDesc& Desc )
{
    if( ( static_cast<u32>( Desc.Pass.ColorFormat ) >= static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
        ( static_cast<u32>( Desc.Pass.DepthFormat ) > static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
        ( ( Desc.Pass.SampleCount != 1 ) && ( Desc.Pass.SampleCount != 2 ) && ( Desc.Pass.SampleCount != 4 ) &&
          ( Desc.Pass.SampleCount != 8 ) ) ||
        ( Desc.Blend < 0 ) || ( Desc.Blend >= RSTATE_BLEND_PRESET_COUNT ) || ( Desc.Depth < 0 ) ||
        ( Desc.Depth >= RSTATE_DEPTH_PRESET_COUNT ) || ( Desc.Raster < 0 ) ||
        ( Desc.Raster >= RSTATE_RASTER_PRESET_COUNT ) )
    {
        return FALSE;
    }

    if( ( Desc.Pass.DepthFormat == RTARGET_FORMAT_COUNT ) && ( Desc.Depth == RSTATE_DEPTH_PRESET_NO_WRITE ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool DecalEncoder::BuildPipelineDesc( render_pipeline_desc& Out, const PipelineDesc& Desc, xbool IsPrewarm )
{
    static xprofile_counter Descriptors =
        x_GetProfiler().RegisterCounter( "DecalPipelineDescriptors", "RenderCounter" );
    Descriptors.Add();

    if( !ValidatePass( Desc ) )
    {
        return FALSE;
    }

    static const shader_vertex_buffer_desc VertexBuffer = []()
    {
        shader_vertex_buffer_desc Result;
        Result.Slot   = 0;
        Result.Stride = sizeof( GpuVertex );

        return Result;
    }();

    static const shader_vertex_element Layout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( GpuVertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof( GpuVertex, Color ) ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( GpuVertex, UV ) ),
        shader_vertex_element( 3, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( GpuVertex, Normal ) ),
        shader_vertex_element( 4, 0, SHADER_VERTEX_FORMAT_UINT1, offsetof( GpuVertex, LightingIndex ) )
    };

    Out = render_pipeline_desc();

    DecalProgram* pProgram = GetProgram( Desc, IsPrewarm );

    if( !pProgram )
    {
        return FALSE;
    }

    Out.Shader.pVertexShader     = pProgram->pVertex;
    Out.Shader.pPixelShader      = &pProgram->Pixel;
    Out.Shader.pVertexBuffers    = &VertexBuffer;
    Out.Shader.VertexBufferCount = 1;
    Out.Shader.pInputElements    = Layout;
    Out.Shader.InputElementCount = ARRAYSIZE( Layout );
    Out.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Out.Depth                    = rstate_GetDepthDesc( Desc.Depth );
    Out.Raster                   = rstate_GetRasterDesc( Desc.Raster );
    Out.ColorCount               = Desc.Pass.WriteSceneDepth ? 2 : 1;
    Out.DepthFormat              = Desc.Pass.DepthFormat;
    Out.SampleCount              = Desc.Pass.SampleCount;
    Out.pDebugName               = Desc.pDebugName ? Desc.pDebugName : "DecalPipeline";
    Out.ColorTargets[0].Format   = Desc.Pass.ColorFormat;
    Out.ColorTargets[0].Blend    = rstate_GetBlendDesc( Desc.Blend );

    if( Desc.Pass.WriteSceneDepth )
    {
        Out.ColorTargets[1].Format               = RTARGET_FORMAT_R32F;
        Out.ColorTargets[1].Blend.ColorWriteMask = RSTATE_COLOR_WRITE_NONE;
    }

    return TRUE;
}

//=========================================================================

render_pipeline* DecalEncoder::GetOrCreatePipeline( const PipelineDesc& Desc, xbool isPrewarm )
{
    const xbool Shadows = Desc.Pass.EnableShadows && m_pContext->pShadowAtlas->GetShadowMode() != ShadowMode::Disabled;
    const ShaderProgramKey Program = ResolveDecalProgram(
        static_cast<ShaderBlend>( Desc.SourceBlend + 1 ), ( Desc.ShaderFlags & DECAL_DRAW_FLAG_ENV_MAPPED ) != 0,
        ( Desc.ShaderFlags & DECAL_DRAW_FLAG_GLOW ) != 0,
        Desc.Pass.SceneFogMode != RENDER_FOG_NONE || Desc.Pass.BloomFogMode != RENDER_FOG_NONE, Shadows,
        Desc.LightingMode );
    const u64 State = GraphicsPipelineStateKey( Desc.Pass.ColorFormat, Desc.Pass.DepthFormat, Desc.Pass.SampleCount,
                                                Desc.Blend, Desc.Depth, Desc.Raster );
    render_pipeline* pPipeline = m_Pipelines.FindPrepared( Program, State, isPrewarm );

    if( pPipeline )
    {
        return pPipeline;
    }

    render_pipeline_desc PipelineDesc;

    if( !BuildPipelineDesc( PipelineDesc, Desc, isPrewarm ) )
    {
        return NULL;
    }

    pPipeline = isPrewarm ? m_Pipelines.QueuePrewarm( PipelineDesc ) : m_Pipelines.GetOrCreate( PipelineDesc );
    m_Pipelines.RememberPrepared( Program, State, pPipeline );

    return pPipeline;
}

//=========================================================================

xbool DecalEncoder::BindResources( const QueuedDraw&        Draw,
                                   s32                      DrawIndex,
                                   const PassDesc&          PassData,
                                   const BackendPassInputs& Inputs )
{
    const shader_resource* pEnvironmentCube = Draw.pEnvironmentCube;

    if( m_pActiveProgram->Bindings.EnvironmentCubeTexturePixel != 0xFFFFFFFFu && !pEnvironmentCube )
    {
        pEnvironmentCube = vram_GetShaderResource( m_BlackCubeTexture );
    }

    const shader_resource* pProjectionAtlas = m_pContext->pProjection->GetShaderResource();
    const shader_resource* pLighting        = rbuffer_GetResource( m_LightingBuffer );
    const shader_resource* pShadowMatrices  = m_pContext->pFrame->GetShadowMatricesResource();
    const shader_resource* pShadowData      = m_pContext->pFrame->GetShadowDataResource();
    const shader_resource* pFaceShadow      = Inputs.pShadowSample ? rtarget_GetShaderResource( *Inputs.pShadowSample )
                                                                   : m_pContext->pFrame->GetFaceShadowResource();
    const shader_resource* pFaceShadowDepth = Inputs.pShadowDepth ? rtarget_GetShaderResource( *Inputs.pShadowDepth )
                                                                  : m_pContext->pFrame->GetFaceShadowDepthResource();
    const rstate_sampler_preset FaceShadowSampler = m_pContext->pFrame->GetFaceShadowSampler();

    if( !Draw.pTexture )
    {
        RENDER_THROW( "DecalEncoder", "missing decal texture packet=%d", DrawIndex );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.EnvironmentCubeTexturePixel != 0xFFFFFFFFu && !pEnvironmentCube )
    {
        RENDER_THROW( "DecalEncoder", "missing environment cube packet=%d", DrawIndex );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.ProjectionAtlasTexturePixel != 0xFFFFFFFFu && !pProjectionAtlas )
    {
        RENDER_THROW( "DecalEncoder", "missing projection atlas packet=%d", DrawIndex );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.LightingDataPixel != 0xFFFFFFFFu && !pLighting )
    {
        RENDER_THROW( "DecalEncoder", "missing lighting buffer packet=%d", DrawIndex );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.ShadowMatricesPixel != 0xFFFFFFFFu && !pShadowMatrices )
    {
        RENDER_THROW( "DecalEncoder", "missing shadow matrices packet=%d", DrawIndex );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.ShadowDataPixel != 0xFFFFFFFFu && !pShadowData )
    {
        RENDER_THROW( "DecalEncoder", "missing shadow data packet=%d", DrawIndex );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.FaceShadowTexturePixel != 0xFFFFFFFFu && !pFaceShadow )
    {
        RENDER_THROW( "DecalEncoder", "missing face shadow atlas packet=%d", DrawIndex );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.FaceShadowDepthTexturePixel != 0xFFFFFFFFu && !pFaceShadowDepth )
    {
        RENDER_THROW( "DecalEncoder", "missing face shadow depth atlas packet=%d", DrawIndex );
        return FALSE;
    }

    if( ( FaceShadowSampler < 0 ) || ( FaceShadowSampler >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        RENDER_THROW( "DecalEncoder", "invalid face shadow sampler packet=%d sampler=%d", DrawIndex,
                      static_cast<s32>( FaceShadowSampler ) );
        return FALSE;
    }

    auto PushConstants = [&]( const FrameConstants& ConstantsData ) -> xbool
    {
        if( !shader_PushUniformData( SHADER_STAGE_VERTEX, m_pActiveProgram->Bindings.FrameConstantsVertex,
                                     &ConstantsData, sizeof( ConstantsData ) ) )
        {
            RENDER_THROW( "DecalEncoder", "failed to push vertex constants packet=%d pass=%s", DrawIndex, "forward" );
            return FALSE;
        }

        if( m_pActiveProgram->Bindings.FrameConstantsPixel != 0xFFFFFFFFu &&
            !shader_PushUniformData( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.FrameConstantsPixel, &ConstantsData,
                                     sizeof( ConstantsData ) ) )
        {
            RENDER_THROW( "DecalEncoder", "failed to push pixel constants packet=%d pass=%s", DrawIndex, "forward" );
            return FALSE;
        }

        return TRUE;
    };

    if( !PushConstants( Draw.Constants ) )
    {
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.ProjectionConstantsPixel != 0xFFFFFFFFu &&
        !shader_PushUniformData( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.ProjectionConstantsPixel,
                                 &m_ProjectionConstants, sizeof( m_ProjectionConstants ) ) )
    {
        RENDER_THROW( "DecalEncoder", "failed to push projection constants packet=%d pass=%s", DrawIndex, "forward" );
        return FALSE;
    }

    if( m_pActiveProgram->Bindings.FogConstantsPixel != 0xFFFFFFFFu )
    {
        const PostProcessFeature* pPost = m_pContext->pPostProcess;

        if( !pPost )
        {
            return FALSE;
        }

        RenderFogConstants Fog;
        pPost->GetFogConstants( Fog );
        Fog.SceneMode = PassData.SceneFogMode;
        Fog.BloomMode = PassData.BloomFogMode;

        if( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.FogConstantsPixel, &Fog,
                                     sizeof( Fog ) ) )
        {
            return FALSE;
        }

        if( m_pActiveProgram->Bindings.FogPalettePixel != 0xFFFFFFFFu &&
            !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.FogPalettePixel,
                                                         pPost->GetFogPaletteResource(),
                                                         pPost->GetFogPaletteSampler() ) ) )
        {
            return FALSE;
        }
    }

    const shader_sampler_binding Samplers[] = {
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.DecalTexturePixel, Draw.pTexture,
                                &m_Samplers[RSTATE_SAMPLER_PRESET_MATERIAL_CLAMP] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.EnvironmentCubeTexturePixel,
                                pEnvironmentCube, &m_Samplers[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.FaceShadowTexturePixel, pFaceShadow,
                                &m_Samplers[FaceShadowSampler] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.FaceShadowDepthTexturePixel,
                                pFaceShadowDepth, &m_Samplers[RSTATE_SAMPLER_PRESET_POINT_CLAMP] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.ProjectionAtlasTexturePixel,
                                pProjectionAtlas, &m_Samplers[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] )
    };
    const char* const SamplerNames[] = { "decal", "environment", "face-shadow", "face-shadow-depth",
                                         "projection-atlas" };

    for( u32 i = 0; i < ARRAYSIZE( Samplers ); ++i )
    {
        if( Samplers[i].Slot != 0xFFFFFFFFu && !shader_BindSampler( Samplers[i] ) )
        {
            RENDER_THROW( "DecalEncoder", "failed to bind %s sampler packet=%d slot=%u pass=%s", SamplerNames[i],
                          DrawIndex, Samplers[i].Slot, "forward" );
            return FALSE;
        }
    }

    const shader_storage_buffer_binding Storage[] = {
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.LightingDataPixel, pLighting ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.ProjectionLightingDataPixel,
                                       rbuffer_GetResource( m_ProjectionLightingBuffer ) ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.AmbientDataPixel,
                                       rbuffer_GetResource( m_AmbientBuffer ) ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.ShadowMatricesPixel,
                                       pShadowMatrices ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Bindings.ShadowDataPixel, pShadowData )
    };
    const char* const StorageNames[] = { "lighting", "projection-lighting", "ambient", "shadow-matrices",
                                         "shadow-data" };

    for( u32 i = 0; i < ARRAYSIZE( Storage ); ++i )
    {
        if( Storage[i].Slot != 0xFFFFFFFFu && !shader_BindStorageBuffer( Storage[i] ) )
        {
            RENDER_THROW( "DecalEncoder", "failed to bind %s buffer packet=%d slot=%u pass=%s", StorageNames[i],
                          DrawIndex, Storage[i].Slot, "forward" );
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void DecalEncoder::DeclareQueuedResources( FrameGraphBuilder& Graph, RenderPassId Pass, s32 DrawIndex ) const
{
    const QueuedDraw& Draw = m_QueuedDraws[DrawIndex];
    m_VertexBuffer.DeclareResources( Graph, Pass );
    Graph.Read( Pass, m_LightingBuffer, RENDER_RESOURCE_ACCESS_STORAGE_READ );
    Graph.Read( Pass, m_ProjectionLightingBuffer, RENDER_RESOURCE_ACCESS_STORAGE_READ );
    Graph.Read( Pass, m_AmbientBuffer, RENDER_RESOURCE_ACCESS_STORAGE_READ );
    Graph.Read( Pass, Draw.pTexture );
    Graph.Read( Pass, Draw.pEnvironmentCube );
    Graph.Read( Pass, vram_GetShaderResource( m_BlackCubeTexture ) );
}

//=========================================================================

xbool DecalEncoder::PrepareQueuedDraw( s32 DrawIndex, const PassDesc& Pass )
{
    if( DrawIndex < 0 || DrawIndex >= m_QueuedDraws.GetCount() )
    {
        return FALSE;
    }

    QueuedDraw&  Draw = m_QueuedDraws[DrawIndex];
    PipelineDesc Desc;

    if( !BuildPipelineState( Desc, Draw.Blend, Draw.Flags, Pass, Draw.LightingMode ) )
    {
        return FALSE;
    }

    Draw.pPipeline = GetOrCreatePipeline( Desc );
    Draw.pProgram  = GetProgram( Desc );

    return Draw.pPipeline && Draw.pProgram;
}

//=========================================================================

xbool DecalEncoder::DrawQueuedDraw( s32 DrawIndex, const PassDesc& PassData, const BackendPassInputs& Inputs )
{
    if( !m_areQueuedDrawsUploaded || ( DrawIndex < 0 ) || ( DrawIndex >= m_QueuedDraws.GetCount() ) )
    {
        RENDER_THROW( "DecalEncoder", "invalid queued draw packet=%d count=%d uploaded=%d", DrawIndex,
                      m_QueuedDraws.GetCount(), m_areQueuedDrawsUploaded );
        return FALSE;
    }

    const QueuedDraw& Draw = m_QueuedDraws[DrawIndex];

    if( !m_VertexBuffer.BindBuffers() )
    {
        RENDER_THROW( "DecalEncoder", "failed to bind vertex/index buffers packet=%d", DrawIndex );
        return FALSE;
    }

    m_pActiveProgram = Draw.pProgram;

    if( !Draw.pPipeline || !m_pActiveProgram || !render_BindPipeline( *Draw.pPipeline ) )
    {
        RENDER_THROW( "DecalEncoder", "failed to bind pipeline packet=%d color=%d depth=%d samples=%u blend=%d",
                      DrawIndex, static_cast<s32>( PassData.ColorFormat ), static_cast<s32>( PassData.DepthFormat ),
                      PassData.SampleCount, static_cast<s32>( Draw.Blend ) );
        return FALSE;
    }

    if( !BindResources( Draw, DrawIndex, PassData, Inputs ) )
    {
        return FALSE;
    }

    if( !m_VertexBuffer.DrawIndexed( Draw.IndexCount, Draw.StartIndex, Draw.BaseVertex ) )
    {
        RENDER_THROW( "DecalEncoder", "indexed draw failed packet=%d indices=%d start=%d base=%d", DrawIndex,
                      Draw.IndexCount, Draw.StartIndex, Draw.BaseVertex );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void DecalEncoder::DiscardQueuedDraws( void )
{
    m_FrameLighting.SetCount( 0 );
    m_ProjectionLighting.SetCount( 0 );
    m_AmbientLighting.SetCount( 0 );
    m_QueuedVertices.SetCount( 0 );
    m_QueuedIndices.SetCount( 0 );
    m_QueuedDraws.SetCount( 0 );
    m_QueuedLighting.SetCount( 0 );
    m_areQueuedDrawsUploaded = FALSE;
    m_isSubmissionOpen       = FALSE;
    x_memset( &m_ProjectionConstants, 0, sizeof( m_ProjectionConstants ) );
}

//=========================================================================

s32 DecalEncoder::GetQueuedDrawCount( void ) const
{
    return m_QueuedDraws.GetCount();
}

//=========================================================================

render::decal_blend_mode DecalEncoder::GetQueuedDrawBlend( s32 DrawIndex ) const
{
    ASSERT( ( DrawIndex >= 0 ) && ( DrawIndex < m_QueuedDraws.GetCount() ) );
    return m_QueuedDraws[DrawIndex].Blend;
}

//=========================================================================

xbool DecalEncoder::IsQueuedDrawGlow( s32 DrawIndex ) const
{
    ASSERT( ( DrawIndex >= 0 ) && ( DrawIndex < m_QueuedDraws.GetCount() ) );
    return ( m_QueuedDraws[DrawIndex].Flags & render::DECAL_DRAW_FLAG_GLOW ) != 0;
}

//=========================================================================

} // namespace render
