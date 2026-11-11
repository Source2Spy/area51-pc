//=========================================================================
//
//  PostProcessFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PostProcessFeature.hpp"
#include "a51_shader_contract.hpp"
#include "../../Pipeline/RenderPipeline.hpp"

namespace render
{

//=========================================================================
namespace
{
static const u64 kFilterHistoryFeatureKey  = 0x504f535448495354ull;
static const s32 kFogGeneratedPaletteIndex = 2;
} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

PostProcessFeature::PostProcessFeature( void )
    : m_isInitialized( FALSE ),
      m_Bloom(),
      m_isRequestsSealed( FALSE ),
      m_hasMotionBlur( FALSE ),
      m_hasForcedGlow( FALSE ),
      m_FrameDeltaTime( 0.0f ),
      m_PostViewL( 0 ),
      m_PostViewT( 0 ),
      m_PostViewR( 0 ),
      m_PostViewB( 0 ),
      m_PostNearZ( 1.0f ),
      m_PostFarZ( 2.0f ),
      m_pMipTexture( NULL ),
      m_isXboxFogEnabled( FALSE ),
      m_isFogValid(),
      m_FogSourcePalette(),
      m_FogPalette(),
      m_FogColor(),
      m_FogConst(),
      m_FogStart(),
      m_GeneratedFogNearZ( 0.0f ),
      m_GeneratedFogFarZ( 0.0f ),
      m_isGeneratedFogValid( FALSE ),
      m_GlowHistoryRetention( 0.0f ),
      m_GlowCurrentWeight( 1.0f ),
      m_shouldStoreGlowHistory( FALSE ),
      m_nFilterMips( 0 ),
      m_FilterHistoryKey( 0 ),
      m_GlowHistoryKey( 0 )
{
}

//=========================================================================
//  FEATURE LIFETIME
//=========================================================================

xbool PostProcessFeature::Init( void )
{
    if( m_isInitialized )
    {
        return TRUE;
    }

    x_try;

        x_DebugMsg( "PostProcessFeature: Initializing post-processing feature\n" );

        m_isRequestsSealed    = FALSE;
        m_isGeneratedFogValid = FALSE;
        m_hasMotionBlur       = FALSE;
        m_hasForcedGlow       = FALSE;
        m_PostViewL = m_PostViewT = m_PostViewR = m_PostViewB = 0;
        m_PostNearZ                                           = 1.0f;
        m_PostFarZ                                            = 2.0f;
        m_pMipTexture                                         = NULL;
        m_FrameDeltaTime                                      = 0.0f;
        m_FilterHistoryHandle                                 = RenderResourceHandle();
        m_FilterHistoryKey                                    = 0;
        m_GlowHistoryKey                                      = 0;
        m_TemporalHistories.Clear();
        m_nFilterMips            = 0;
        m_GlowHistoryRetention   = 0.0f;
        m_GlowCurrentWeight      = 1.0f;
        m_shouldStoreGlowHistory = FALSE;

        for( s32 i = 0; i < 5; i++ )
        {
            m_isFogValid[i] = FALSE;
            x_memset( m_FogSourcePalette[i], 0, sizeof( m_FogSourcePalette[i] ) );
            x_memset( m_FogPalette[i], 0, sizeof( m_FogPalette[i] ) );
            m_FogColor[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
            m_FogConst[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
            m_FogStart[i] = 0.0f;
        }

        m_Requests   = PostChainRequests();
        m_MotionBlur = PostMotionBlurParams();
        m_Glow       = PostGlowParams();
        m_RadialBlur = PostRadialBlurParams();
        m_ScreenWarp = PostScreenWarpParams();
        m_PreparedWarpViews.SetCount( 0 );
        m_FogFilter       = PostFogFilterParams();
        m_MipFilter       = PostMipFilterParams();
        m_PainFilter      = PostMipFilterParams();
        m_SceneColorGrade = SceneColorGradeParams();
        m_Simple          = PostSimpleParams();

        m_FogResources = FogResources();

        const xbool FogInitialized = m_FogResources.Initialize();
        m_GlowResources            = GlowResources();

        const xbool GlowInitialized = m_GlowResources.Initialize();
        m_FilterResources           = FilterResources();

        const xbool FilterInitialized = m_FilterResources.Initialize();
        xbool       isInitialized     = FogInitialized && GlowInitialized && FilterInitialized;

        if( !isInitialized )
        {
            Kill();
            RENDER_THROW( "PostProcessFeature", "initialization failed; post-processing is unavailable" );
            return FALSE;
        }

        if( !m_Bloom.Init() )
        {
            Kill();
            RENDER_THROW( "PostProcessFeature", "bloom stage initialization failed" );
            return FALSE;
        }

        m_isInitialized = TRUE;
        x_DebugMsg( "PostProcessFeature: post-processing feature initialized successfully\n" );

        return TRUE;

    x_catch_begin;

        Kill();

    x_catch_end_ret;
}

//=========================================================================

xbool PostProcessFeature::QueuePipelines( GraphicsPipelineCache&    Cache,
                                          const RenderFormatPolicy& Formats,
                                          rtarget_format            OutputFormat,
                                          u32                       OutputSamples )
{
    if( !m_isInitialized )
    {
        return FALSE;
    }

    auto Queue = [&Cache]( const shader& PixelShader, composite_blend_mode Blend, rtarget_format Format,
                           u32 Samples = 1 ) -> xbool
    {
        render_pipeline_desc Desc;
        return composite_GetPipelineDesc( Desc, Blend, Format, RTARGET_FORMAT_COUNT, Samples, &PixelShader ) &&
               Cache.QueuePrewarm( Desc );
    };

    struct Requirement
    {
        const shader*        pPixelShader;
        composite_blend_mode Blend;
    };

    const Requirement Scene[] = {
        { &m_FogResources.CompositePS, COMPOSITE_BLEND_ALPHA_RGB },
        { &m_FilterResources.MotionBlurPS, COMPOSITE_BLEND_ALPHA_RGB },
        { &m_FilterResources.MipReducePS, COMPOSITE_BLEND_COPY },
        { &m_FilterResources.MipDownsamplePS, COMPOSITE_BLEND_COPY },
        { &m_FilterResources.PainBlurPS, COMPOSITE_BLEND_COPY },
        { &m_FilterResources.PainBlurPS, COMPOSITE_BLEND_COPY_RGB },
        { &m_FilterResources.MipCompositePS, COMPOSITE_BLEND_ALPHA_RGB },
        { &m_FilterResources.MipCompositeCustomPS, COMPOSITE_BLEND_ALPHA_RGB },
        { &m_FilterResources.RadialBlurPS, COMPOSITE_BLEND_ALPHA },
        { &m_FilterResources.ScreenWarpPS, COMPOSITE_BLEND_COPY },
        { &m_GlowResources.CompositePS, COMPOSITE_BLEND_ADDITIVE_RGB },
    };

    for( const Requirement& R : Scene )
    {
        if( !Queue( *R.pPixelShader, R.Blend, Formats.SceneColorFormat ) )
        {
            return FALSE;
        }
    }

    const shader* Bloom[] = { &m_GlowResources.PrefilterPS, &m_GlowResources.DownsamplePS, &m_GlowResources.BlurHPS,
                              &m_GlowResources.BlurVPS, &m_GlowResources.CombinePS };

    for( const shader* pPixelShader : Bloom )
    {
        if( !Queue( *pPixelShader, COMPOSITE_BLEND_COPY, Formats.BloomFormat ) )
        {
            return FALSE;
        }
    }

    const rtarget_format MaskFormats[] = { RTARGET_FORMAT_R8, RTARGET_FORMAT_RGBA8 };

    for( rtarget_format Format : MaskFormats )
    {
        if( !Queue( m_FilterResources.WarpMaskPS, COMPOSITE_BLEND_COPY, Format ) ||
            !Queue( m_FilterResources.WarpMaskBlurPS, COMPOSITE_BLEND_COPY, Format ) )
        {
            return FALSE;
        }
    }

    return Queue( m_FilterResources.NoisePS, COMPOSITE_BLEND_ALPHA_RGB, OutputFormat, OutputSamples ) &&
           Queue( m_FilterResources.ScreenFadePS, COMPOSITE_BLEND_ALPHA, OutputFormat, OutputSamples );
}

//=========================================================================

void PostProcessFeature::Kill( void )
{
    const xbool WasInitialized = m_isInitialized;

    if( WasInitialized )
    {
        x_DebugMsg( "PostProcessFeature: Shutting down post-processing feature\n" );
    }

    m_FogResources.Shutdown();
    m_GlowResources.Shutdown();
    m_FilterResources.Shutdown();
    m_TemporalHistories.Clear();
    m_Bloom.Kill();

    m_isInitialized    = FALSE;
    m_isRequestsSealed = FALSE;

    if( WasInitialized )
    {
        x_DebugMsg( "PostProcessFeature: post-processing feature shutdown complete\n" );
    }
}

//=========================================================================

void PostProcessFeature::Update( f32 DeltaTime )
{
    m_FrameDeltaTime = x_clamp( DeltaTime, 0.0f, 0.1f );
}

//=========================================================================

void PostProcessFeature::SetSceneColorGrade( const SceneColorGradeParams& Params )
{
    if( !m_isInitialized )
    {
        RENDER_THROW( "PostProcessFeature", "SetSceneColorGrade called before initialization" );
        return;
    }

    m_SceneColorGrade            = Params;
    m_SceneColorGrade.Brightness = x_clamp( m_SceneColorGrade.Brightness, -1.0f, 1.0f );
    m_SceneColorGrade.Contrast   = x_clamp( m_SceneColorGrade.Contrast, 0.0f, 4.0f );
}

//=========================================================================

const SceneColorGradeParams& PostProcessFeature::GetSceneColorGrade( void ) const
{
    return m_SceneColorGrade;
}

//=========================================================================
//  POST-PROCESSING PIPELINE
//=========================================================================

void PostProcessFeature::BeginFrame( void )
{
    if( !m_isInitialized )
    {
        RENDER_THROW( "PostProcessFeature", "BeginFrame called before initialization" );
        return;
    }

    m_isRequestsSealed = FALSE;
    ResetFrameRequests();
    m_hasMotionBlur                  = FALSE;
    m_hasForcedGlow                  = FALSE;
    m_FilterHistoryHandle            = RenderResourceHandle();
    m_FilterHistoryKey               = 0;
    m_GlowHistoryKey                 = 0;
    m_FilterResources.isHistoryValid = FALSE;
    m_GlowResources.isHistoryValid   = FALSE;
    UpdateGlowStageBegin();
}

//=========================================================================

void PostProcessFeature::BeginFrame( const RenderFrameContext& Frame )
{
    BeginFrame();

    const u64 Serial = eng_GetCurrentFrameSerial();

    for( s32 i = m_TemporalHistories.GetCount() - 1; i >= 0; --i )
    {
        if( Serial >= m_TemporalHistories[i].LastUsedSerial && Serial - m_TemporalHistories[i].LastUsedSerial >= 180 )
        {
            m_TemporalHistories.Delete( i );
        }
    }

    m_Bloom.BeginFrame( Frame.Desc.EnablePostProcessing );
    UpdatePostView( Frame );
}

//=========================================================================

xbool PostProcessFeature::Prepare( RenderFeaturePrepareContext& Context )
{
    if( !Context.Submission.IsSealed() )
    {
        RENDER_THROW( "PostProcessFeature", "post preparation requires a sealed submission" );
        return FALSE;
    }

    if( Context.Submission.IsSceneSubmitted() && Context.Frame.Desc.EnablePostProcessing &&
        ( m_Requests.DoZFogFn || m_Requests.DoZFogCustom ) )
    {
        UpdatePostView( Context.Frame );

        if( m_Requests.DoZFogFn )
        {
            BuildFogPalette( m_FogFilter.Fn[kFogGeneratedPaletteIndex], m_FogFilter.GeneratedColor,
                             m_FogFilter.GeneratedParam1, m_FogFilter.GeneratedParam2 );
        }

        const s32 PaletteIndex = m_FogFilter.PaletteIndex;

        if( ( PaletteIndex < 0 ) || ( PaletteIndex >= 5 ) || !m_isFogValid[PaletteIndex] ||
            !m_FogResources.UpdatePaletteTexture( *Context.Frame.pGraph, m_FogPalette[PaletteIndex] ) )
        {
            RENDER_THROW( "PostProcessFeature", "failed to upload fog palette index=%d", PaletteIndex );
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !Frame.SceneRendered )
    {
        return TRUE;
    }

    if( !m_Bloom.Declare( Graph, Frame ) )
    {
        return FALSE;
    }

    return Graph.GetLastError() == NULL;
}

//=========================================================================

xbool PostProcessFeature::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !Frame.SceneRendered )
    {
        return TRUE;
    }

    if( !m_Bloom.Build( Graph, Frame ) )
    {
        return FALSE;
    }

    return Graph.GetLastError() == NULL;
}

//=========================================================================

xbool PostProcessFeature::BuildGraphStage( RenderFeatureGraphStage Stage,
                                           FrameGraphBuilder&      Graph,
                                           RenderFrameContext&     Frame )
{
    if( ( Stage != RENDER_FEATURE_STAGE_SCREEN_FADE ) && !Frame.SceneRendered )
    {
        return TRUE;
    }

    if( !Frame.Desc.EnablePostProcessing )
    {
        return TRUE;
    }

    switch( Stage )
    {
        case RENDER_FEATURE_STAGE_FOG:
        {
            return DeclareFogPass( Graph, Frame );
        }
        case RENDER_FEATURE_STAGE_POST_PROCESS:
        {
            return DeclarePostChain( Graph, Frame );
        }
        case RENDER_FEATURE_STAGE_FILM_GRAIN:
        {
            if( !m_Requests.DoNoise )
            {
                return TRUE;
            }

            RenderPassDesc Desc;
            Desc.pDebugName  = "Noise";
            Desc.Color       = Frame.FinalColor;
            Desc.ColorLoadOp = RTARGET_LOAD_LOAD;

            const RenderPassId Pass = Graph.AddPass( Desc, ExecuteNoisePass, this );
            return Pass != INVALID_RENDER_PASS && !Graph.GetLastError() &&
                   Graph.DeclareCompositePipeline( Pass, COMPOSITE_BLEND_ALPHA_RGB, &m_FilterResources.NoisePS );
        }
        case RENDER_FEATURE_STAGE_SCREEN_FADE:
        {
            return DeclareScreenFadePass( Graph, Frame );
        }
        default:
        {
            return TRUE;
        }
    }
}

//=========================================================================

void PostProcessFeature::AbortFrame( void )
{
    m_isRequestsSealed               = TRUE;
    m_hasMotionBlur                  = FALSE;
    m_FilterHistoryHandle            = RenderResourceHandle();
    m_FilterHistoryKey               = 0;
    m_GlowHistoryKey                 = 0;
    m_FilterResources.isHistoryValid = FALSE;
    m_GlowResources.isHistoryValid   = FALSE;
    m_Bloom.AbortFrame();
    ResetFrameRequests();
    m_GlowResources.ResetFrame();
}

//=========================================================================

void PostProcessFeature::ResetFrameRequests( void )
{
    const xbool OverrideEnabled = m_Requests.Override;
    m_Requests                  = PostChainRequests();
    m_Requests.Override         = OverrideEnabled;
    m_MotionBlur                = PostMotionBlurParams();
    m_Glow                      = PostGlowParams();
    m_RadialBlur                = PostRadialBlurParams();
    m_ScreenWarp                = PostScreenWarpParams();
    m_PreparedWarpViews.SetCount( 0 );
    m_FogFilter     = PostFogFilterParams();
    m_MipFilter     = PostMipFilterParams();
    m_PainFilter    = PostMipFilterParams();
    m_pMipTexture   = NULL;
    m_Simple        = PostSimpleParams();
    m_FilterPost[0] = RenderResourceHandle();
    m_FilterPost[1] = RenderResourceHandle();

    for( s32 i = 0; i < MAX_POST_MIPS; ++i )
    {
        m_FilterMip[i] = RenderResourceHandle();
    }

    m_FilterRadialMip   = RenderResourceHandle();
    m_FilterPain        = RenderResourceHandle();
    m_FilterPainScratch = RenderResourceHandle();
    m_FilterMipSource   = RenderResourceHandle();
    m_FilterWarpSource  = RenderResourceHandle();
    m_FilterWarpResult  = RenderResourceHandle();

    for( s32 i = 0; i < 2; ++i )
    {
        m_WarpMaskRaw[i]     = RenderResourceHandle();
        m_WarpMaskScratch[i] = RenderResourceHandle();
        m_WarpMask[i]        = RenderResourceHandle();
    }

    m_ScreenFadeSource = RenderResourceHandle();
    m_nFilterMips      = 0;
}

//=========================================================================
//  TEMPORAL HISTORY
//=========================================================================

xbool PostProcessFeature::PrepareTemporalHistory( u64                       Key,
                                                  const RenderExtent&       Extent,
                                                  rtarget_format            Format,
                                                  const RenderFrameContext& Frame )
{
    if( ( Key == 0 ) || !Extent.IsValid() || ( Format == RTARGET_FORMAT_COUNT ) )
    {
        return FALSE;
    }

    TemporalHistoryState* pState = NULL;

    for( s32 i = 0; i < m_TemporalHistories.GetCount(); ++i )
    {
        if( m_TemporalHistories[i].Key == Key )
        {
            pState = &m_TemporalHistories[i];
            break;
        }
    }

    if( !pState )
    {
        TemporalHistoryState StateData;
        StateData.Key = Key;

        m_TemporalHistories.Append() = StateData;
        pState                       = &m_TemporalHistories[m_TemporalHistories.GetCount() - 1];
    }

    if( ( pState->Width != Extent.Width ) || ( pState->Height != Extent.Height ) || ( pState->Format != Format ) )
    {
        pState->Width  = Extent.Width;
        pState->Height = Extent.Height;
        pState->Format = Format;
        pState->Valid  = FALSE;
    }

    if( pState->ResetToken != Frame.Desc.TemporalResetToken || !Frame.pResources ||
        !Frame.pResources->HasPersistentResource( Key ) )
    {
        pState->Valid = FALSE;
    }

    pState->ResetToken     = Frame.Desc.TemporalResetToken;
    pState->LastUsedSerial = eng_GetCurrentFrameSerial();

    return pState->Valid;
}

//=========================================================================

void PostProcessFeature::MarkTemporalHistoryValid( u64                       Key,
                                                   const RenderExtent&       Extent,
                                                   rtarget_format            Format,
                                                   const RenderFrameContext& Frame )
{
    PrepareTemporalHistory( Key, Extent, Format, Frame );

    for( s32 i = 0; i < m_TemporalHistories.GetCount(); ++i )
    {
        if( m_TemporalHistories[i].Key == Key )
        {
            m_TemporalHistories[i].Valid = TRUE;
            return;
        }
    }
}

//=========================================================================

void PostProcessFeature::InvalidateTemporalHistoryKey( u64 Key )
{
    for( s32 i = 0; i < m_TemporalHistories.GetCount(); ++i )
    {
        if( m_TemporalHistories[i].Key == Key )
        {
            m_TemporalHistories[i].Valid = FALSE;
            return;
        }
    }
}

//=========================================================================

void PostProcessFeature::UpdatePostView( const RenderFrameContext& Frame )
{
    const RenderView* pRenderView = Frame.Desc.Views.pViews ? &Frame.Desc.Views.pViews[0] : NULL;
    const view*       pView       = pRenderView ? pRenderView->GetSourceView() : NULL;

    if( pView )
    {
        pView->GetViewport( m_PostViewL, m_PostViewT, m_PostViewR, m_PostViewB );
        pView->GetZLimits( m_PostNearZ, m_PostFarZ );
    }
}

//=========================================================================
//  GRAPH DECLARATION
//=========================================================================

xbool PostProcessFeature::DeclarePostChain( FrameGraphBuilder& Graph, RenderFrameContext& FrameData )
{
    if( !m_isInitialized || m_isRequestsSealed )
    {
        RENDER_THROW( "PostProcessFeature", "post chain declaration has invalid lifecycle state" );
        return FALSE;
    }

    UpdatePostView( FrameData );

    const xbool HasScenePost = m_Requests.DoZFogCustom || m_Requests.DoZFogFn || m_Requests.DoMipCustom ||
                               m_Requests.DoMipFn || ( m_PainFilter.PaletteIndex >= 0 ) || m_Requests.DoMotionBlur ||
                               m_Requests.DoSelfIllumGlow || m_Requests.DoRadialBlur || ( m_ScreenWarp.Count > 0 );

    if( !HasScenePost )
    {
        m_isRequestsSealed = TRUE;
        return TRUE;
    }

    RenderPassId PassData = INVALID_RENDER_PASS;

    if( m_Requests.DoMotionBlur )
    {
        m_hasMotionBlur = TRUE;

        const RenderResourceDesc* pSceneColor = Graph.GetDesc( FrameData.SceneColor );

        if( !pSceneColor )
        {
            RENDER_THROW( "PostProcessFeature", "motion blur scene color resource is unavailable" );
            return FALSE;
        }

        m_FilterHistoryKey               = RenderHistoryKey( kFilterHistoryFeatureKey, FrameData );
        m_FilterResources.isHistoryValid = FALSE;

        if( m_FilterHistoryKey != 0 )
        {
            m_FilterResources.isHistoryValid =
                PrepareTemporalHistory( m_FilterHistoryKey, pSceneColor->Extent, pSceneColor->Format, FrameData );

            RenderResourceDesc History;
            History.Extent = pSceneColor->Extent;
            History.Format = pSceneColor->Format;
            History.Usage =
                RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_DST;
            History.Lifetime      = RENDER_RESOURCE_PERSISTENT;
            History.PersistentKey = m_FilterHistoryKey;
            History.pDebugName    = "PostFilterHistory";
            m_FilterHistoryHandle = Graph.CreateResource( History );

            if( !m_FilterHistoryHandle.IsValid() )
            {
                RENDER_THROW( "PostProcessFeature", "failed to allocate motion blur history resource" );
                return FALSE;
            }
        }
    }
#define ADD_COLOR_POST_PASS( Name, Callback, Blend, PixelShader )                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        RenderPassDesc Desc;                                                                                           \
        Desc.pDebugName  = Name;                                                                                       \
        Desc.Color       = FrameData.ScenePostColor;                                                                   \
        Desc.ColorLoadOp = RTARGET_LOAD_LOAD;                                                                          \
        PassData         = Graph.AddPass( Desc, Callback, this );                                                      \
        Graph.Read( PassData, vram_GetShaderResource( m_FilterResources.MipRampTexture ) );                              \
        if( PassData == INVALID_RENDER_PASS )                                                                          \
        {                                                                                                              \
            RENDER_THROW( "PostProcessFeature", "failed to declare post pass '%s'", Name );                            \
            return FALSE;                                                                                              \
        }                                                                                                              \
        if( Graph.GetLastError() )                                                                                     \
        {                                                                                                              \
            RENDER_THROW( "PostProcessFeature", "failed to declare resources for post pass '%s'", Name );              \
            return FALSE;                                                                                              \
        }                                                                                                              \
        if( !Graph.DeclareCompositePipeline( PassData, Blend, PixelShader ) )                                          \
        {                                                                                                              \
            return FALSE;                                                                                              \
        }                                                                                                              \
    } while( FALSE )

    if( ( m_Requests.DoMipCustom || m_Requests.DoMipFn ) && !DeclareMipFilterPasses( Graph, FrameData ) )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare mip filter passes" );
        return FALSE;
    }

