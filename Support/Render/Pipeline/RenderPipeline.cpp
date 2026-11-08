//=========================================================================
//
//  RenderPipeline.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RenderPipeline.hpp"
#include "a51_shader_catalog.hpp"

#include "../Render.hpp"
#include "../Features/Bloom/BloomEffect.hpp"
#include "../Features/Output/OutputFeature.hpp"
#include "../Features/PostProcess/PostProcessFeature.hpp"
#include "e_Composite.hpp"
#include "e_Engine.hpp"
#include "e_VRAM.hpp"
#include "x_debug.hpp"
#include "x_math.hpp"

namespace render
{

//=========================================================================
//  CONSTANTS
//=========================================================================

static const rtarget_format kSceneDepthFormat        = RTARGET_FORMAT_DEPTH_STENCIL;
static const u64            kOutputHistoryFeatureKey = 0x5343454e4f555448ull;

//=========================================================================
//  TYPES
//=========================================================================

struct RenderPipeline::CopyRegionRequest
{
    RenderPipeline*      pRenderer;
    RenderResourceHandle Destination;
    RenderResourceHandle Source;
    u32                  SourceX;
    u32                  SourceY;
    u32                  CopyWidth;
    u32                  CopyHeight;
    f32                  SourceU;
    f32                  SourceV;
    f32                  SourceWidth;
    f32                  SourceHeight;
};

//-------------------------------------------------------------------------

struct RenderPipeline::OutputHistoryState
{
    OutputHistoryState( void )
        : HistoryKey( 0 ),
          ScopeId( -2 ),
          Extent(),
          Format( RTARGET_FORMAT_COUNT ),
          SampleCount( 0 ),
          Valid( FALSE )
    {
    }

    u64            HistoryKey;
    u64            ResetToken     = 0;
    u64            LastUsedSerial = 0;
    s32            ScopeId;
    RenderExtent   Extent;
    rtarget_format Format;
    u32            SampleCount;
    xbool          Valid;
};

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
u32 ScaleDimension( u32 Dimension, f32 Scale )
{
    return MAX( 1u, static_cast<u32>( x_ceil( static_cast<f32>( Dimension ) * Scale ) ) );
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

RenderPipeline& GetRenderPipeline( void )
{
    static RenderPipeline Pipeline;
    return Pipeline;
}

//=========================================================================
//  RENDER FRAME SUBMISSION
//=========================================================================

void RenderSubmission::Begin( void )
{
    if( m_isOpen )
    {
        RENDER_THROW( "RenderSubmission", "cannot begin while another submission is active" );
    }

    m_Geom.SetCount( 0 );
    m_DynamicGeom.SetCount( 0 );
    m_DynamicVertexStorage.SetCount( 0 );
    m_DynamicIndexStorage.SetCount( 0 );
    m_DynamicDamageStorage.SetCount( 0 );
    m_DynamicVertexOffsets.SetCount( 0 );
    m_DynamicIndexOffsets.SetCount( 0 );
    m_DynamicDamageOffsets.SetCount( 0 );
    m_TransformStorage.SetCount( 0 );

    m_pEnvironmentCubeMap = NULL;
    m_NextGeomSequence    = 0;

    m_isSceneSubmitted = FALSE;
    m_isOpen           = TRUE;
    m_isSealed         = FALSE;
}

//=========================================================================

void RenderSubmission::Reset( void )
{
    m_Geom.SetCount( 0 );
    m_DynamicGeom.SetCount( 0 );
    m_DynamicVertexStorage.SetCount( 0 );
    m_DynamicIndexStorage.SetCount( 0 );
    m_DynamicDamageStorage.SetCount( 0 );
    m_DynamicVertexOffsets.SetCount( 0 );
    m_DynamicIndexOffsets.SetCount( 0 );
    m_DynamicDamageOffsets.SetCount( 0 );
    m_TransformStorage.SetCount( 0 );

    m_pEnvironmentCubeMap = NULL;
    m_NextGeomSequence    = 0;

    m_isSceneSubmitted = FALSE;
    m_isOpen           = FALSE;
    m_isSealed         = FALSE;
}

//=========================================================================

void RenderSubmission::RebindDynamicPointers( void )
{
    ASSERT( m_DynamicGeom.GetCount() == m_DynamicVertexOffsets.GetCount() );
    ASSERT( m_DynamicGeom.GetCount() == m_DynamicIndexOffsets.GetCount() );
    ASSERT( m_DynamicGeom.GetCount() == m_DynamicDamageOffsets.GetCount() );

    for( s32 i = 0; i < m_DynamicGeom.GetCount(); ++i )
    {
        dynamic_geom_draw& Draw = m_DynamicGeom[i];
        Draw.pVertices          = &m_DynamicVertexStorage[m_DynamicVertexOffsets[i]];
        Draw.pIndices           = &m_DynamicIndexStorage[m_DynamicIndexOffsets[i]];

        const s32 DamageOffset = m_DynamicDamageOffsets[i];
        Draw.pDamageUpload     = ( DamageOffset >= 0 ) ? &m_DynamicDamageStorage[DamageOffset] : NULL;
    }
}

//=========================================================================

matrix4* RenderSubmission::StoreTransform( const matrix4& Transform )
{
    ValidateOpen( "StoreTransform" );

    if( m_TransformStorage.GetCount() >= m_TransformStorage.GetCapacity() )
    {
        return NULL;
    }

    m_TransformStorage.Append() = Transform;

    return &m_TransformStorage[m_TransformStorage.GetCount() - 1];
}

//=========================================================================

void RenderSubmission::Seal( void )
{
    ValidateOpen( "Seal" );
    RebindDynamicPointers();
    m_isSealed = TRUE;
}

//=========================================================================

void RenderSubmission::ValidateOpen( const char* pOperation ) const
{
    if( !IsOpen() )
    {
        RENDER_THROW( "RenderSubmission", "%s requires an open frame submission", pOperation );
    }
}

//=========================================================================

void RenderShadowSubmission::ClearDynamicDraws( void )
{
    m_DynamicShadowDraws.SetCount( 0 );
    m_DynamicVertexStorage.SetCount( 0 );
    m_DynamicIndexStorage.SetCount( 0 );
    m_DynamicDamageStorage.SetCount( 0 );
    m_DynamicVertexOffsets.SetCount( 0 );
    m_DynamicIndexOffsets.SetCount( 0 );
    m_DynamicDamageOffsets.SetCount( 0 );
}

//=========================================================================

void RenderShadowSubmission::RebindDynamicPointers( void )
{
    ASSERT( m_DynamicShadowDraws.GetCount() == m_DynamicVertexOffsets.GetCount() );
    ASSERT( m_DynamicShadowDraws.GetCount() == m_DynamicIndexOffsets.GetCount() );
    ASSERT( m_DynamicShadowDraws.GetCount() == m_DynamicDamageOffsets.GetCount() );

    for( s32 i = 0; i < m_DynamicShadowDraws.GetCount(); ++i )
    {
        dynamic_geom_shadow_draw& Draw = m_DynamicShadowDraws[i];
        Draw.pVertices                 = &m_DynamicVertexStorage[m_DynamicVertexOffsets[i]];
        Draw.pIndices                  = &m_DynamicIndexStorage[m_DynamicIndexOffsets[i]];

        const s32 DamageOffset = m_DynamicDamageOffsets[i];
        Draw.pDamageUpload     = ( DamageOffset >= 0 ) ? &m_DynamicDamageStorage[DamageOffset] : NULL;
    }
}

//=========================================================================
//  RENDER PIPELINE
//=========================================================================

RenderPipeline::RenderPipeline( void )
    : m_Resources(),
      m_Graph( m_Resources ),
      m_Features(),
      m_Backend(),
      m_LightSystem(),
      m_LightingTable(),
      m_ProjectionTextureRegistry(),
      m_ShadowMapAtlas(),
#ifndef X_RETAIL
      m_DebugOptions(),
#endif
      m_GeomFeature(),
      m_ClothFeature(),
      m_ShadowFeature(),
      m_PostProcessFeature(),
      m_Cmaa2Feature(),
      m_XeGtaoFeature(),
      m_OutputFeature( *this ),
      m_Frame(),
      m_Submission(),
      m_ShadowSubmission(),
      m_RenderScale( RenderQualityProfile::PlatformDefault().RenderScale ),
      m_nSamples( 1 ),
      m_nRequestedSamples( 1 ),
      m_isPhongShadingEnabled( FALSE ),
      m_isTessellationEnabled( FALSE ),
      m_Views(),
      m_CopyRequests(),
      m_FreeCopyRequests(),
      m_OutputHistory(),
      m_isOutputHistoryValid( FALSE ),
      m_OutputHistoryKey( 0 ),
      m_nOutputHistorySamples( 0 ),
      m_OutputHistoryScopeId( -2 ),
      m_OutputHistoryStates(),
      m_pOutputHistoryState( NULL ),
      m_PresentPixelShader(),
      m_TonemapPixelShader(),
      m_RegionCopyPixelShader(),
      m_isInitialized( FALSE ),
      m_isFrameValid( FALSE ),
      m_isFrameAborted( FALSE ),
      m_isFeatureFrameActive( FALSE ),
      m_isDeclared( FALSE ),
      m_isCompiled( FALSE ),
      m_isExecuted( FALSE )
{
    m_Features.Register( m_OutputFeature );
    m_Backend.ConfigureServices( m_LightSystem, m_LightingTable, m_ProjectionTextureRegistry, m_ShadowMapAtlas,
                                 m_ShadowFeature, m_PostProcessFeature, m_nSamples, m_isPhongShadingEnabled );
}

//=========================================================================

RenderPipeline::~RenderPipeline( void )
{
    Kill();
}

//=========================================================================

xbool RenderPipeline::Init( void )
{
    if( m_isInitialized )
    {
        return TRUE;
    }

    shader_SetCatalogId( A51ShaderCatalogId );
    m_Resources.Init();

    if( !shader_LoadComboStage( m_PresentPixelShader, A51FixedPrograms[A51FixedProgram_present].PackageFile,
                                SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_present].Key,
                                SHADER_STAGE_PIXEL ) )
    {
        m_Resources.Kill();
        RENDER_THROW( "RenderPipeline", "failed to load present shader" );
        return FALSE;
    }

    if( !shader_LoadComboStage( m_TonemapPixelShader, A51FixedPrograms[A51FixedProgram_scene_tonemap].PackageFile,
                                SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_scene_tonemap].Key,
                                SHADER_STAGE_PIXEL ) )
    {
        shader_Destroy( m_PresentPixelShader );
        shader_Destroy( m_TonemapPixelShader );
        m_Resources.Kill();
        RENDER_THROW( "RenderPipeline", "failed to load scene tonemap shader" );
        return FALSE;
    }

