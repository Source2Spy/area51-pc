//=========================================================================
//
//  PrimitiveStorage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PrimitiveStorage.hpp"
#include "a51_shader_catalog.hpp"
#include "../../ShaderContract.hpp"
#include "../../Features/PostProcess/PostProcessFeature.hpp"
#include "../FrameStorage.hpp"

#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static xbool PrimitiveStagingFits( u64 Count, u32 ElementSize )
{
    return ( Count > 0 ) && ( Count <= 0x7fffffffu ) && ( ElementSize > 0 ) &&
           ( Count * static_cast<u64>( ElementSize ) <= FrameStorage::MAX_STREAM_BYTES );
}

//=========================================================================

static s32 PrimitiveNextCapacity( s32 Current, s32 Required )
{
    s32 Capacity = MAX( Current, 1 );

    while( Capacity < Required )
    {
        if( Capacity > ( 0x7fffffff / 2 ) )
        {
            return Required;
        }

        Capacity *= 2;
    }

    return Capacity;
}

//=========================================================================

static xbool CreateSpriteIndices( rbuffer& Buffer )
{
    static const u16 Indices[6] = { 0, 1, 2, 2, 3, 0 };
    rbuffer_desc     Desc;
    Desc.Size       = sizeof( Indices );
    Desc.Stride     = sizeof( u16 );
    Desc.UsageFlags = RBUFFER_USAGE_INDEX;
    Desc.pDebugName = "PrimitiveSpriteIndices";

    return rbuffer_Create( Buffer, Desc, Indices );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

PrimitiveStorage::PrimitiveStorage( void )
    : m_VertexBuffer(),
      m_Pipelines(),
      m_WhiteTexture(),
      m_isInitialized( FALSE ),
      m_pContext( NULL ),
      m_BatchDesc(),
      m_BatchStartVertex( 0 ),
      m_iBatchStartIndex( 0 ),
      m_nBatchVertices( 0 ),
      m_nBatchIndices( 0 ),
      m_isBatchOpen( FALSE ),
      m_QueuedVertices(),
      m_QueuedIndices(),
      m_BatchDescs(),
      m_QueuedDraws(),
      m_areQueuedDrawsUploaded( FALSE )
{
}

//=========================================================================

void PrimitiveStorage::Init( const RenderBackendContext& Context )
{
    if( m_isInitialized )
    {
        return;
    }

    if( !Context.IsComplete() )
    {
        RENDER_THROW( "PrimitiveStorage", "backend services were not configured" );
        return;
    }

    m_pContext = &Context;

    x_try;

        m_QueuedVertices.SetCapacity( 8192 );
        m_QueuedIndices.SetCapacity( 24576 );
        m_BatchDescs.SetCapacity( 2048 );
        m_QueuedDraws.SetCapacity( 2048 );
        m_Sprites.SetCapacity( 2048 );
        m_SortedSprites.SetCapacity( 2048 );
        m_SpriteScratch.SetCapacity( 1024 );

        const char* pFailedStage = NULL;

        if( !m_VertexBuffer.Init( sizeof( Vertex ) ) )
        {
            pFailedStage = "StaticGeometryStorage";
        }
        else if( !CreateSpriteIndices( m_SpriteIndices ) )
        {
            pFailedStage = "SpriteIndices";
        }
        else if( !LoadShaders() )
        {
            pFailedStage = "LoadShaders";
        }
        else if( !CreateSamplers() )
        {
            pFailedStage = "CreateSamplers";
        }
        else if( !CreateWhiteTexture() )
        {
            pFailedStage = "CreateWhiteTexture";
        }

        if( pFailedStage )
        {
            Kill();
            RENDER_THROW( "PrimitiveStorage", "initialization failed at stage '%s'", pFailedStage );
            return;
        }

        m_isInitialized = TRUE;

    x_catch_begin;

        Kill();

    x_catch_end_ret;
}

//=========================================================================

void PrimitiveStorage::Kill( void )
{
    ClearBatch();
    ClearQueuedDraws();
    m_QueuedVertices.Clear();
    m_QueuedIndices.Clear();
    m_BatchDescs.Clear();
    m_QueuedDraws.Clear();
    m_Sprites.Clear();
    m_SortedSprites.Clear();
    m_SpriteScratch.Clear();
    rbuffer_Destroy( m_SpriteBuffer );
    rbuffer_Destroy( m_SpriteIndices );
    m_SpriteCapacity = 0;

    DestroyPipelines();
    DestroySamplers();
    vram_DestroyTexture( m_WhiteTexture );

    for( auto& Sprites : m_Programs )
    {
        for( auto& Outputs : Sprites )
        {
            for( auto& Blends : Outputs )
            {
                for( auto& Depths : Blends )
                {
                    for( auto& Program : Depths )
                    {
                        shader_Destroy( Program.Shader );
                        Program = PixelProgram();
                    }
                }
            }
        }
    }

    m_pActiveProgram = NULL;

    for( auto& Sprites : m_VertexShaders )
    {
        for( auto& Vertex : Sprites )
        {
            shader_Destroy( Vertex );
        }
    }

    m_VertexBuffer.Kill();

    m_isInitialized = FALSE;
}

//=========================================================================

void PrimitiveStorage::BeginRender( void )
{
    if( m_isBatchOpen || HasQueuedDraws() )
    {
        RENDER_THROW( "PrimitiveStorage", "stale frame submission batch=%d draws=%d uploaded=%d", m_isBatchOpen,
                      m_QueuedDraws.GetCount(), m_areQueuedDrawsUploaded );
    }

    ASSERTS( !m_isBatchOpen, "Primitive batch survived the previous frame" );
    ASSERTS( !HasQueuedDraws(), "Primitive draw queue survived the previous frame" );

    ClearBatch();
    ClearQueuedDraws();
    m_SpriteScratch.SetCount( 0 );
}

//=========================================================================
//  SHADER RESOURCES
//=========================================================================

xbool PrimitiveStorage::LoadShaders( void )
{
    return TRUE;
}

//=========================================================================

ShaderProgramKey PrimitiveStorage::ResolveProgram( const PipelineDesc& Desc )
{
    const xbool Fog = Desc.Pass.SceneFogMode != RENDER_FOG_NONE || Desc.Pass.BloomFogMode != RENDER_FOG_NONE;
    return ResolvePrimitiveProgram( Desc.Output == PRIMITIVE_OUTPUT_DISTORTION ? SurfaceRecipe::Distortion
                                                                               : SurfaceRecipe::Diffuse,
                                    Desc.Output == PRIMITIVE_OUTPUT_GLOW, ( ShaderBlend )Desc.SourceBlend,
                                    Desc.Pass.WriteSceneDepth, Fog, Desc.InstancedSprite );
}

//=========================================================================

PrimitiveStorage::PixelProgram* PrimitiveStorage::GetProgram( const PipelineDesc& Desc, xbool IsPrewarm )
{
    if( Desc.Output >= PRIMITIVE_OUTPUT_COUNT || Desc.SourceBlend >= PRIMITIVE_BLEND_COUNT )
    {
        return NULL;
    }

    if( Desc.Output == PRIMITIVE_OUTPUT_DISTORTION && Desc.SourceBlend != PRIMITIVE_BLEND_ALPHA )
    {
        return NULL;
    }

    const u32 Fog = Desc.Output != PRIMITIVE_OUTPUT_DISTORTION &&
                    ( Desc.Pass.SceneFogMode != RENDER_FOG_NONE || Desc.Pass.BloomFogMode != RENDER_FOG_NONE );
    const ShaderProgramKey Key = ResolveProgram( Desc );
    PixelProgram&          Program =
        m_Programs[!!Desc.InstancedSprite][Desc.Output][Desc.SourceBlend][!!Desc.Pass.WriteSceneDepth][Fog];

    if( Program.Shader )
    {
        return &Program;
    }

    const A51RecipeProgramDesc* pRecipe = A51FindRecipeProgram( Key.Hash() );

    if( !pRecipe )
    {
        RENDER_THROW( "ShaderCatalog", "primitive program %016llx is absent from catalog",
                      ( unsigned long long )Key.Hash() );
        return NULL;
    }

    if( !IsPrewarm && Program.Shader.pLoad )
    {
        RENDER_THROW( "ShaderCatalog", "program used before bytecode prewarm completed" );
        return NULL;
    }

    shader& Vertex = m_VertexShaders[!!Desc.InstancedSprite][Desc.Pass.WriteSceneDepth || Fog];

    if( !Vertex && !( IsPrewarm ? shader_RequestComboStage( Vertex, pRecipe->PackageFile, SHADER_COMBO_KEYED,
                                                            pRecipe->Key, SHADER_STAGE_VERTEX )
                                : shader_LoadComboStage( Vertex, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                                         SHADER_STAGE_VERTEX ) ) )
    {
        return NULL;
    }

    if( !( IsPrewarm ? shader_RequestComboStage( Program.Shader, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                                 SHADER_STAGE_PIXEL )
                     : shader_LoadComboStage( Program.Shader, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                              SHADER_STAGE_PIXEL ) ) )
    {
        return NULL;
    }

    if( Vertex.ProgramId != pRecipe->VertexId || Program.Shader.ProgramId != pRecipe->PixelId )
    {
        shader_Destroy( Program.Shader );
        Program = PixelProgram();
        RENDER_THROW( "ShaderCatalog", "primitive stage IDs do not match the selected recipe" );
        return NULL;
    }

    Program.pVertex    = &Vertex;
    Program.DrawVertex = Vertex.ResourceSlots[A51Resource_cbPrimitiveDraw];

    if( Program.DrawVertex == 0xFFFFFFFFu )
    {
        shader_Destroy( Program.Shader );
        Program = PixelProgram();
        return NULL;
    }

    Program.Texture = Program.Shader.ResourceSlots[A51Resource_txPrimitive];

    if( Program.Texture == 0xFFFFFFFFu )
    {
        shader_Destroy( Program.Shader );
        Program = PixelProgram();
        return NULL;
    }

    Program.Draw    = Program.Shader.ResourceSlots[A51Resource_cbPrimitiveDraw];
    Program.Scene   = Program.Shader.ResourceSlots[A51Resource_txPrimitiveScene];
    Program.Fog     = Program.Shader.ResourceSlots[A51Resource_cbFogConstants];
    Program.Palette = Program.Shader.ResourceSlots[A51Resource_FogPalette];

    if( Desc.Output == PRIMITIVE_OUTPUT_DISTORTION && Program.Scene == 0xFFFFFFFFu )
    {
        shader_Destroy( Program.Shader );
        Program = PixelProgram();
        return NULL;
    }

    return &Program;
}

//=========================================================================

xbool PrimitiveStorage::CreateSamplers( void )
{
    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        if( !rstate_CreateSampler( m_Samplers[i], static_cast<rstate_sampler_preset>( i ), "PrimitiveSampler" ) )
        {
            DestroySamplers();
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void PrimitiveStorage::DestroySamplers( void )
{
    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        rstate_DestroySampler( m_Samplers[i] );
    }
}

//=========================================================================

xbool PrimitiveStorage::CreateWhiteTexture( void )
{
    vram_texture_desc Desc;
    Desc.Width      = 1;
    Desc.Height     = 1;
    Desc.Format     = VRAM_TEXTURE_FORMAT_RGBA8;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.pDebugName = "PrimitiveWhiteTexture";

    if( !vram_CreateTexture( m_WhiteTexture, Desc ) )
    {
        return FALSE;
    }

    const u32                WhitePixel = 0xffffffffu;
    vram_texture_upload_desc Upload;
    Upload.Region.Width  = 1;
    Upload.Region.Height = 1;
    Upload.Region.Depth  = 1;
    Upload.pData         = &WhitePixel;
    Upload.Size          = sizeof( WhitePixel );
    Upload.RowPitch      = sizeof( WhitePixel );
    Upload.SlicePitch    = sizeof( WhitePixel );

    return vram_UploadTexture( m_WhiteTexture, Upload );
}

//=========================================================================

void PrimitiveStorage::DestroyPipelines( void )
{
    m_Pipelines.Reset();
}

//=========================================================================

xbool PrimitiveStorage::QueuePipeline( const PipelineDesc& Desc )
{
    if( !GetOrCreatePipeline( Desc, TRUE ) )
    {
        return FALSE;
    }

    if( Desc.Topology == SHADER_TOPOLOGY_TRIANGLE_LIST && !Desc.InstancedSprite )
    {
        PipelineDesc Sprite    = Desc;
        Sprite.InstancedSprite = TRUE;
        return GetOrCreatePipeline( Sprite, TRUE ) != NULL;
    }

    return TRUE;
}

//=========================================================================

void PrimitiveStorage::ReleaseLevelPipelines( void )
{
    m_Pipelines.Reset();

    for( auto& Sprites : m_Programs )
    {
        for( auto& Outputs : Sprites )
        {
            for( auto& Blends : Outputs )
            {
                for( auto& Depths : Blends )
                {
                    for( auto& Program : Depths )
                    {
                        shader_Destroy( Program.Shader );
                        Program = PixelProgram();
                    }
                }
            }
        }
    }

    m_pActiveProgram = NULL;
}

//=========================================================================
//  PIPELINE STATE
//=========================================================================

xbool PrimitiveStorage::ValidatePipelineDesc( const PipelineDesc& Desc )
{
    if( !ValidatePipelineState( Desc ) )
    {
        return FALSE;
    }

    if( ( static_cast<u32>( Desc.Pass.ColorFormat ) >= static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
        ( static_cast<u32>( Desc.Pass.DepthFormat ) > static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
        ( ( Desc.Pass.SampleCount != 1 ) && ( Desc.Pass.SampleCount != 2 ) && ( Desc.Pass.SampleCount != 4 ) &&
          ( Desc.Pass.SampleCount != 8 ) ) )
    {
        return FALSE;
    }

    if( ( Desc.Pass.ColorFormat == RTARGET_FORMAT_DEPTH_STENCIL ) ||
        ( Desc.Pass.ColorFormat == RTARGET_FORMAT_DEPTH32F ) )
    {
        return FALSE;
    }

    if( ( Desc.Pass.DepthFormat != RTARGET_FORMAT_COUNT ) &&
        ( Desc.Pass.DepthFormat != RTARGET_FORMAT_DEPTH_STENCIL ) &&
        ( Desc.Pass.DepthFormat != RTARGET_FORMAT_DEPTH32F ) )
    {
        return FALSE;
    }

    if( ( ( Desc.Depth == RSTATE_DEPTH_PRESET_NORMAL ) || ( Desc.Depth == RSTATE_DEPTH_PRESET_NO_WRITE ) ||
          ( Desc.Depth == RSTATE_DEPTH_PRESET_WRITE_ALWAYS ) ) &&
        ( Desc.Pass.DepthFormat == RTARGET_FORMAT_COUNT ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveStorage::ValidatePipelineState( const PipelineDesc& Desc )
{
    if( ( Desc.Topology <= SHADER_TOPOLOGY_UNDEFINED ) || ( Desc.Topology > SHADER_TOPOLOGY_TRIANGLE_STRIP ) ||
        ( Desc.InstancedSprite && Desc.Topology != SHADER_TOPOLOGY_TRIANGLE_LIST ) || ( Desc.Blend < 0 ) ||
        ( Desc.Blend >= RSTATE_BLEND_PRESET_COUNT ) || ( Desc.Depth < 0 ) ||
        ( Desc.Depth >= RSTATE_DEPTH_PRESET_COUNT ) || ( Desc.Raster < 0 ) ||
        ( Desc.Raster >= RSTATE_RASTER_PRESET_COUNT ) || ( Desc.Sampler < 0 ) ||
        ( Desc.Sampler >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveStorage::BuildPipelineDesc( render_pipeline_desc& Out, const PipelineDesc& Desc, xbool IsPrewarm )
{
    static xprofile_counter Descriptors =
        x_GetProfiler().RegisterCounter( "PrimitivePipelineDescriptors", "RenderCounter" );
    Descriptors.Add();

    if( !ValidatePipelineDesc( Desc ) )
    {
        return FALSE;
    }

    static const shader_vertex_buffer_desc VertexBuffer = []()
    {
        shader_vertex_buffer_desc Result;
        Result.Slot   = 0;
        Result.Stride = sizeof( Vertex );

        return Result;
    }();

    static const shader_vertex_element Layout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( Vertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof( Vertex, Color ) ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( Vertex, UV ) )
    };

    static const shader_vertex_buffer_desc SpriteBuffer = []()
    {
        shader_vertex_buffer_desc Result;
        Result.Slot         = 0;
        Result.Stride       = sizeof( primitive_sprite_instance );
        Result.bPerInstance = TRUE;

        return Result;
    }();
    static const shader_vertex_element SpriteLayout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( primitive_sprite_instance, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof( primitive_sprite_instance, Color ) ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( primitive_sprite_instance, Right ) ),
        shader_vertex_element( 3, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( primitive_sprite_instance, Up ) ),
        shader_vertex_element( 4, 0, SHADER_VERTEX_FORMAT_FLOAT4, offsetof( primitive_sprite_instance, UVRect ) )
    };

    Out = render_pipeline_desc();

    PixelProgram* pProgram = GetProgram( Desc, IsPrewarm );

    if( !pProgram )
    {
        return FALSE;
    }

    Out.Shader.pVertexShader     = pProgram->pVertex;
    Out.Shader.pPixelShader      = &pProgram->Shader;
    Out.Shader.pVertexBuffers    = Desc.InstancedSprite ? &SpriteBuffer : &VertexBuffer;
    Out.Shader.VertexBufferCount = 1;
    Out.Shader.pInputElements    = Desc.InstancedSprite ? SpriteLayout : Layout;
    Out.Shader.InputElementCount = Desc.InstancedSprite ? ARRAYSIZE( SpriteLayout ) : ARRAYSIZE( Layout );
    Out.Shader.Topology          = Desc.Topology;
    Out.Depth                    = rstate_GetDepthDesc( Desc.Depth );
    Out.Raster                   = rstate_GetRasterDesc( Desc.Raster );
    Out.ColorCount               = Desc.Pass.WriteSceneDepth ? 2 : 1;
    Out.DepthFormat              = Desc.Pass.DepthFormat;
    Out.SampleCount              = Desc.Pass.SampleCount;
    Out.pDebugName               = Desc.pDebugName ? Desc.pDebugName : "PrimitivePipeline";

    Out.ColorTargets[0].Format = Desc.Pass.ColorFormat;
    Out.ColorTargets[0].Blend  = rstate_GetBlendDesc( Desc.Blend );

    if( Desc.Pass.WriteSceneDepth )
    {
        Out.ColorTargets[1].Format = RTARGET_FORMAT_R32F;
        Out.ColorTargets[1].Blend.ColorWriteMask =
            Out.Depth.bDepthWrite ? RSTATE_COLOR_WRITE_RED : RSTATE_COLOR_WRITE_NONE;
    }

    return TRUE;
}

//=========================================================================

render_pipeline* PrimitiveStorage::GetOrCreatePipeline( const PipelineDesc& Desc, xbool isPrewarm )
{
    const ShaderProgramKey Program = ResolveProgram( Desc );
    const u64 State = GraphicsPipelineStateKey( Desc.Pass.ColorFormat, Desc.Pass.DepthFormat, Desc.Pass.SampleCount,
                                                Desc.Blend, Desc.Depth, Desc.Raster, Desc.Topology );
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
//  DRAW RESOURCES
//=========================================================================

xbool PrimitiveStorage::BindResources( const BatchDesc& Desc, s32 DrawIndex, const BackendPassInputs& Inputs )
{
    auto PushConstants = [&]( const DrawConstants& Constants ) -> xbool
    {
        if( !shader_PushUniformData( SHADER_STAGE_VERTEX, m_pActiveProgram->DrawVertex, &Constants,
                                     sizeof( Constants ) ) )
        {
            RENDER_THROW( "PrimitiveStorage", "failed to push vertex constants packet=%d output=%d", DrawIndex,
                          static_cast<s32>( Desc.Output ) );
            return FALSE;
        }

        if( m_pActiveProgram->Draw != 0xFFFFFFFFu &&
            !shader_PushUniformData( SHADER_STAGE_PIXEL, m_pActiveProgram->Draw, &Constants, sizeof( Constants ) ) )
        {
            RENDER_THROW( "PrimitiveStorage", "failed to push pixel constants packet=%d output=%d", DrawIndex,
                          static_cast<s32>( Desc.Output ) );
            return FALSE;
        }

        return TRUE;
    };

    if( !PushConstants( Desc.Constants ) )
    {
        return FALSE;
    }

    if( m_pActiveProgram->Fog != 0xFFFFFFFFu )
    {
        const PostProcessFeature* pPost = m_pContext->pPostProcess;

        if( !pPost )
        {
            return FALSE;
        }

        RenderFogConstants Fog;
        pPost->GetFogConstants( Fog );
        Fog.SceneMode = Desc.Pipeline.Pass.SceneFogMode;
        Fog.BloomMode = Desc.Pipeline.Pass.BloomFogMode;

        if( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_pActiveProgram->Fog, &Fog, sizeof( Fog ) ) )
        {
            return FALSE;
        }

        if( m_pActiveProgram->Palette != 0xFFFFFFFFu &&
            !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Palette,
                                                         pPost->GetFogPaletteResource(),
                                                         pPost->GetFogPaletteSampler() ) ) )
        {
            return FALSE;
        }
    }

    const shader_resource* pTexture = Desc.pTexture;

    if( !pTexture || !*pTexture )
    {
        pTexture = vram_GetShaderResource( m_WhiteTexture );

        if( !pTexture || !*pTexture )
        {
            RENDER_THROW( "PrimitiveStorage", "missing texture and white fallback packet=%d output=%d", DrawIndex,
                          static_cast<s32>( Desc.Output ) );
            return FALSE;
        }
    }

    rstate_sampler_preset Sampler = Desc.Pipeline.Sampler;

    if( ( Sampler < 0 ) || ( Sampler >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        RENDER_THROW( "PrimitiveStorage", "invalid sampler packet=%d sampler=%d output=%d", DrawIndex,
                      static_cast<s32>( Sampler ), static_cast<s32>( Desc.Output ) );
        return FALSE;
    }

    if( !m_Samplers[Sampler] )
    {
        RENDER_THROW( "PrimitiveStorage", "unavailable sampler packet=%d sampler=%d output=%d", DrawIndex,
                      static_cast<s32>( Sampler ), static_cast<s32>( Desc.Output ) );
        return FALSE;
    }

    const shader_resource* pScene = pTexture;

    if( Desc.Output == render::PRIMITIVE_OUTPUT_DISTORTION )
    {
        pScene = Inputs.pDistortionScene;

        if( !pScene || !*pScene )
        {
            RENDER_THROW( "PrimitiveStorage", "missing distortion scene packet=%d", DrawIndex );
            return FALSE;
        }
    }

    if( !shader_BindSampler(
            shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Texture, pTexture, &m_Samplers[Sampler] ) ) )
    {
        RENDER_THROW( "PrimitiveStorage", "failed to bind texture sampler packet=%d slot=%u output=%d", DrawIndex,
                      m_pActiveProgram->Texture, static_cast<s32>( Desc.Output ) );
        return FALSE;
    }

    const rstate_sampler_preset SceneSampler = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;

    if( m_pActiveProgram->Scene != 0xFFFFFFFFu &&
        !shader_BindSampler(
            shader_sampler_binding( SHADER_STAGE_PIXEL, m_pActiveProgram->Scene, pScene, &m_Samplers[SceneSampler] ) ) )
    {
        RENDER_THROW( "PrimitiveStorage", "failed to bind scene sampler packet=%d slot=%u output=%d", DrawIndex,
                      m_pActiveProgram->Scene, static_cast<s32>( Desc.Output ) );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  SPRITE SUBMISSION
//=========================================================================

xbool PrimitiveStorage::SubmitSprites( const BatchDesc& Desc, const primitive_sprite_instance* pSprites, s32 Count )
{
    if( !m_isInitialized || m_isBatchOpen || m_areQueuedDrawsUploaded || !pSprites || Count <= 0 ||
        !Desc.Pipeline.InstancedSprite || !ValidatePipelineState( Desc.Pipeline ) ||
        !PrimitiveStagingFits( static_cast<u64>( m_Sprites.GetCount() ) + Count,
                               sizeof( primitive_sprite_instance ) ) ||
        !PrimitiveStagingFits( static_cast<u64>( m_QueuedDraws.GetCount() ) + Count, sizeof( QueuedDraw ) ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < Count; ++i )
    {
        const primitive_sprite_instance& Sprite = pSprites[i];

        if( !vector3( Sprite.Position ).IsValid() || !vector3( Sprite.Right ).IsValid() ||
            !vector3( Sprite.Up ).IsValid() )
        {
            return FALSE;
        }

        for( u32 UV = 0; UV < 4; ++UV )
        {
            if( !x_isvalid( Sprite.UVRect[UV] ) )
            {
                return FALSE;
            }
        }
    }

    const s32 DescIndex = AddBatchDesc( Desc );
    if( DescIndex < 0 )
    {
        return FALSE;
    }

    const s32 SpriteCount = m_Sprites.GetCount() + Count;
    const s32 DrawCount   = m_QueuedDraws.GetCount() + Count;

    if( m_Sprites.GetCapacity() < SpriteCount )
    {
        m_Sprites.SetCapacity(
            MIN( PrimitiveNextCapacity( m_Sprites.GetCapacity(), SpriteCount ),
                 static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( primitive_sprite_instance ) ) ) );
    }

    if( m_QueuedDraws.GetCapacity() < DrawCount )
    {
        m_QueuedDraws.SetCapacity( MIN( PrimitiveNextCapacity( m_QueuedDraws.GetCapacity(), DrawCount ),
                                        static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( QueuedDraw ) ) ) );
    }

    for( s32 i = 0; i < Count; ++i )
    {
        QueuedDraw& Draw    = m_QueuedDraws.Append();
        Draw                = QueuedDraw();
        Draw.DescIndex      = DescIndex;
        Draw.SortDepth      = pSprites[i].Position.Z;
        Draw.FirstSprite    = m_Sprites.GetCount();
        Draw.SpriteCount    = 1;

        m_Sprites.Append() = pSprites[i];
    }

    return TRUE;
}

//=========================================================================

xbool PrimitiveStorage::IsSpriteDraw( s32 DrawIndex ) const
{
    return m_BatchDescs[m_QueuedDraws[DrawIndex].DescIndex].Pipeline.InstancedSprite;
}

//=========================================================================

void PrimitiveStorage::BeginSpriteBatches( void )
{
    m_SortedSprites.SetCount( 0 );

    if( m_SortedSprites.GetCapacity() < m_Sprites.GetCount() )
    {
        m_SortedSprites.SetCapacity( m_Sprites.GetCapacity() );
    }
}

//=========================================================================

s32 PrimitiveStorage::AppendSpriteBatch( s32 DrawIndex, s32 PreviousDrawIndex )
{
    QueuedDraw& Draw = m_QueuedDraws[DrawIndex];

    if( !m_BatchDescs[Draw.DescIndex].Pipeline.InstancedSprite || Draw.SpriteCount != 1 )
    {
        return -1;
    }

    const primitive_sprite_instance Sprite = m_Sprites[Draw.FirstSprite];
    Draw.FirstSprite                       = m_SortedSprites.GetCount();

    m_SortedSprites.Append() = Sprite;

    if( PreviousDrawIndex >= 0 )
    {
        QueuedDraw&         Previous     = m_QueuedDraws[PreviousDrawIndex];
        const BatchDesc&    PreviousDesc = m_BatchDescs[Previous.DescIndex];
        const BatchDesc&    DrawDesc     = m_BatchDescs[Draw.DescIndex];
        const PipelineDesc& A            = PreviousDesc.Pipeline;
        const PipelineDesc& B            = DrawDesc.Pipeline;

        if( ( Previous.DescIndex == Draw.DescIndex ||
              ( A.InstancedSprite && A.Output == B.Output && A.SourceBlend == B.SourceBlend &&
                A.Topology == B.Topology && A.Blend == B.Blend && A.Depth == B.Depth &&
                A.Raster == B.Raster && A.Sampler == B.Sampler && PreviousDesc.pTexture == DrawDesc.pTexture &&
                x_memcmp( &PreviousDesc.Constants, &DrawDesc.Constants, sizeof( DrawConstants ) ) == 0 ) ) &&
            Previous.FirstSprite + Previous.SpriteCount == Draw.FirstSprite )
        {
            Previous.SpriteCount++;
            return PreviousDrawIndex;
        }
    }

    return DrawIndex;
}

//=========================================================================

xbool PrimitiveStorage::UploadSpriteBatches( FrameGraphBuilder& Graph )
{
    const u32 Count = m_SortedSprites.GetCount();

    if( !Count )
    {
        return TRUE;
    }

    if( !m_SpriteBuffer || m_SpriteCapacity < Count )
    {
        const u32 Capacity =
            MIN( static_cast<u32>( PrimitiveNextCapacity( m_SpriteCapacity, Count ) ),
                 FrameStorage::MAX_STREAM_BYTES / static_cast<u32>( sizeof( primitive_sprite_instance ) ) );
        rbuffer_desc Desc;
        Desc.Size       = Capacity * sizeof( primitive_sprite_instance );
        Desc.Stride     = sizeof( primitive_sprite_instance );
        Desc.UsageFlags = RBUFFER_USAGE_VERTEX;
        Desc.pDebugName = "PrimitiveSprites";

        if( !rbuffer_Create( m_SpriteBuffer, Desc ) )
        {
            return FALSE;
        }

        m_SpriteCapacity = Capacity;
    }

    return Graph.QueueUpload( m_SpriteBuffer, m_SortedSprites.GetPtr(), Count * sizeof( primitive_sprite_instance ), 0,
                           TRUE );
}

//=========================================================================
//  PRIMITIVE SUBMISSION
//=========================================================================

xbool PrimitiveStorage::BeginBatch( const BatchDesc& Desc )
{
    if( !m_isInitialized || m_areQueuedDrawsUploaded || ( Desc.Output < render::PRIMITIVE_OUTPUT_COLOR ) ||
        ( Desc.Output >= render::PRIMITIVE_OUTPUT_COUNT ) || !x_isvalid( Desc.SortDepth ) ||
        !ValidatePipelineState( Desc.Pipeline ) )
    {
        return FALSE;
    }

    if( m_isBatchOpen )
    {
        return FALSE;
    }

    m_BatchDesc        = Desc;
    m_BatchStartVertex = m_QueuedVertices.GetCount();
    m_iBatchStartIndex = m_QueuedIndices.GetCount();
    m_nBatchVertices   = 0;
    m_nBatchIndices    = 0;
    m_isBatchOpen      = TRUE;

    return TRUE;
}

//=========================================================================

xbool PrimitiveStorage::SubmitBatch( const Vertex* pVertices, s32 nVertices, const u16* pIndices, s32 nIndices )
{
    if( !m_isBatchOpen )
    {
        return FALSE;
    }

    if( !pVertices || !pIndices )
    {
        return FALSE;
    }

    if( ( nVertices <= 0 ) || ( nIndices <= 0 ) ||
        !PrimitiveStagingFits( static_cast<u64>( nVertices ), sizeof( Vertex ) ) ||
        !PrimitiveStagingFits( static_cast<u64>( nIndices ), sizeof( u16 ) ) )
    {
        return FALSE;
    }

    if( nVertices > MAX_BATCH_VERTICES )
    {
        return FALSE;
    }

    for( s32 i = 0; i < nIndices; i++ )
    {
        if( pIndices[i] >= nVertices )
        {
            return FALSE;
        }
    }

    if( ( m_nBatchVertices + nVertices ) > MAX_BATCH_VERTICES )
    {
        if( m_QueuedDraws.GetCount() >= m_QueuedDraws.GetCapacity() )
        {
            return FALSE;
        }

        BatchDesc Desc = m_BatchDesc;

        if( !EndBatch() )
        {
            return FALSE;
        }

        if( !BeginBatch( Desc ) )
        {
            return FALSE;
        }
    }

    if( ( m_nBatchVertices == 0 ) && ( m_QueuedDraws.GetCount() >= m_QueuedDraws.GetCapacity() ) )
    {
        return FALSE;
    }

    const s32 VertexOffset     = m_QueuedVertices.GetCount();
    const s32 IndexOffset      = m_QueuedIndices.GetCount();
    const u64 RequiredVertices = static_cast<u64>( VertexOffset ) + static_cast<u64>( nVertices );
    const u64 RequiredIndices  = static_cast<u64>( IndexOffset ) + static_cast<u64>( nIndices );

    if( !PrimitiveStagingFits( RequiredVertices, sizeof( Vertex ) ) ||
        !PrimitiveStagingFits( RequiredIndices, sizeof( u16 ) ) )
    {
        return FALSE;
    }

    const s32 VertexCount = static_cast<s32>( RequiredVertices );
    const s32 IndexCount  = static_cast<s32>( RequiredIndices );

    if( m_QueuedVertices.GetCapacity() < VertexCount )
    {
        s32 NewCapacity = PrimitiveNextCapacity( m_QueuedVertices.GetCapacity(), VertexCount );
        NewCapacity     = MIN( NewCapacity, static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( Vertex ) ) );
        m_QueuedVertices.SetCapacity( NewCapacity );
    }

    if( m_QueuedIndices.GetCapacity() < IndexCount )
    {
        s32 NewCapacity = PrimitiveNextCapacity( m_QueuedIndices.GetCapacity(), IndexCount );
        NewCapacity     = MIN( NewCapacity, static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( u16 ) ) );
        m_QueuedIndices.SetCapacity( NewCapacity );
    }

    m_QueuedVertices.SetCount( VertexCount );
    x_memcpy( &m_QueuedVertices[VertexOffset], pVertices, nVertices * sizeof( Vertex ) );

    m_QueuedIndices.SetCount( IndexCount );

    for( s32 i = 0; i < nIndices; i++ )
    {
        // Indices stay batch-local; the draw applies m_BatchStartVertex once.
        m_QueuedIndices[IndexOffset + i] = static_cast<u16>( m_nBatchVertices + pIndices[i] );
    }

    m_nBatchVertices += nVertices;
    m_nBatchIndices += nIndices;

    return TRUE;
}

//=========================================================================

xbool PrimitiveStorage::EndBatch( void )
{
    if( !m_isBatchOpen )
    {
        return TRUE;
    }

    if( ( m_nBatchVertices == 0 ) || ( m_nBatchIndices == 0 ) )
    {
        ClearBatch();
        return TRUE;
    }

    const s32 DrawIndex = m_QueuedDraws.GetCount();

    if( DrawIndex >= m_QueuedDraws.GetCapacity() )
    {
        m_QueuedVertices.SetCount( m_BatchStartVertex );
        m_QueuedIndices.SetCount( m_iBatchStartIndex );
        ClearBatch();
        RENDER_THROW( "PrimitiveStorage", "primitive draw queue capacity exhausted" );
        return FALSE;
    }

    const s32 DescIndex = AddBatchDesc( m_BatchDesc );
    if( DescIndex < 0 )
    {
        m_QueuedVertices.SetCount( m_BatchStartVertex );
        m_QueuedIndices.SetCount( m_iBatchStartIndex );
        ClearBatch();
        return FALSE;
    }

    m_QueuedDraws.SetCount( DrawIndex + 1 );

    QueuedDraw& Draw = m_QueuedDraws[DrawIndex];
    Draw             = QueuedDraw();
    Draw.DescIndex   = DescIndex;
    Draw.SortDepth   = m_BatchDesc.SortDepth;
    Draw.StartIndex  = m_iBatchStartIndex;
    Draw.BaseVertex  = m_BatchStartVertex;
    Draw.IndexCount  = m_nBatchIndices;
    Draw.FirstSprite = Draw.SpriteCount = 0;

    m_areQueuedDrawsUploaded = FALSE;
    ClearBatch();

    return TRUE;
}

//=========================================================================
//  UPLOAD AND EXECUTION
//=========================================================================

xbool PrimitiveStorage::UploadQueuedDraws( FrameGraphBuilder& Graph )
{
    if( m_areQueuedDrawsUploaded )
    {
        return FALSE;
    }

    if( m_isBatchOpen )
    {
        return FALSE;
    }

    if( !HasQueuedDraws() )
    {
        return TRUE;
    }

    TransientGeometryStorage::Primitive PrimitiveData;
    PrimitiveData.pVertex   = m_QueuedVertices.GetPtr();
    PrimitiveData.pIndex    = m_QueuedIndices.GetPtr();
    PrimitiveData.nVertices = m_QueuedVertices.GetCount();
    PrimitiveData.nIndices  = m_QueuedIndices.GetCount();

    m_areQueuedDrawsUploaded = PrimitiveData.nVertices == 0 || m_VertexBuffer.UploadPrimitive( PrimitiveData, Graph );

    return m_areQueuedDrawsUploaded;
}

//=========================================================================

render::primitive_blend_mode PrimitiveStorage::GetQueuedDrawBlend( s32 DrawIndex ) const
{
    ASSERT( ( DrawIndex >= 0 ) && ( DrawIndex < m_QueuedDraws.GetCount() ) );
    return m_BatchDescs[m_QueuedDraws[DrawIndex].DescIndex].Blend;
}

//=========================================================================

xbool PrimitiveStorage::QueuedDrawReadsSceneColor( s32 DrawIndex ) const
{
    ASSERT( DrawIndex >= 0 && DrawIndex < m_QueuedDraws.GetCount() );
    return ResolveProgram( m_BatchDescs[m_QueuedDraws[DrawIndex].DescIndex].Pipeline ).Surface == SurfaceRecipe::Distortion;
}

//=========================================================================

render::primitive_output_mode PrimitiveStorage::GetQueuedDrawOutput( s32 DrawIndex ) const
{
    ASSERT( ( DrawIndex >= 0 ) && ( DrawIndex < m_QueuedDraws.GetCount() ) );
    return m_BatchDescs[m_QueuedDraws[DrawIndex].DescIndex].Output;
}

//=========================================================================

f32 PrimitiveStorage::GetQueuedDrawSortDepth( s32 DrawIndex ) const
{
    ASSERT( ( DrawIndex >= 0 ) && ( DrawIndex < m_QueuedDraws.GetCount() ) );
    return m_QueuedDraws[DrawIndex].SortDepth;
}

//=========================================================================

void PrimitiveStorage::DeclareQueuedResources( FrameGraphBuilder& Graph, RenderPassId Pass, s32 DrawIndex ) const
{
    const QueuedDraw& Draw = m_QueuedDraws[DrawIndex];
    const BatchDesc&  Desc = m_BatchDescs[Draw.DescIndex];
    if( Desc.Pipeline.InstancedSprite )
    {
        Graph.Read( Pass, m_SpriteBuffer, RENDER_RESOURCE_ACCESS_VERTEX_READ );
        Graph.Read( Pass, m_SpriteIndices, RENDER_RESOURCE_ACCESS_INDEX_READ );
    }
    else
    {
        m_VertexBuffer.DeclareResources( Graph, Pass );
    }
    Graph.Read( Pass, Desc.pTexture );
    Graph.Read( Pass, vram_GetShaderResource( m_WhiteTexture ) );
    Graph.Read( Pass, m_pContext->pPostProcess->GetFogPaletteResource() );
}

//=========================================================================

xbool PrimitiveStorage::PrepareQueuedDraw( s32 DrawIndex, const PassDesc& Pass )
{
    if( DrawIndex < 0 || DrawIndex >= m_QueuedDraws.GetCount() )
    {
        return FALSE;
    }

    QueuedDraw&  Draw = m_QueuedDraws[DrawIndex];
    PipelineDesc Desc = m_BatchDescs[Draw.DescIndex].Pipeline;
    Desc.Pass         = Pass;

    if( Pass.DepthFormat == RTARGET_FORMAT_COUNT )
    {
        if( Desc.Depth == RSTATE_DEPTH_PRESET_WRITE_ALWAYS )
        {
            RENDER_THROW( "PrimitiveStorage", "depth-writing packet has no depth target" );
            return FALSE;
        }

        Desc.Depth = RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE;
    }

    Draw.pPipeline = GetOrCreatePipeline( Desc );
    Draw.pProgram  = GetProgram( Desc );

    return Draw.pPipeline && Draw.pProgram;
}

//=========================================================================

xbool PrimitiveStorage::DrawQueuedDraw( s32 DrawIndex, const PassDesc& PassData, const BackendPassInputs& Inputs )
{
    if( !m_areQueuedDrawsUploaded || ( DrawIndex < 0 ) || ( DrawIndex >= m_QueuedDraws.GetCount() ) )
    {
        RENDER_THROW( "PrimitiveStorage", "invalid queued draw packet=%d count=%d uploaded=%d", DrawIndex,
                      m_QueuedDraws.GetCount(), m_areQueuedDrawsUploaded );
        return FALSE;
    }

    const QueuedDraw& Draw     = m_QueuedDraws[DrawIndex];
    BatchDesc         DescData = m_BatchDescs[Draw.DescIndex];
    DescData.Pipeline.Pass     = PassData;
    DescData.Constants.PassDimensions.Set( PassData.Width, PassData.Height, 1.0f / MAX( PassData.Width, 1u ),
                                           1.0f / MAX( PassData.Height, 1u ) );

    if( !( DescData.Pipeline.InstancedSprite ? ( rbuffer_BindVertex( m_SpriteBuffer, 0 ) &&
                                                 rbuffer_BindIndex( m_SpriteIndices, RBUFFER_INDEX_FORMAT_U16 ) )
                                             : m_VertexBuffer.BindBuffers() ) )
    {
        RENDER_THROW( "PrimitiveStorage", "failed to bind vertex/index buffers packet=%d output=%d", DrawIndex,
                      static_cast<s32>( DescData.Output ) );
        return FALSE;
    }

    m_pActiveProgram = Draw.pProgram;

    if( !Draw.pPipeline || !m_pActiveProgram || !render_BindPipeline( *Draw.pPipeline ) )
    {
        RENDER_THROW( "PrimitiveStorage",
                      "failed to bind pipeline packet=%d output=%d color=%d depth=%d samples=%u blend=%d", DrawIndex,
                      static_cast<s32>( DescData.Output ), static_cast<s32>( DescData.Pipeline.Pass.ColorFormat ),
                      static_cast<s32>( DescData.Pipeline.Pass.DepthFormat ), DescData.Pipeline.Pass.SampleCount,
                      static_cast<s32>( DescData.Pipeline.Blend ) );
        return FALSE;
    }

    if( !BindResources( DescData, DrawIndex, Inputs ) )
    {
        return FALSE;
    }

    if( !( DescData.Pipeline.InstancedSprite
               ? rdraw_DrawIndexedInstanced( 6, Draw.SpriteCount, 0, 0, Draw.FirstSprite )
               : m_VertexBuffer.DrawIndexed( Draw.IndexCount, Draw.StartIndex, Draw.BaseVertex ) ) )
    {
        RENDER_THROW( "PrimitiveStorage", "indexed draw failed packet=%d output=%d indices=%d start=%d base=%d",
                      DrawIndex, static_cast<s32>( DescData.Output ), Draw.IndexCount, Draw.StartIndex,
                      Draw.BaseVertex );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void PrimitiveStorage::DiscardQueuedDraws( void )
{
    ClearBatch();
    ClearQueuedDraws();
}

//=========================================================================

void PrimitiveStorage::ClearBatch( void )
{
    m_BatchDesc        = BatchDesc();
    m_BatchStartVertex = 0;
    m_iBatchStartIndex = 0;
    m_nBatchVertices   = 0;
    m_nBatchIndices    = 0;
    m_isBatchOpen      = FALSE;
}

//=========================================================================

s32 PrimitiveStorage::AddBatchDesc( const BatchDesc& Desc )
{
    const s32 Count = m_BatchDescs.GetCount() + 1;
    if( !PrimitiveStagingFits( static_cast<u64>( Count ), sizeof( BatchDesc ) ) )
    {
        return -1;
    }

    if( m_BatchDescs.GetCapacity() < Count )
    {
        m_BatchDescs.SetCapacity( MIN( PrimitiveNextCapacity( m_BatchDescs.GetCapacity(), Count ),
                                      static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( BatchDesc ) ) ) );
    }

    m_BatchDescs.Append() = Desc;
    return Count - 1;
}

//=========================================================================

void PrimitiveStorage::ClearQueuedDraws( void )
{
    m_QueuedVertices.SetCount( 0 );
    m_QueuedIndices.SetCount( 0 );
    m_BatchDescs.SetCount( 0 );
    m_QueuedDraws.SetCount( 0 );
    m_Sprites.SetCount( 0 );
    m_SortedSprites.SetCount( 0 );
    m_areQueuedDrawsUploaded = FALSE;
}

//=========================================================================

xbool PrimitiveStorage::BuildDrawConstants( DrawConstants& Out, const matrix4& LocalToWorld, const view& RenderView )
{
    Out.LocalToClip = RenderView.GetW2C() * LocalToWorld;
    Out.LocalToView = RenderView.GetW2V() * LocalToWorld;
    return TRUE;
}

//=========================================================================

} // namespace render