    if( !DeclarePainFilterPasses( Graph, FrameData ) )
    {
        return FALSE;
    }

    if( m_Requests.DoMotionBlur )
    {
        ADD_COLOR_POST_PASS( "MotionBlur", ExecuteMotionPass, COMPOSITE_BLEND_ALPHA_RGB,
                             &m_FilterResources.MotionBlurPS );
        if( m_FilterResources.isHistoryValid )
        {
            Graph.Read( PassData, m_FilterHistoryHandle );
        }
    }

    if( m_Requests.DoSelfIllumGlow )
    {
        if( !m_Bloom.DeclarePostPasses( Graph, FrameData, *this ) )
        {
            RENDER_THROW( "PostProcessFeature", "failed to declare glow passes" );
            return FALSE;
        }

        if( m_GlowResult.IsValid() )
        {
            ADD_COLOR_POST_PASS( "BloomComposite", ExecuteBloomCompositePass, COMPOSITE_BLEND_ADDITIVE_RGB,
                                 &m_GlowResources.CompositePS );
            Graph.Read( PassData, m_GlowResult );
        }
    }

    if( ( m_Requests.DoRadialBlur || ( m_ScreenWarp.Count > 0 ) ) && !DeclareWarpPasses( Graph, FrameData ) )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare warp passes" );
        return FALSE;
    }

    if( m_Requests.DoRadialBlur || m_ScreenWarp.Count > 0 )
    {
        FrameData.ScenePostColor = m_FilterWarpResult;
    }