    if( !shader_LoadComboStage( m_RegionCopyPixelShader,
                                A51FixedPrograms[A51FixedProgram_scene_region_copy].PackageFile, SHADER_COMBO_FIXED,
                                A51FixedPrograms[A51FixedProgram_scene_region_copy].Key, SHADER_STAGE_PIXEL ) )
    {
        shader_Destroy( m_PresentPixelShader );
        shader_Destroy( m_TonemapPixelShader );
        shader_Destroy( m_RegionCopyPixelShader );
        m_Resources.Kill();
        RENDER_THROW( "RenderPipeline", "failed to load scene region copy shader" );
        return FALSE;
    }

    m_isInitialized = TRUE;
    x_DebugMsg( "RenderPipeline: initialized successfully\n" );

    return TRUE;
}

//=========================================================================

xbool RenderPipeline::QueueCommonPipelines( const RenderScopeDesc& Scope )
{
    if( !m_isInitialized )
    {
        return FALSE;
    }

    const rtarget_format OutputFormat =
        Scope.pColorTarget ? Scope.pColorTarget->Desc.Format : rtarget_GetBackBufferFormat();
    const u32 OutputSamples = Scope.pColorTarget ? Scope.pColorTarget->Desc.SampleCount : 1;

    if( OutputFormat == RTARGET_FORMAT_COUNT )
    {
        return FALSE;
    }

    const RenderFormatPolicy Formats = RenderQualityProfile::PlatformDefault().Formats;
    auto Queue = [this]( const shader* pPixelShader, rtarget_format Format, u32 Samples = 1 ) -> xbool
    {
        render_pipeline_desc Desc;
        return composite_GetPipelineDesc( Desc, COMPOSITE_BLEND_COPY, Format, RTARGET_FORMAT_COUNT, Samples,
                                          pPixelShader ) &&
               m_CommonPipelines.QueuePrewarm( Desc );
    };

    // Output and internal copies use the same composite VS as graph execution.
    if( !Queue( &m_PresentPixelShader, OutputFormat, OutputSamples ) || !Queue( NULL, OutputFormat, OutputSamples ) ||
        !Queue( NULL, Formats.SceneColorFormat ) || !Queue( NULL, RTARGET_FORMAT_RGBA8 ) ||
        !Queue( &m_RegionCopyPixelShader, Formats.SceneColorFormat ) ||
        !Queue( &m_RegionCopyPixelShader, RTARGET_FORMAT_RGBA8 ) )
    {
        return FALSE;
    }

    if( !Scope.EnablePostProcessing )
    {
        return TRUE;
    }

    if( Formats.SceneColorFormat == RTARGET_FORMAT_RGBA16F || Formats.SceneColorFormat == RTARGET_FORMAT_RGBA32F )
    {
        render_pipeline_desc Desc;
        const u32 Samples = Scope.SampleCount ? Scope.SampleCount : m_nSamples;

        if( !composite_GetPipelineDesc( Desc, COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB, Formats.SceneColorFormat,
                                        kSceneDepthFormat, Samples ) || !m_CommonPipelines.QueuePrewarm( Desc ) )
        {
            return FALSE;
        }
    }

    return Queue( &m_TonemapPixelShader, OutputFormat, OutputSamples ) &&
           Queue( &m_TonemapPixelShader, RTARGET_FORMAT_RGBA8 ) &&
           m_PostProcessFeature.QueuePipelines( m_CommonPipelines, Formats, OutputFormat, OutputSamples );
}

//=========================================================================

void RenderPipeline::Kill( void )
{
    AbortFrame();
    m_Backend.Kill();

    for( s32 i = 0; i < m_FreeCopyRequests.GetCount(); ++i )
    {
        delete m_FreeCopyRequests[i];
    }

    m_FreeCopyRequests.Clear();
    m_Graph.Reset();
    m_Resources.Kill();
    m_CommonPipelines.Reset();
    shader_Destroy( m_PresentPixelShader );
    shader_Destroy( m_TonemapPixelShader );
    shader_Destroy( m_RegionCopyPixelShader );
    m_Frame = RenderFrameContext();
    m_Views.Clear();
    m_OutputHistory         = RenderResourceHandle();
    m_isOutputHistoryValid  = FALSE;
    m_OutputHistoryKey      = 0;
    m_nOutputHistorySamples = 0;
    m_OutputHistoryScopeId  = -2;

    for( s32 i = 0; i < m_OutputHistoryStates.GetCount(); ++i )
    {
        delete m_OutputHistoryStates[i];
    }

    m_OutputHistoryStates.Clear();
    m_pOutputHistoryState = NULL;
    m_ShadowSubmission.Reset();
    m_isInitialized        = FALSE;
    m_isFrameValid         = FALSE;
    m_isFrameAborted       = FALSE;
    m_isFeatureFrameActive = FALSE;
}

//=========================================================================

xbool RenderPipeline::RegisterFeature( IRenderFeature& Feature )
{
    return m_Features.Register( Feature );
}

//=========================================================================

xbool RenderPipeline::UnregisterFeature( IRenderFeature& Feature )
{
    return m_Features.Unregister( Feature );
}

//=========================================================================
//  FRAME LIFECYCLE
//=========================================================================

void RenderPipeline::BeginEngineFrame( void )
{
    m_isFrameAborted = FALSE;

    if( !m_isInitialized )
    {
        return;
    }

    m_Resources.BeginEngineFrame( eng_GetCurrentFrameSlot(), eng_GetCurrentFrameSerial(), eng_GetFramesInFlight() );

    const u64 Serial = eng_GetCurrentFrameSerial();

    for( s32 i = m_OutputHistoryStates.GetCount() - 1; i >= 0; --i )
    {
        OutputHistoryState* pState = m_OutputHistoryStates[i];

        if( Serial >= pState->LastUsedSerial && Serial - pState->LastUsedSerial >= 180 )
        {
            if( m_pOutputHistoryState == pState )
            {
                m_pOutputHistoryState = NULL;
            }

            if( m_OutputHistoryKey == pState->HistoryKey )
            {
                m_isOutputHistoryValid = FALSE;
            }

            delete pState;
            m_OutputHistoryStates.Delete( i );
        }
    }
}

//=========================================================================

u32 RenderPipeline::GetFeatureSceneTextureRequirements( void ) const
{
    return m_Features.GetSceneTextureRequirements();
}

//=========================================================================

xbool RenderPipeline::BeginFeatureFrames( void )
{
    if( !m_isFrameValid )
    {
        RENDER_THROW( "RenderPipeline", "cannot begin feature frames without an active frame" );
        return FALSE;
    }

    m_isFeatureFrameActive = m_Features.BeginFrame( m_Frame );
    return m_isFeatureFrameActive;
}

//=========================================================================

xbool RenderPipeline::BeginFrame( const RenderFrameDesc& Desc )
{
    GraphicsPipelineCache::UpdatePrewarm();

    if( m_isFrameValid )
    {
        RENDER_THROW( "RenderPipeline", "cannot begin scope=%d while scope=%d is active", Desc.ScopeId,
                      m_Frame.Desc.ScopeId );
        return FALSE;
    }

    AbortFrame();
    m_isFrameAborted = FALSE;
    RecycleCopyRequests();
    m_Graph.Reset();
    m_Resources.BeginScope();
    m_Frame = RenderFrameContext();
    m_Views.SetCount( 0 );
    m_OutputHistory        = RenderResourceHandle();
    m_pOutputHistoryState  = NULL;
    m_isOutputHistoryValid = FALSE;
    m_isFrameValid         = FALSE;
    m_isFeatureFrameActive = FALSE;
    m_Submission.Begin();
    m_LightingTable.BeginScope();
    m_PreparedScene.Reset();
    m_isDeclared = FALSE;
    m_isCompiled = FALSE;
    m_isExecuted = FALSE;

    if( ( Desc.RenderScale < RENDER_SCALE_MIN ) || ( Desc.RenderScale > RENDER_SCALE_MAX ) ||
        ( ( Desc.SampleCount != 1 ) && ( Desc.SampleCount != 2 ) && ( Desc.SampleCount != 4 ) &&
          ( Desc.SampleCount != 8 ) ) ||
        ( Desc.Capabilities.MaxColorAttachments == 0 ) ||
        ( ( Desc.SampleCount > 1 ) && ( Desc.Capabilities.MaxColorAttachments < 2 ) ) ||
        ( Desc.Capabilities.MaxColorAttachments > RTARGET_MAX_TARGETS ) || !Desc.Views.pViews ||
        ( Desc.Views.ViewCount == 0 ) || ( Desc.Views.ViewMask == 0 ) )
    {
        RENDER_THROW( "RenderPipeline", "invalid frame description" );
        return FALSE;
    }

    if( !( Desc.SceneTextureRequirements & SCENE_TEXTURE_DEPTH ) )
    {
        RENDER_THROW( "RenderPipeline", "SceneDepth is mandatory for the forward renderer" );
        return FALSE;
    }

    if( Desc.Formats.SceneColorFormat == RTARGET_FORMAT_RGBA8 && Desc.Formats.BloomFormat == RTARGET_FORMAT_RGBA16F &&
        !Desc.Formats.BloomCompositeToLDR )
    {
        RENDER_THROW( "RenderPipeline", "HDR bloom into LDR SceneColor requires explicit clamp semantics" );
        return FALSE;
    }

    const u32 UnsupportedRequirements = Desc.SceneTextureRequirements & ~Desc.Capabilities.SupportedSceneTextures;

    if( UnsupportedRequirements )
    {
        RENDER_THROW( "RenderPipeline", "scene texture provider is unavailable flags=0x%x", UnsupportedRequirements );
        return FALSE;
    }

    if( !Desc.Capabilities.SupportsMultiview &&
        ( Desc.Views.ViewCount != 1 || Desc.Views.ViewMask != 1 || Desc.Views.pViews[0].ViewIndex != 0 ||
          Desc.Views.pViews[0].LayerIndex != 0 ) )
    {
        RENDER_THROW( "RenderPipeline", "layered and multiview rendering is not supported" );
        return FALSE;
    }

    u32 SeenViews = 0;

    for( u32 i = 0; i < Desc.Views.ViewCount; ++i )
    {
        const RenderView& FrameView = Desc.Views.pViews[i];

        if( ( FrameView.ViewIndex >= Desc.Views.ViewCount ) || ( FrameView.ViewIndex >= 32 ) ||
            !( Desc.Views.ViewMask & ( 1u << FrameView.ViewIndex ) ) || ( SeenViews & ( 1u << FrameView.ViewIndex ) ) ||
            ( FrameView.Viewport.Width <= 0.0f ) || ( FrameView.Viewport.Height <= 0.0f ) )
        {
            RENDER_THROW( "RenderPipeline", "invalid view family entry index=%u view=%u layer=%u mask=0x%08x", i,
                          FrameView.ViewIndex, FrameView.LayerIndex, Desc.Views.ViewMask );
            return FALSE;
        }

        SeenViews |= 1u << FrameView.ViewIndex;
    }

    m_Frame      = RenderFrameContext();
    m_Frame.Desc = Desc;
#ifndef X_RETAIL
    m_Frame.Desc.EnableShadows = Desc.EnableShadows && !m_DebugOptions.DisableShadowMaps;
#endif
    m_Views.SetCount( Desc.Views.ViewCount );

    for( u32 i = 0; i < Desc.Views.ViewCount; ++i )
    {
        m_Views[i] = Desc.Views.pViews[i];
    }

    m_Frame.Desc.Views.pViews = m_Views.GetPtr();

    for( u32 i = 0; i < m_Views.GetCount(); ++i )
    {
        if( m_Views[i].ViewId == 0 )
        {
            if( Desc.EnableTemporalHistory )
            {
                RENDER_THROW( "RenderPipeline", "temporal frame requires a stable ViewId index=%u scope=%d",
                              m_Views[i].ViewIndex, Desc.ScopeId );
                return FALSE;
            }

            // Non-temporal views may need a synthetic ViewId.
            m_Views[i].ViewId = static_cast<u64>( m_Views[i].ViewIndex + 1u );
        }

        if( Desc.EnableTemporalHistory )
        {
            for( u32 Previous = 0; Previous < i; ++Previous )
            {
                if( m_Views[Previous].ViewId == m_Views[i].ViewId )
                {
                    RENDER_THROW( "RenderPipeline", "temporal ViewId %llu is duplicated in scope=%d",
                                  static_cast<unsigned long long>( m_Views[i].ViewId ), Desc.ScopeId );
                    return FALSE;
                }
            }
        }
    }

    m_Graph.BeginFrame( m_Frame );

    RenderExtent   DisplayExtent = Desc.Output.IsValid() ? Desc.Output.Extent : RenderExtent();
    const rtarget* pOutputColor  = Desc.Output.IsValid() ? Desc.Output.pColor : rtarget_GetBackBuffer();

    if( !DisplayExtent.IsValid() && pOutputColor )
    {
        DisplayExtent = RenderExtent( pOutputColor->Desc.Width, pOutputColor->Desc.Height );
    }

    if( !DisplayExtent.IsValid() || !pOutputColor )
    {
        RENDER_THROW( "RenderPipeline", "render scope has no valid output" );
        return FALSE;
    }

    // Linear depth is resolved separately; hardware depth keeps the scene sample count.
    const RenderExtent SceneExtent( ScaleDimension( DisplayExtent.Width, Desc.RenderScale ),
                                    ScaleDimension( DisplayExtent.Height, Desc.RenderScale ) );
    const rtarget*     pOutputDepth = Desc.Output.IsValid() ? Desc.Output.pDepth : NULL;

    const xbool ReadHardwareDepth  = ( Desc.SceneTextureRequirements & SCENE_TEXTURE_HARDWARE_DEPTH ) != 0;
    const u32   HardwareDepthUsage = RENDER_RESOURCE_USAGE_DEPTH |
                                     ( ReadHardwareDepth ? RENDER_RESOURCE_USAGE_STORAGE_READ : 0 );

    if( pOutputDepth )
    {
        // Reject incompatible external depth instead of rendering with mismatched occlusion.
        if( !rtarget_HasDepthStencil( *pOutputDepth ) || ( pOutputDepth->Desc.Width != SceneExtent.Width ) ||
            ( pOutputDepth->Desc.Height != SceneExtent.Height ) ||
            ( pOutputDepth->Desc.SampleCount != Desc.SampleCount ) )
        {
            RENDER_THROW( "RenderPipeline", "external depth is incompatible with scene extent/sample" );
            return FALSE;
        }

        if( ReadHardwareDepth &&
            ( !( pOutputDepth->Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ ) ||
              !rtarget_HasShaderResource( *pOutputDepth ) ) )
        {
            RENDER_THROW( "RenderPipeline", "external depth does not support hardware depth reads" );
            return FALSE;
        }
    }

    // Allocate scene color and depth at the render scale.
    RenderResourceDesc Color;
    Color.Extent      = SceneExtent;
    Color.Format      = Desc.Formats.SceneColorFormat;
    Color.SampleCount = 1;
    Color.ArrayLayers = 1;
    Color.ViewMask    = 1;
    Color.Usage       = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC |
                  RENDER_RESOURCE_USAGE_COPY_DST;
    Color.pDebugName = "SceneColor";

    m_Frame.SceneColor     = m_Graph.CreateResource( Color );
    m_Frame.ScenePostColor = m_Frame.SceneColor;

    RenderResourceDesc Depth;
    Depth.Extent        = SceneExtent;
    Depth.SampleCount   = 1;
    Depth.ArrayLayers   = 1;
    Depth.ViewMask      = 1;
    Depth.ClearColor[0] = 1.0f;
    Depth.ClearColor[1] = 1.0f;
    Depth.ClearColor[2] = 1.0f;
    Depth.ClearColor[3] = 1.0f;

    if( Desc.SampleCount == 1 )
    {
        Depth.Format     = RTARGET_FORMAT_R32F;
        Depth.Usage      = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED;
        Depth.pDebugName = "SceneDepth";

        m_Frame.SceneDepth = m_Graph.CreateResource( Depth );

        if( pOutputDepth )
        {
            m_Frame.SceneDepthTarget = m_Graph.ImportExternal( *pOutputDepth, "ExternalSceneDepth" );
        }
        else
        {
            RenderResourceDesc DepthTarget = Depth;
            DepthTarget.Format             = kSceneDepthFormat;
            DepthTarget.Usage              = HardwareDepthUsage;
            DepthTarget.pDebugName         = "SceneDepthTarget";

            m_Frame.SceneDepthTarget = m_Graph.CreateResource( DepthTarget );
        }
    }
    else
    {
        // SDL GPU resolves color attachments, not multisample depth textures.
        Depth.Format     = RTARGET_FORMAT_R32F;
        Depth.Usage      = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED;
        Depth.pDebugName = "SceneDepth";

        m_Frame.SceneDepth = m_Graph.CreateResource( Depth );

        if( pOutputDepth )
        {
            m_Frame.SceneDepthTarget = m_Graph.ImportExternal( *pOutputDepth, "ExternalSceneDepthMSAA" );
        }
        else
        {
            const RenderResourceDesc MsaaDepth = [SceneExtent, HardwareDepthUsage, &Desc]()
            {
                RenderResourceDesc Result;
                Result.Extent      = SceneExtent;
                Result.Format      = kSceneDepthFormat;
                Result.SampleCount = Desc.SampleCount;
                Result.ArrayLayers = 1;
                Result.ViewMask    = 1;
                Result.Usage       = HardwareDepthUsage;
                Result.pDebugName  = "SceneDepthTarget";

                return Result;
            }();
            m_Frame.SceneDepthTarget = m_Graph.CreateResource( MsaaDepth );
        }

        RenderResourceDesc MsaaLinearDepth = Depth;
        MsaaLinearDepth.SampleCount        = Desc.SampleCount;
        MsaaLinearDepth.Usage              = RENDER_RESOURCE_USAGE_COLOR;
        if( Desc.SceneTextureRequirements & SCENE_TEXTURE_DEPTH_SAMPLES )
        {
            MsaaLinearDepth.Usage |= RENDER_RESOURCE_USAGE_STORAGE_READ;
        }
        MsaaLinearDepth.pDebugName         = "SceneDepthMSAA";

        m_Frame.SceneDepthMSAA = m_Graph.CreateResource( MsaaLinearDepth );

        RenderResourceDesc MsaaColor = Color;
        MsaaColor.SampleCount        = Desc.SampleCount;
        MsaaColor.Usage              = RENDER_RESOURCE_USAGE_COLOR;
        MsaaColor.pDebugName         = "SceneColorMSAA";

        m_Frame.SceneColorMSAA = m_Graph.CreateResource( MsaaColor );
    }

    if( Desc.SceneTextureRequirements & SCENE_TEXTURE_NORMALS )
    {
        RenderResourceDesc Normals;
        Normals.Extent      = SceneExtent;
        Normals.Format      = RTARGET_FORMAT_RGBA8;
        Normals.SampleCount = Desc.SampleCount;
        Normals.Usage       = RENDER_RESOURCE_USAGE_COLOR |
            ( Desc.SampleCount > 1 ? RENDER_RESOURCE_USAGE_STORAGE_READ : RENDER_RESOURCE_USAGE_SAMPLED );
        Normals.pDebugName = "SceneNormals";
        m_Frame.SceneNormals = m_Graph.CreateResource( Normals );
        if( !m_Frame.SceneNormals.IsValid() )
        {
            return FALSE;
        }
    }

    // The selected output extent may change between scopes.
    if( !m_Frame.Desc.Output.IsValid() )
    {
        m_Frame.Desc.Output.pColor      = pOutputColor;
        m_Frame.Desc.Output.ColorFormat = pOutputColor->Desc.Format;
        m_Frame.Desc.Output.Extent      = DisplayExtent;
        m_Frame.Desc.Output.SampleCount = pOutputColor->Desc.SampleCount;
        m_Frame.Desc.Output.ArrayLayers = 1;
        m_Frame.Desc.Output.ViewMask    = 1;
    }

    m_Frame.OutputColor =
        m_Graph.ImportExternal( *pOutputColor, Desc.Output.IsValid() ? "ExternalOutput" : "BackBuffer" );

    // Allocate the composition target at the output extent.
    RenderResourceDesc FinalColor;
    FinalColor.Extent      = DisplayExtent;
    FinalColor.Format      = pOutputColor->Desc.Format;
    FinalColor.SampleCount = pOutputColor->Desc.SampleCount;
    FinalColor.Usage = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC |
                       RENDER_RESOURCE_USAGE_COPY_DST;
    FinalColor.pDebugName = "FinalColor";

    m_Frame.FinalColor = m_Graph.CreateResource( FinalColor );

    if( m_Cmaa2Feature.IsEnabled() && Desc.EnablePostProcessing )
    {
        RenderResourceDesc AntiAliasingSource = FinalColor;
        AntiAliasingSource.Format             = RTARGET_FORMAT_RGBA8;
        AntiAliasingSource.SampleCount        = 1;
        AntiAliasingSource.Usage              = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED;
        AntiAliasingSource.pDebugName         = "Cmaa2Source";

        m_Frame.AntiAliasingSource = m_Graph.CreateResource( AntiAliasingSource );
    }

    if( Desc.EnableTemporalHistory && m_Frame.FinalColor.IsValid() &&
        !SetupOutputHistory( DisplayExtent, *pOutputColor, RenderHistoryKey( kOutputHistoryFeatureKey, m_Frame ),
                             Desc.ScopeId ) )
    {
        RENDER_THROW( "RenderPipeline", "failed to create output history resource" );
        m_Frame.FinalColor = RenderResourceHandle();
    }

    if( m_OutputHistory.IsValid() )
    {
        m_Frame.FinalColor = m_OutputHistory;
    }

    m_isFrameValid =
        m_Frame.SceneColor.IsValid() && m_Frame.SceneDepth.IsValid() && m_Frame.SceneDepthTarget.IsValid() &&
        m_Frame.FinalColor.IsValid() && m_Frame.OutputColor.IsValid() &&
        ( !m_Cmaa2Feature.IsEnabled() || !Desc.EnablePostProcessing || m_Frame.AntiAliasingSource.IsValid() ) &&
        ( Desc.SampleCount == 1 || ( m_Frame.SceneColorMSAA.IsValid() && m_Frame.SceneDepthMSAA.IsValid() ) );

    if( m_isFrameValid )
    {
        m_Graph.SetOutput( m_Frame.OutputColor );
    }

    if( !m_isFrameValid )
    {
        RENDER_THROW( "RenderPipeline",
                      "frame target allocation failed output=%d scale=%.3f samples=%u color=(%u,%u) depth=(%u,%u)",
                      Desc.Output.IsValid(), Desc.RenderScale, Desc.SampleCount, m_Frame.SceneColor.Index,
                      m_Frame.SceneColor.Generation, m_Frame.SceneDepth.Index, m_Frame.SceneDepth.Generation );
    }

    if( m_isFrameValid && !m_Backend.BeginFrame( eng_GetCurrentFrameSlot(), eng_GetCurrentFrameSerial() ) )
    {
        RENDER_THROW( "RenderPipeline", "backend frame storage is still in use" );
        AbortFrame();

        return FALSE;
    }

    return m_isFrameValid;
}