#undef ADD_COLOR_POST_PASS

    if( !DeclareFilterHistoryPass( Graph, FrameData ) )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare temporal history update" );
        return FALSE;
    }

    m_isRequestsSealed = TRUE;

    return Graph.GetLastError() == NULL;
}

//=========================================================================

xbool PostProcessFeature::DeclareFogPass( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( m_isRequestsSealed )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass declared after post chain seal" );
        return FALSE;
    }

    if( !m_Requests.DoZFogCustom && !m_Requests.DoZFogFn )
    {
        return TRUE;
    }

    RenderPassDesc Desc;
    Desc.pDebugName  = "SceneFog";
    Desc.Color       = Frame.GetSceneColorTarget();
    Desc.ColorLoadOp = RTARGET_LOAD_LOAD;

    const RenderPassId PassData = Graph.AddPass( Desc, ExecuteFogPass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare SceneFog pass" );
        return FALSE;
    }

    Graph.Read( PassData, Frame.SceneDepth );
    Graph.Read( PassData, GetFogPaletteResource() );

    if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_ALPHA_RGB, &m_FogResources.CompositePS ) )
    {
        return FALSE;
    }

    return Graph.GetLastError() == NULL;
}

//=========================================================================

void PostProcessFeature::EndFrame( void )
{
    ResetFrameRequests();
    m_isRequestsSealed = TRUE;
    m_Bloom.EndFrame();
}