//=========================================================================

void RenderPipeline::AbortFrame( void )
{
    m_isFrameAborted = TRUE;
    m_Backend.AbortFrame();

    if( m_isFeatureFrameActive )
    {
        m_Features.AbortFrame();
    }

    RecycleCopyRequests();
    m_Graph.Reset();
    m_Submission.Reset();
    m_LightingTable.EndScope();
    m_PreparedScene.Reset();
    m_Frame = RenderFrameContext();
    m_Views.SetCount( 0 );
    m_OutputHistory        = RenderResourceHandle();
    m_isFrameValid         = FALSE;
    m_isFeatureFrameActive = FALSE;
    m_isDeclared           = FALSE;
    m_isCompiled           = FALSE;
    m_isExecuted           = FALSE;
}

//=========================================================================

xbool RenderPipeline::BeginUIOnlyFrame( void )
{
    if( m_isFrameValid )
    {
        return TRUE;
    }

    AbortFrame();
    m_isFrameAborted = FALSE;
    RecycleCopyRequests();
    m_Graph.Reset();
    m_Resources.BeginScope();
    m_Frame = RenderFrameContext();
    m_Views.SetCount( 0 );
    m_Submission.Begin();
    m_LightingTable.BeginScope();
    m_isFeatureFrameActive = FALSE;
    m_isDeclared = m_isCompiled = m_isExecuted = FALSE;

    const rtarget* pOutput = rtarget_GetBackBuffer();

    if( !pOutput )
    {
        RENDER_THROW( "RenderPipeline", "UI-only frame has no backbuffer" );
        return FALSE;
    }

    m_Views.SetCount( 1 );
    m_Views[0]                   = RenderView();
    m_Views[0].ViewIndex         = 0;
    m_Views[0].LayerIndex        = 0;
    m_Views[0].ViewId            = 1;
    m_Views[0].Viewport.TopLeftX = 0.0f;
    m_Views[0].Viewport.TopLeftY = 0.0f;
    m_Views[0].Viewport.Width    = static_cast<f32>( pOutput->Desc.Width );
    m_Views[0].Viewport.Height   = static_cast<f32>( pOutput->Desc.Height );

    m_Frame.Desc.Views.pViews          = m_Views.GetPtr();
    m_Frame.Desc.Views.ViewCount       = 1;
    m_Frame.Desc.Views.ViewMask        = 1;
    m_Frame.Desc.Output.pColor         = pOutput;
    m_Frame.Desc.Output.ColorFormat    = pOutput->Desc.Format;
    m_Frame.Desc.Output.Extent         = RenderExtent( pOutput->Desc.Width, pOutput->Desc.Height );
    m_Frame.Desc.Output.SampleCount    = pOutput->Desc.SampleCount;
    m_Frame.Desc.EnableTemporalHistory = TRUE;
    m_Graph.BeginFrame( m_Frame );
    m_PreparedScene.Reset();
    m_Frame.OutputColor = m_Graph.ImportExternal( *pOutput, "UIOnlyBackBuffer" );

    RenderResourceDesc FinalColor;
    FinalColor.Extent      = RenderExtent( pOutput->Desc.Width, pOutput->Desc.Height );
    FinalColor.Format      = pOutput->Desc.Format;
    FinalColor.SampleCount = pOutput->Desc.SampleCount;
    FinalColor.Usage = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC |
                       RENDER_RESOURCE_USAGE_COPY_DST;
    FinalColor.pDebugName = "UIOnlyColor";

    m_Frame.FinalColor = m_Graph.CreateResource( FinalColor );
    // UI-only is an overlay scope. It must continue the last completed engine
    // scope's history instead of creating a second history keyed to -1.
    m_Frame.Desc.ScopeId = m_OutputHistoryScopeId;

    if( m_Frame.FinalColor.IsValid() &&
        !SetupOutputHistory( m_Frame.Desc.Output.Extent, *pOutput,
                             m_OutputHistoryKey ? m_OutputHistoryKey
                                                : RenderHistoryKey( kOutputHistoryFeatureKey, m_Frame ),
                             m_OutputHistoryScopeId ) )
    {
        RENDER_THROW( "RenderPipeline", "failed to create UI output history resource" );
        m_Frame.FinalColor = RenderResourceHandle();
    }

    if( !m_OutputHistoryKey )
    {
        m_isOutputHistoryValid = FALSE;
    }

    if( m_OutputHistory.IsValid() )
    {
        m_Frame.FinalColor = m_OutputHistory;
    }

    m_isFrameValid = m_Frame.OutputColor.IsValid() && m_Frame.FinalColor.IsValid();

    if( m_isFrameValid )
    {
        m_isFrameValid = m_Backend.BeginReadOnlyFrame( eng_GetCurrentFrameSlot(), eng_GetCurrentFrameSerial() );

        if( !m_isFrameValid )
        {
            RENDER_THROW( "RenderPipeline", "backend frame storage is still in use for UI-only frame" );
            AbortFrame();

            return FALSE;
        }

        m_Graph.SetOutput( m_Frame.OutputColor );
        m_isFeatureFrameActive = m_Features.BeginFrame( m_Frame );

        if( !m_isFeatureFrameActive )
        {
            AbortFrame();
            return FALSE;
        }
    }

    return m_isFrameValid;
}