//=========================================================================
//  PASS EXECUTION
//=========================================================================

xbool PostProcessFeature::ExecuteFogPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const RenderFrameContext& Frame = Context.GetFrame();

    if( !pPost || !Context.Write( Frame.GetSceneColorTarget() ) || !Context.Read( Frame.SceneDepth ) )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass resources are unavailable" );
        return FALSE;
    }

    if( !pPost->ExecuteZFogFilter( Context.Read( Frame.SceneDepth ) ) )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass execution failed" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::ExecuteMotionPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pHistory =
        ( pPost && pPost->m_FilterResources.isHistoryValid ) ? Context.Read( pPost->m_FilterHistoryHandle ) : NULL;

    if( !pPost || !Context.Write( Context.GetFrame().SceneColor ) )
    {
        RENDER_THROW( "PostProcessFeature", "motion blur pass resources are unavailable" );
        return FALSE;
    }

    if( pPost->m_FilterResources.isHistoryValid && !pHistory )
    {
        RENDER_THROW( "PostProcessFeature", "motion blur history resource is unavailable" );
        return FALSE;
    }

    if( !pPost->ExecuteMotionBlur( pHistory ) )
    {
        RENDER_THROW( "PostProcessFeature", "motion blur pass execution failed" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::ExecuteRadialPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost || !Context.Write( Context.GetPass().Color ) )
    {
        RENDER_THROW( "PostProcessFeature", "radial blur output resource is unavailable" );
        return FALSE;
    }

    if( !pPost->ExecuteRadialBlur( Context.Read( Context.GetFrame().SceneColor ),
                                   Context.Read( pPost->m_FilterRadialMip ), Context.GetFrame() ) )
    {
        RENDER_THROW( "PostProcessFeature", "radial blur pass execution failed" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::ExecuteWarpPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const RenderFrameContext& Frame = Context.GetFrame();

    if( !pPost )
    {
        RENDER_THROW( "PostProcessFeature", "screen warp pass received a null feature" );
        return FALSE;
    }

    const RenderResourceHandle Source = pPost->m_FilterWarpSource;

    if( !Context.Write( pPost->m_FilterWarpResult ) )
    {
        RENDER_THROW( "PostProcessFeature", "screen warp output resource is unavailable" );
        return FALSE;
    }

    if( !pPost->ExecuteScreenWarps( Context.Read( Source ), Context.Read( Frame.SceneDepth ), Frame, Context.GetView(),
                                    Context.Read( pPost->m_WarpMask[0] ), Context.Read( pPost->m_WarpMask[1] ) ) )
    {
        RENDER_THROW( "PostProcessFeature", "screen warp pass execution failed" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::ExecuteNoisePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost || !Context.Write( Context.GetPass().Color ) )
    {
        RENDER_THROW( "PostProcessFeature", "noise pass resources are unavailable" );
        return FALSE;
    }

    if( !pPost->ExecuteNoiseFilter() )
    {
        RENDER_THROW( "PostProcessFeature", "noise pass execution failed" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  EFFECT REQUEST RECORDING
//=========================================================================

xbool PostProcessFeature::AcceptRequest( const char* pName ) const
{
    if( !m_isInitialized )
    {
        RENDER_THROW( "PostProcessFeature", "%s requested before initialization", pName );
        return FALSE;
    }

    if( m_isRequestsSealed )
    {
        RENDER_THROW( "PostProcessFeature", "%s requested after post chain seal", pName );
        return FALSE;
    }

    return !m_Requests.Override;
}

//=========================================================================

void PostProcessFeature::ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff )
{
    if( !AcceptRequest( "ApplySelfIllumGlows" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    m_Requests.DoSelfIllumGlow = TRUE;
    m_Glow.MotionBlurIntensity = x_clamp( MotionBlurIntensity, 0.0f, 1.0f );
    m_Glow.Cutoff              = x_clamp( GlowCutoff, 0, 255 );
}

//=========================================================================

void PostProcessFeature::AddScreenWarp( const vector3& WorldPos, f32 Radius, f32 WarpAmount )
{
    if( !AcceptRequest( "AddScreenWarp" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    if( !WorldPos.IsValid() || !x_isvalid( Radius ) || !x_isvalid( WarpAmount ) || ( Radius <= 0.0f ) ||
        ( WarpAmount <= 0.0f ) )
    {
        return;
    }

    const RenderFrameContext* pFrame      = GetRenderPipeline().GetFrame();
    const RenderView*         pRenderView = pFrame ? pFrame->GetView( 0 ) : NULL;
    const view*               pView       = pRenderView ? pRenderView->GetSourceView() : NULL;

    if( !pFrame || !pFrame->Desc.EnablePostProcessing || !pView ||
        ( pView->SphereInView( WorldPos, Radius, view::WORLD ) == view::VISIBLE_NONE ) )
    {
        return;
    }

    s32 Index = m_ScreenWarp.Count;

    if( Index >= MAX_POST_SCREEN_WARPS )
    {
        const f32 NewDistance      = pView->ConvertW2V( WorldPos ).LengthSquared() / ( Radius * Radius );
        f32       FarthestDistance = NewDistance;
        Index                      = -1;

        for( s32 i = 0; i < m_ScreenWarp.Count; ++i )
        {
            const f32 Distance = pView->ConvertW2V( m_ScreenWarp.WorldPos[i] ).LengthSquared() /
                                 ( m_ScreenWarp.Radius[i] * m_ScreenWarp.Radius[i] );

            if( Distance > FarthestDistance )
            {
                FarthestDistance = Distance;
                Index            = i;
            }
        }

        if( Index < 0 )
        {
            return;
        }
    }
    else
    {
        m_ScreenWarp.Count++;
    }

    m_ScreenWarp.WorldPos[Index] = WorldPos;
    m_ScreenWarp.Radius[Index]   = Radius;
    m_ScreenWarp.Amount[Index]   = WarpAmount;
}

//=========================================================================

void PostProcessFeature::MotionBlur( f32 Intensity )
{
    if( !AcceptRequest( "MotionBlur" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    m_Requests.DoMotionBlur = TRUE;
    m_MotionBlur.Intensity  = Intensity;
}

//=========================================================================

void PostProcessFeature::ZFogFilter( render::post_falloff_fn FnData, xcolor ColorData, f32 Param1, f32 Param2 )
{
    if( !AcceptRequest( "ZFogFilter" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    ASSERT( FnData != render::FALLOFF_CUSTOM );

    m_FogFilter.GeneratedColor  = ColorData;
    m_FogFilter.GeneratedParam1 = Param1;
    m_FogFilter.GeneratedParam2 = Param2;
    m_FogFilter.PaletteIndex    = 2;
    m_FogFilter.Fn[2]           = FnData;
    m_Requests.DoZFogFn         = TRUE;
}

//=========================================================================

void PostProcessFeature::ZFogFilter( render::post_falloff_fn FnData, s32 PaletteIndex )
{
    if( !AcceptRequest( "ZFogFilter" ) )
    {
        return;
    }

    ASSERT( ( PaletteIndex >= 0 ) && ( PaletteIndex <= 4 ) );
    if( !m_isFogValid[PaletteIndex] )
    {
        RENDER_THROW( "PostProcessFeature", "fog palette is unavailable index=%d", PaletteIndex );
        return;
    }

    ASSERT( !m_isRequestsSealed );
    ASSERT( FnData == render::FALLOFF_CUSTOM );
    m_Requests.DoZFogCustom      = TRUE;
    m_FogFilter.Fn[PaletteIndex] = FnData;
    m_FogFilter.PaletteIndex     = PaletteIndex;
}

//=========================================================================

void PostProcessFeature::MipFilter( s32                     nFilters,
                                    f32                     Offset,
                                    render::post_falloff_fn FnData,
                                    xcolor                  ColorData,
                                    f32                     Param1,
                                    f32                     Param2,
                                    s32                     PaletteIndex )
{
    if( !AcceptRequest( "MipFilter" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    ASSERT( FnData != render::FALLOFF_CUSTOM );
    ASSERT( ( PaletteIndex >= 0 ) && ( PaletteIndex < 4 ) );

    if( FnData == render::FALLOFF_CONSTANT )
    {
        m_PainFilter.Fn[PaletteIndex]     = FnData;
        m_PainFilter.Color[PaletteIndex]  = ColorData;
        m_PainFilter.Count[PaletteIndex]  = nFilters;
        m_PainFilter.Offset[PaletteIndex] = Offset;
        m_PainFilter.PaletteIndex         = PaletteIndex;

        return;
    }

    BuildMipPalette( FnData, ColorData, Param1, Param2, PaletteIndex );
    m_Requests.DoMipFn               = TRUE;
    m_MipFilter.Fn[PaletteIndex]     = FnData;
    m_MipFilter.Count[PaletteIndex]  = nFilters;
    m_MipFilter.Offset[PaletteIndex] = Offset;
    m_MipFilter.PaletteIndex         = PaletteIndex;
}

//=========================================================================

void PostProcessFeature::MipFilter( s32                     nFilters,
                                    f32                     Offset,
                                    render::post_falloff_fn FnData,
                                    const texture::handle&  Texture,
                                    s32                     PaletteIndex )
{
    if( !AcceptRequest( "MipFilter" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    ASSERT( FnData == render::FALLOFF_CUSTOM );
    ASSERT( ( PaletteIndex >= 0 ) && ( PaletteIndex < 4 ) );

    if( Texture.GetPointer() == NULL )
    {
        RENDER_THROW( "PostProcessFeature", "MipFilter custom texture is unavailable" );
        return;
    }

    m_Requests.DoMipCustom           = TRUE;
    m_pMipTexture                    = &Texture.GetPointer()->m_bitmap;
    m_MipFilter.Fn[PaletteIndex]     = FnData;
    m_MipFilter.Count[PaletteIndex]  = nFilters;
    m_MipFilter.Offset[PaletteIndex] = Offset;
    m_MipFilter.PaletteIndex         = PaletteIndex;
    ASSERT( m_pMipTexture );
}

//=========================================================================

void PostProcessFeature::NoiseFilter( xcolor ColorData )
{
    if( !AcceptRequest( "NoiseFilter" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    m_Requests.DoNoise  = TRUE;
    m_Simple.NoiseColor = ColorData;
}

//=========================================================================

void PostProcessFeature::ScreenFade( xcolor ColorData )
{
    if( !AcceptRequest( "ScreenFade" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    m_Simple.FadeColor      = ColorData;
    m_Requests.DoScreenFade = TRUE;
}

//=========================================================================

void PostProcessFeature::RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale )
{
    if( !AcceptRequest( "RadialBlur" ) )
    {
        return;
    }

    ASSERT( !m_isRequestsSealed );
    m_Requests.DoRadialBlur = TRUE;
    m_RadialBlur.Zoom       = Zoom;
    m_RadialBlur.Angle      = Angle;
    m_RadialBlur.AlphaSub   = AlphaSub;
    m_RadialBlur.AlphaScale = AlphaScale;
}

//=========================================================================

void PostProcessFeature::InvalidateTemporalHistory( void )
{
    m_FilterResources.InvalidateHistory();
    m_GlowResources.InvalidateHistory();

    for( s32 i = 0; i < m_TemporalHistories.GetCount(); ++i )
    {
        m_TemporalHistories[i].Valid = FALSE;
    }
}

} // namespace render