//=========================================================================
//  OUTPUT PASSES
//=========================================================================

xbool RenderPipeline::ExecutePresentPass( RenderPassContext& Context, RenderPipeline* pRenderer )
{
    const rtarget* pFinalColor = Context.Read( Context.GetFrame().FinalColor );

    if( !pFinalColor || !Context.Write( Context.GetFrame().OutputColor ) )
    {
        RENDER_THROW( "RenderPipeline", "Present has invalid source or external output" );
        return FALSE;
    }

    return composite_Blit( *pFinalColor, COMPOSITE_BLEND_COPY, 1.0f, &pRenderer->m_PresentPixelShader,
                           RSTATE_SAMPLER_PRESET_POINT_CLAMP, A51Resource_CompositeSource );
}

//=========================================================================

xbool RenderPipeline::ExecuteTonemapPass( RenderPassContext& Context, RenderPipeline* pRenderer )
{
    const rtarget*             pSceneColor = Context.Read( Context.GetPass().Source );
    const RenderResourceHandle Target =
        pRenderer && pRenderer->m_Cmaa2Feature.IsEnabled() && Context.GetFrame().Desc.EnablePostProcessing
            ? Context.GetFrame().AntiAliasingSource
            : Context.GetFrame().FinalColor;

    if( !pRenderer || !pSceneColor || !Context.Write( Target ) )
    {
        RENDER_THROW( "RenderPipeline", "tonemap pass has invalid scene or composition target" );
        return FALSE;
    }

    const SceneColorGradeParams& Grade = pRenderer->m_PostProcessFeature.GetSceneColorGrade();
    const xbool                  IsHDR = Context.GetFrame().Desc.Formats.SceneColorFormat == RTARGET_FORMAT_RGBA16F;
    const shader* pTonemap             = Context.GetFrame().Desc.EnablePostProcessing && ( IsHDR || !Grade.IsNeutral() )
                                             ? &pRenderer->m_TonemapPixelShader
                                             : NULL;

    if( pTonemap && !( *pTonemap ) )
    {
        RENDER_THROW( "RenderPipeline", "scene tonemap pass requires an unavailable shader" );
        return FALSE;
    }

    SceneColorGradeConstants Constants;

    if( pTonemap )
    {
        Constants.Adjustments.Set( Grade.Brightness, Grade.Contrast, 0.0f, 0.0f );

        if( !shader_PushUniformData( SHADER_STAGE_PIXEL, pTonemap->ResourceSlots[A51Resource_SceneColorGradeParams],
                                     &Constants, sizeof( Constants ) ) )
        {
            RENDER_THROW( "RenderPipeline", "scene color grade shader constants failed" );
            return FALSE;
        }
    }

    return composite_Blit( *pSceneColor, COMPOSITE_BLEND_COPY, 1.0f, pTonemap, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP,
                           pTonemap ? static_cast<u32>( A51Resource_SceneColor ) : 0xFFFFFFFFu );
}

//=========================================================================

xbool RenderPipeline::BuildPresentPass( void )
{
    if( !m_isFrameValid )
    {
        RENDER_THROW( "RenderPipeline", "cannot build Present pass without a valid frame" );
        return FALSE;
    }

    RenderPassDesc DescData;
    DescData.pDebugName  = "Present";
    DescData.Type        = RENDER_PASS_PRESENT;
    DescData.Color       = m_Frame.OutputColor;
    DescData.Source      = m_Frame.FinalColor;
    DescData.ColorLoadOp = RTARGET_LOAD_DONT_CARE;

    const RenderPassId PassData = m_Graph.AddPass( DescData, ExecutePresentTypedPass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        return FALSE;
    }

    return m_Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_PresentPixelShader );
}

//=========================================================================

xbool RenderPipeline::ExecutePresentTypedPass( RenderPassContext& Context, RenderPipeline* pRenderer )
{
    return ExecutePresentPass( Context, pRenderer );
}

//=========================================================================

xbool RenderPipeline::BuildTonemapPass( void )
{
    if( !m_isFrameValid )
    {
        RENDER_THROW( "RenderPipeline", "cannot build tonemap pass without a valid frame" );
        return FALSE;
    }

    const RenderResourceDesc*    pSource = m_Graph.GetDesc( m_Frame.ScenePostColor );
    const RenderResourceDesc*    pFinal  = m_Graph.GetDesc( m_Frame.FinalColor );
    const SceneColorGradeParams& Grade   = m_PostProcessFeature.GetSceneColorGrade();
    const xbool                  NeedsConversion =
        m_Frame.Desc.EnablePostProcessing &&
        ( m_Frame.Desc.Formats.SceneColorFormat == RTARGET_FORMAT_RGBA16F || !Grade.IsNeutral() ||
          m_Cmaa2Feature.IsEnabled() );

    if( pSource && pFinal && !NeedsConversion && pSource->Extent.Width == pFinal->Extent.Width &&
        pSource->Extent.Height == pFinal->Extent.Height && pSource->Format == pFinal->Format &&
        pSource->SampleCount == pFinal->SampleCount )
    {
        if( m_OutputHistory.IsValid() )
        {
            if( !m_Graph.RetainResource( m_Frame.ScenePostColor, m_pOutputHistoryState->HistoryKey ) )
            {
                return FALSE;
            }

            m_OutputHistory = m_Frame.ScenePostColor;
        }

        m_Frame.FinalColor = m_Frame.ScenePostColor;
        return TRUE;
    }

    RenderPassDesc Desc;
    Desc.pDebugName = "TonemapResolve";
    Desc.Source     = m_Frame.ScenePostColor;
    Desc.Color =
        m_Cmaa2Feature.IsEnabled() && m_Frame.Desc.EnablePostProcessing ? m_Frame.AntiAliasingSource : m_Frame.FinalColor;
    Desc.ColorLoadOp = RTARGET_LOAD_DONT_CARE;

    const RenderPassId PassData = m_Graph.AddPass( Desc, ExecuteTonemapPass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        return FALSE;
    }

    m_Graph.Read( PassData, Desc.Source );

    const xbool   IsHDR = m_Frame.Desc.Formats.SceneColorFormat == RTARGET_FORMAT_RGBA16F;
    const shader* pTonemap =
        m_Frame.Desc.EnablePostProcessing && ( IsHDR || !Grade.IsNeutral() ) ? &m_TonemapPixelShader : NULL;

    if( !m_Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, pTonemap ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool RenderPipeline::ExecuteClearPass( RenderPassContext& Context, RenderPipeline* pRenderer )
{
    ( void )pRenderer;
    return Context.Write( Context.GetFrame().FinalColor ) != NULL;
}

//=========================================================================

// Ending this pass triggers SDL GPU's StoreOp::RESOLVE.
xbool RenderPipeline::ExecuteAttachmentResolvePass( RenderPassContext& Context, RenderPipeline* pRenderer )
{
    ( void )pRenderer;
    return Context.Write( Context.GetPass().Color ) &&
           ( !Context.GetPass().ColorResolve.IsValid() || Context.Write( Context.GetPass().ColorResolve ) ) &&
           ( !Context.GetPass().Color1.IsValid() || Context.Write( Context.GetPass().Color1 ) ) &&
           ( !Context.GetPass().Color1Resolve.IsValid() || Context.Write( Context.GetPass().Color1Resolve ) );
}

//=========================================================================

xbool RenderPipeline::BuildClearPass( void )
{
    RenderPassDesc DescData;
    DescData.pDebugName  = "ClearOutput";
    DescData.Color       = m_Frame.FinalColor;
    DescData.ColorLoadOp = RTARGET_LOAD_CLEAR;

    return m_Graph.AddPass( DescData, ExecuteClearPass, this ) != INVALID_RENDER_PASS;
}

//=========================================================================
//  OUTPUT HISTORY
//=========================================================================

xbool RenderPipeline::SetupOutputHistory( const RenderExtent& Extent,
                                          const rtarget&      Output,
                                          u64                 HistoryKey,
                                          s32                 ScopeId )
{
    m_OutputHistory       = RenderResourceHandle();
    m_pOutputHistoryState = NULL;

    for( s32 i = 0; i < m_OutputHistoryStates.GetCount(); ++i )
    {
        if( m_OutputHistoryStates[i] && ( m_OutputHistoryStates[i]->HistoryKey == HistoryKey ) )
        {
            m_pOutputHistoryState = m_OutputHistoryStates[i];
            break;
        }
    }

    if( !m_pOutputHistoryState )
    {
        m_pOutputHistoryState = new OutputHistoryState;

        if( !m_pOutputHistoryState )
        {
            RENDER_THROW( "RenderPipeline", "failed to allocate output history state" );
            return FALSE;
        }

        m_pOutputHistoryState->HistoryKey = HistoryKey;
        m_pOutputHistoryState->ScopeId    = ScopeId;
        m_OutputHistoryStates.Append()    = m_pOutputHistoryState;
    }

    const xbool SameConfiguration = ( m_pOutputHistoryState->Extent.Width == Extent.Width ) &&
                                    ( m_pOutputHistoryState->Extent.Height == Extent.Height ) &&
                                    ( m_pOutputHistoryState->Format == Output.Desc.Format ) &&
                                    ( m_pOutputHistoryState->SampleCount == Output.Desc.SampleCount );

    if( !SameConfiguration || m_pOutputHistoryState->ResetToken != m_Frame.Desc.TemporalResetToken ||
        !m_Resources.HasPersistentResource( HistoryKey ) )
    {
        m_pOutputHistoryState->Valid = FALSE;
    }

    m_pOutputHistoryState->ResetToken     = m_Frame.Desc.TemporalResetToken;
    m_pOutputHistoryState->LastUsedSerial = eng_GetCurrentFrameSerial();

    m_nOutputHistorySamples            = Output.Desc.SampleCount;
    m_isOutputHistoryValid             = m_pOutputHistoryState->Valid;
    m_pOutputHistoryState->Extent      = Extent;
    m_pOutputHistoryState->Format      = Output.Desc.Format;
    m_pOutputHistoryState->SampleCount = Output.Desc.SampleCount;

    // Retained output history requires a single-sample texture.
    if( Output.Desc.SampleCount != 1 )
    {
        m_isOutputHistoryValid       = FALSE;
        m_pOutputHistoryState->Valid = FALSE;
        return TRUE;
    }

    RenderResourceDesc History;
    History.Extent      = Extent;
    History.Format      = Output.Desc.Format;
    History.SampleCount = 1;
    History.ArrayLayers = 1;
    History.ViewMask    = 1;
    History.Usage       = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC |
                    RENDER_RESOURCE_USAGE_COPY_DST;
    History.Lifetime         = RENDER_RESOURCE_PERSISTENT;
    History.PersistentKey    = HistoryKey;
    History.PreserveContents = TRUE;
    History.pDebugName       = "OutputHistory";
    m_OutputHistory          = m_Graph.CreateResource( History );

    return m_OutputHistory.IsValid();
}

//=========================================================================

xbool RenderPipeline::DeclareSceneDepthResolve( const RenderFrameContext& Frame )
{
    if( !Frame.SceneDepthMSAA.IsValid() )
    {
        return TRUE;
    }

    RenderPassDesc DescData;
    DescData.pDebugName   = "ResolveSceneDepth";
    DescData.Color        = Frame.SceneDepthMSAA;
    DescData.ColorResolve = Frame.SceneDepth;
    DescData.ColorLoadOp  = RTARGET_LOAD_LOAD;
    DescData.ColorStoreOp = RTARGET_STORE_RESOLVE_AND_STORE;

    return m_Graph.AddPass( DescData, ExecuteAttachmentResolvePass, this ) != INVALID_RENDER_PASS;
}

//=========================================================================

xbool RenderPipeline::BuildSceneColorResolvePass( xbool PreserveMSAA )
{
    if( !m_Frame.SceneColorMSAA.IsValid() )
    {
        return TRUE;
    }

    RenderPassDesc DescData;
    DescData.pDebugName   = "ResolveSceneColor";
    DescData.Color        = m_Frame.SceneColorMSAA;
    DescData.ColorResolve = m_Frame.SceneColor;
    DescData.ColorLoadOp  = RTARGET_LOAD_LOAD;
    DescData.ColorStoreOp = PreserveMSAA ? RTARGET_STORE_RESOLVE_AND_STORE : RTARGET_STORE_RESOLVE;

    const RenderPassId PassData = m_Graph.AddPass( DescData, ExecuteAttachmentResolvePass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "RenderPipeline", "failed to declare SceneColor resolve pass" );
        return FALSE;
    }

    return m_Graph.GetLastError() == NULL;
}

//=========================================================================

RenderSubmission& RenderPipeline::GetSubmission( void )
{
    if( !m_isFrameValid )
    {
        RENDER_THROW( "RenderPipeline", "frame submission requested without an active frame" );
    }

    return m_Submission;
}

//=========================================================================

const RenderSubmission& RenderPipeline::GetSubmission( void ) const
{
    return m_Submission;
}

//=========================================================================
//  FRAME EXECUTION
//=========================================================================

xbool RenderPipeline::DeclareFrame( void )
{
    if( !m_isFrameValid || m_isDeclared || m_isCompiled || m_isExecuted )
    {
        RENDER_THROW( "RenderPipeline", "invalid DeclareFrame lifecycle scope=%d", m_Frame.Desc.ScopeId );
        return FALSE;
    }

    if( m_Submission.IsOpen() )
    {
        m_Submission.Seal();
    }

    // All submission sources close before any feature prepares GPU data.
    if( m_Backend.GetPrimitiveEncoder().IsSubmissionActive() && !m_Backend.GetPrimitiveEncoder().EndSubmission() )
    {
        return FALSE;
    }

    if( m_Backend.GetDecalEncoder().IsSubmissionOpen() && !m_Backend.GetDecalEncoder().EndSubmission() )
    {
        return FALSE;
    }

    if( !m_isFeatureFrameActive )
    {
        return FALSE;
    }

    if( !m_Backend.SealAndPrepare( m_Submission.GetGeom(), m_Submission.GetDynamicGeom(),
                                   m_Submission.GetDynamicStorage(), m_Submission.GetEnvironmentCubeMap(),
                                   m_Submission.IsSceneSubmitted(), m_Frame ) )
    {
        return FALSE;
    }

    RenderFeaturePrepareContext Context( m_Frame, m_Submission, m_PreparedScene );

    if( !m_Features.Prepare( Context ) )
    {
        return FALSE;
    }

    // Publish scene preparation before declaration so features share its state.
    m_Frame.SceneRendered = m_Submission.IsSceneSubmitted() && m_PreparedScene.IsScenePrepared();

    if( !m_Features.BuildGraph( m_Graph, m_Frame ) )
    {
        return FALSE;
    }

    m_isDeclared = TRUE;

    return TRUE;
}

//=========================================================================

xbool RenderPipeline::CompileFrame( void )
{
    if( !m_isDeclared || m_isCompiled || m_isExecuted || !m_Graph.Compile() )
    {
        RENDER_THROW( "RenderPipeline", "frame compile failed scope=%d: %s", m_Frame.Desc.ScopeId,
                      m_Graph.GetLastError() ? m_Graph.GetLastError() : "invalid lifecycle" );
        return FALSE;
    }

    m_isCompiled = TRUE;
#ifndef X_RETAIL
    if( m_DebugOptions.DumpFrameGraph )
    {
        m_Graph.Dump();
    }
#endif
    return TRUE;
}

//=========================================================================

xbool RenderPipeline::ExecuteFrame( void )
{
    if( m_isCompiled && !m_isExecuted && m_pOutputHistoryState )
    {
        m_pOutputHistoryState->Valid = FALSE;
    }

    if( !m_isCompiled || m_isExecuted || !m_Graph.Execute() )
    {
        RENDER_THROW( "RenderPipeline", "frame execution failed scope=%d: %s", m_Frame.Desc.ScopeId,
                      m_Graph.GetLastError() ? m_Graph.GetLastError() : "invalid lifecycle" );
        return FALSE;
    }

    m_isExecuted = TRUE;
#ifndef X_RETAIL
    if( m_DebugOptions.ShowLightingStats && m_Frame.Desc.Purpose == RENDER_VIEW_GAMEPLAY &&
        m_Frame.SceneDepth.IsValid() )
    {
        CaptureLightingDebugStats();
    }
#endif
    return TRUE;
}

//=========================================================================

#ifndef X_RETAIL
//=========================================================================

void RenderPipeline::CaptureLightingDebugStats( void )
{
    lighting_debug_stats Stats;
    Stats.Valid         = TRUE;
    Stats.FrameSerial   = eng_GetCurrentFrameSerial();
    Stats.DynamicLights = m_LightSystem.GetNDynamicLights();
    Stats.CharLights    = m_LightSystem.GetNCharLights();
    Stats.FadingLights =
        m_DebugOptions.DisableLocalLights || m_DebugOptions.DisableFadingLights ? 0 : m_LightSystem.GetNFadingLights();
    Stats.Cookies    = m_LightSystem.GetLightCookieCount();
    Stats.Collection = m_LightSystem.GetCollectionStats();

    for( s32 i = 0; i < m_LightSystem.GetNDynamicLights(); ++i )
    {
        vector3 Position, Direction;
        xcolor  Color;
        f32     Radius, InnerRadius, Falloff, InnerAngle, OuterAngle;
        s32     Shape, Resolution, Priority;
        xbool   CastShadows, Flashlight;
        m_LightSystem.GetDynamicLight( i, Position, Radius, Color, Shape, CastShadows, InnerRadius, Direction, Falloff,
                                       InnerAngle, OuterAngle, Resolution, Priority, Flashlight );

        if( Shape == LightSystem::LIGHT_SHAPE_SPOT )
        {
            ++Stats.SpotLights;
        }
        else
        {
            ++Stats.OmniLights;
        }

        if( CastShadows )
        {
            ++Stats.ShadowRequests;
        }
    }

    Stats.LightingSets = m_LightingTable.GetCount();

    for( s32 i = 0; i < m_LightingTable.GetCount(); ++i )
    {
        const u32 Count = static_cast<u32>( x_clamp( m_LightingTable.GetAt( i ).LightCount, 0, MAX_GEOM_LIGHTS ) );
        Stats.AssignedLights += Count;
        Stats.MaxAssignedLights = MAX( Stats.MaxAssignedLights, Count );
        ++Stats.LightHistogram[Count];
    }

    Stats.ProjectedLights  = m_ProjectionTextureRegistry.GetProjLightCount();
    Stats.ProjectedShadows = m_ProjectionTextureRegistry.GetProjShadowCount();
    Stats.Mode             = m_ShadowMapAtlas.GetShadowMode();
    Stats.Filter           = m_ShadowMapAtlas.GetShadowFilterType();
    Stats.ShadowsEnabled   = m_Frame.Desc.EnableShadows && Stats.Mode != ShadowMode::Disabled;
    Stats.AtlasSize        = m_ShadowMapAtlas.GetAtlasSize();

    u64 ActiveTexels = 0;

    if( Stats.ShadowsEnabled )
    {
        Stats.Casters = m_ShadowFeature.Backend().GetLastCasterStats();

        for( s32 i = 0; i < m_ShadowMapAtlas.GetSourceCount(); ++i )
        {
            const ShadowMapAtlas::ShadowSource& Source = m_ShadowMapAtlas.GetSource( i );

            if( Source.Type == ShadowMapAtlas::SHADOW_SOURCE_POINT_FACE )
            {
                ++Stats.PointShadowFaces;

                if( Source.FaceIndex == 0 )
                {
                    ++Stats.PointShadowLights;
                }
            }
            else
            {
                ++Stats.SpotShadowSources;
            }

            ActiveTexels += static_cast<u64>( Source.AtlasWidth ) * Source.AtlasHeight;
        }
    }

    if( Stats.AtlasSize )
    {
        Stats.ActiveAtlasArea =
            100.0f * static_cast<f32>( ActiveTexels ) / ( static_cast<f32>( Stats.AtlasSize ) * Stats.AtlasSize );
    }

    m_LightingDebugStats = Stats;
}
#endif

//=========================================================================

xbool RenderPipeline::EndFrame( void )
{
    if( !m_isExecuted )
    {
        RENDER_THROW( "RenderPipeline", "EndFrame called before ExecuteFrame scope=%d", m_Frame.Desc.ScopeId );
        return FALSE;
    }

    m_Features.EndFrame();

    if( !m_Backend.EndFrame() )
    {
        RENDER_THROW( "RenderPipeline", "backend frame storage failed to retire frame" );
        AbortFrame();

        return FALSE;
    }

    if( m_Frame.Desc.EnableTemporalHistory )
    {
        m_isOutputHistoryValid = m_OutputHistory.IsValid() && ( m_nOutputHistorySamples == 1 );

        if( m_pOutputHistoryState )
        {
            m_pOutputHistoryState->Valid = m_isOutputHistoryValid;
            m_OutputHistoryKey           = m_isOutputHistoryValid ? m_pOutputHistoryState->HistoryKey : 0;
            m_OutputHistoryScopeId       = m_pOutputHistoryState->ScopeId;
        }
    }
    else
    {
        m_isOutputHistoryValid = FALSE;
        m_OutputHistoryKey     = 0;
        m_pOutputHistoryState  = NULL;
    }

    m_isFrameValid         = FALSE;
    m_isFeatureFrameActive = FALSE;
    m_Submission.Reset();
    m_PreparedScene.Reset();
    m_LightingTable.EndScope();

    return TRUE;
}

//=========================================================================

xbool RenderPipeline::FinishRenderScope( void )
{
    if( !DeclareFrame() || !CompileFrame() || !ExecuteFrame() || !EndFrame() )
    {
        AbortFrame();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  SCENE COLOR CAPTURE
//=========================================================================

xbool RenderPipeline::CopySceneColorRegion( rtarget& Destination, s32 SourceX, s32 SourceY, s32 Width, s32 Height )
{
    if( !m_isFrameValid || !rtarget_IsValid( Destination ) || !rtarget_HasRenderTarget( Destination ) ||
        ( Destination.Desc.SampleCount != 1 ) || ( Width <= 0 ) || ( Height <= 0 ) )
    {
        RENDER_THROW( "RenderPipeline", "invalid SceneColor copy request source=(%d,%d) size=%dx%d", SourceX, SourceY,
                      Width, Height );
        return FALSE;
    }

    const RenderExtent OutputExtent = m_Frame.Desc.Output.Extent;

    if( !OutputExtent.IsValid() )
    {
        RENDER_THROW( "RenderPipeline", "SceneColor copy request has no output extent" );
        return FALSE;
    }

    // Convert output-pixel bounds to normalized scene coordinates.
    const f32 OutputWidth  = static_cast<f32>( OutputExtent.Width );
    const f32 OutputHeight = static_cast<f32>( OutputExtent.Height );
    const f32 Left         = x_clamp( static_cast<f32>( SourceX ), 0.0f, OutputWidth );
    const f32 Top          = x_clamp( static_cast<f32>( SourceY ), 0.0f, OutputHeight );
    const f32 Right        = x_clamp( static_cast<f32>( SourceX ) + static_cast<f32>( Width ), 0.0f, OutputWidth );
    const f32 Bottom       = x_clamp( static_cast<f32>( SourceY ) + static_cast<f32>( Height ), 0.0f, OutputHeight );

    if( ( Right <= Left ) || ( Bottom <= Top ) )
    {
        RENDER_THROW( "RenderPipeline", "SceneColor copy request is outside output bounds" );
        return FALSE;
    }

    CopyRegionRequest* pRequest = NULL;

    if( m_FreeCopyRequests.GetCount() > 0 )
    {
        const s32 FreeIndex = m_FreeCopyRequests.GetCount() - 1;
        pRequest            = m_FreeCopyRequests[FreeIndex];
        m_FreeCopyRequests.SetCount( FreeIndex );
    }
    else
    {
        pRequest = new CopyRegionRequest;
    }

    if( !pRequest )
    {
        RENDER_THROW( "RenderPipeline", "failed to allocate SceneColor copy request" );
        return FALSE;
    }

    pRequest->pRenderer = this;
    pRequest->Destination =
        m_Graph.ImportExternalMutable( Destination, "SceneColorRegionOutput", RENDER_RESOURCE_USAGE_COPY_DST );

    if( !pRequest->Destination.IsValid() )
    {
        RENDER_THROW( "RenderPipeline", "failed to import SceneColor copy destination" );
        m_FreeCopyRequests.Append() = pRequest;

        return FALSE;
    }

    pRequest->SourceU      = Left / OutputWidth;
    pRequest->SourceV      = Top / OutputHeight;
    pRequest->SourceWidth  = ( Right - Left ) / OutputWidth;
    pRequest->SourceHeight = ( Bottom - Top ) / OutputHeight;

    m_CopyRequests.Append()           = pRequest;
    m_Frame.SceneColorCopies.Append() = pRequest->Destination;

    return TRUE;
}

//=========================================================================

void RenderPipeline::RecycleCopyRequests( void )
{
    for( s32 i = 0; i < m_CopyRequests.GetCount(); ++i )
    {
        m_FreeCopyRequests.Append() = m_CopyRequests[i];
    }

    m_CopyRequests.SetCount( 0 );
}

//=========================================================================

xbool RenderPipeline::DeclareCopyPasses( void )
{
    for( s32 i = 0; i < m_CopyRequests.GetCount(); ++i )
    {
        CopyRegionRequest* pRequest = m_CopyRequests[i];
        RenderPassDesc     Desc;
        Desc.pDebugName = "ScopeCapture";

        pRequest->Source = m_Frame.SceneColor;

        const RenderResourceDesc* pSource      = m_Graph.GetDesc( pRequest->Source );
        const RenderResourceDesc* pDestination = m_Graph.GetDesc( pRequest->Destination );

        if( !pSource || !pDestination )
        {
            return FALSE;
        }

        pRequest->SourceX    = static_cast<u32>( pRequest->SourceU * pSource->Extent.Width + 0.5f );
        pRequest->SourceY    = static_cast<u32>( pRequest->SourceV * pSource->Extent.Height + 0.5f );
        pRequest->CopyWidth  = MAX( 1u, static_cast<u32>( pRequest->SourceWidth * pSource->Extent.Width + 0.5f ) );
        pRequest->CopyHeight = MAX( 1u, static_cast<u32>( pRequest->SourceHeight * pSource->Extent.Height + 0.5f ) );

        const xbool DirectCopy =
            ( pSource->SampleCount == 1 ) && ( pDestination->SampleCount == 1 ) &&
            ( pSource->Format == pDestination->Format ) && ( pRequest->CopyWidth == pDestination->Extent.Width ) &&
            ( pRequest->CopyHeight == pDestination->Extent.Height ) && ( pRequest->SourceX <= pSource->Extent.Width ) &&
            ( pRequest->CopyWidth <= pSource->Extent.Width - pRequest->SourceX ) &&
            ( pRequest->SourceY <= pSource->Extent.Height ) &&
            ( pRequest->CopyHeight <= pSource->Extent.Height - pRequest->SourceY );

        if( DirectCopy )
        {
            Desc.Type        = RENDER_PASS_COPY;
            Desc.Source      = pRequest->Source;
            Desc.Destination = pRequest->Destination;
        }
        else
        {
            Desc.Color = pRequest->Destination;
        }

        const RenderPassId PassData = m_Graph.AddPass( Desc, ExecuteCopyRegionPass, pRequest );

        if( PassData == INVALID_RENDER_PASS )
        {
            RENDER_THROW( "RenderPipeline", "failed to declare ScopeCapture" );
            return FALSE;
        }

        if( DirectCopy )
        {
            continue;
        }

        m_Graph.Read( PassData, pRequest->Source );

        if( !m_Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_RegionCopyPixelShader ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool RenderPipeline::ExecuteCopyRegionPass( RenderPassContext& Context, CopyRegionRequest* pRequest )
{
    static xprofile_counter SceneColorRegionDirectCopies =
        x_GetProfiler().RegisterCounter( "SceneColorRegionDirectCopies", "RenderCounter" );
    static xprofile_counter SceneColorRegionCompositeCopies =
        x_GetProfiler().RegisterCounter( "SceneColorRegionCompositeCopies", "RenderCounter" );

    if( !pRequest )
    {
        RENDER_THROW( "RenderPipeline", "copy pass received a null request" );
        return FALSE;
    }

    if( Context.GetPass().Type == RENDER_PASS_COPY )
    {
        if( !Context.CopyRegion( pRequest->Destination, pRequest->Source, pRequest->SourceX, pRequest->SourceY,
                                 pRequest->CopyWidth, pRequest->CopyHeight ) )
        {
            return FALSE;
        }

        SceneColorRegionDirectCopies.Add();
        return TRUE;
    }

    rtarget*       pDestination = Context.WriteMutable( pRequest->Destination );
    const rtarget* pSource      = Context.Read( pRequest->Source );

    if( !pRequest->pRenderer || !pDestination || !pSource || !rtarget_HasShaderResource( *pSource ) )
    {
        RENDER_THROW( "RenderPipeline", "copy pass resources are unavailable" );
        return FALSE;
    }

    SceneRegionCopyConstants Constants;
    Constants.SourceUV.Set( pRequest->SourceU, pRequest->SourceV, pRequest->SourceWidth, pRequest->SourceHeight );

    if( !shader_PushUniformData(
            SHADER_STAGE_PIXEL,
            pRequest->pRenderer->m_RegionCopyPixelShader.ResourceSlots[A51Resource_SceneRegionParams], &Constants,
            sizeof( Constants ) ) )
    {
        RENDER_THROW( "RenderPipeline", "SceneColor region shader constants failed" );
        return FALSE;
    }

    const xbool Result =
        composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &pRequest->pRenderer->m_RegionCopyPixelShader,
                        RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_SceneColorSource );

    if( Result )
    {
        SceneColorRegionCompositeCopies.Add();
    }

    return Result;
}

//=========================================================================
} // namespace render
