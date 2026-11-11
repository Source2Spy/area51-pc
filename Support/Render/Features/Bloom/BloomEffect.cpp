//=========================================================================
//
//  BloomEffect.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BloomEffect.hpp"
#include "../PostProcess/PostProcessFeature.hpp"

namespace render
{

namespace
{

//=========================================================================
//  CONSTANTS
//=========================================================================

static const f32 kGlowPulseMin           = 0.6800f;
static const f32 kGlowPulseMax           = 0.7500f;
static const f32 kGlowPulseRate          = 0.1500f;
static const f32 kGlowReferenceFrameRate = 30.0f;
static const u64 kGlowHistoryFeatureKey  = 0x504f5354474c4f57ull;
} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

BloomEffect::BloomEffect( void )
    : m_isEnabled( FALSE ),
      m_State( STATE_DISABLED )
{
}

//=========================================================================

xbool BloomEffect::Init( void )
{
    m_isEnabled = FALSE;
    m_State     = STATE_DISABLED;
    return TRUE;
}

//=========================================================================

void BloomEffect::Kill( void )
{
    AbortFrame();
}

//=========================================================================

void BloomEffect::BeginFrame( xbool Enabled )
{
    m_isEnabled = Enabled;
    m_State     = Enabled ? STATE_IDLE : STATE_DISABLED;
}

//=========================================================================

void BloomEffect::BeginFrame( const RenderFrameContext& Frame )
{
    ( void )Frame;
    BeginFrame( TRUE );
}

//=========================================================================

void BloomEffect::AbortFrame( void )
{
    m_isEnabled = FALSE;
    m_State     = STATE_FAILED;
}

//=========================================================================

void BloomEffect::EndFrame( void )
{
    m_isEnabled = FALSE;
    m_State     = STATE_DISABLED;
}

//=========================================================================

xbool BloomEffect::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !m_isEnabled )
    {
        return TRUE;
    }

    const RenderResourceDesc* pSceneColor = Graph.GetDesc( Frame.SceneColor );

    if( !pSceneColor )
    {
        RENDER_THROW( "BloomEffect", "cannot declare bloom without SceneColor" );
        AbortFrame();

        return FALSE;
    }

    m_State = STATE_DECLARED;

    return TRUE;
}

//=========================================================================

xbool BloomEffect::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !m_isEnabled )
    {
        return TRUE;
    }

    if( ( m_State != STATE_DECLARED ) || ( Frame.pGraph != &Graph ) || !Frame.GetSceneDepthTarget().IsValid() )
    {
        RENDER_THROW( "BloomEffect", "invalid bloom build state" );
        AbortFrame();

        return FALSE;
    }

    m_State = STATE_BUILT;

    return TRUE;
}

//=========================================================================

xbool BloomEffect::DeclarePostPasses( FrameGraphBuilder&        Graph,
                                      const RenderFrameContext& Frame,
                                      PostProcessFeature&       Post )
{
    if( !m_isEnabled )
    {
        return TRUE;
    }

    if( m_State != STATE_BUILT )
    {
        RENDER_THROW( "BloomEffect", "bloom post chain was declared before feature build" );
        return FALSE;
    }

    const RenderResourceHandle Source  = Frame.SceneColor;
    const RenderResourceDesc*  pSource = Source.IsValid() ? Graph.GetDesc( Source ) : NULL;

    if( !pSource || !( pSource->Usage & RENDER_RESOURCE_USAGE_SAMPLED ) )
    {
        RENDER_THROW( "BloomEffect", "bloom contribution resource is unavailable" );
        return FALSE;
    }

    if( !Post.m_GlowResources.PrefilterPS || !Post.m_GlowResources.DownsamplePS || !Post.m_GlowResources.BlurHPS ||
        !Post.m_GlowResources.BlurVPS || !Post.m_GlowResources.CombinePS || !Post.m_GlowResources.CompositePS )
    {
        RENDER_THROW( "BloomEffect", "bloom shader set is incomplete" );
        return FALSE;
    }

    // Size the downsample chain from the current scene extent.
    u32 Widths[3];
    u32 Heights[3];
    Widths[0]  = MAX( ( pSource->Extent.Width + 1u ) / 2u, 1u );
    Heights[0] = MAX( ( pSource->Extent.Height + 1u ) / 2u, 1u );
    Widths[1]  = MAX( ( Widths[0] + 1u ) / 2u, 1u );
    Heights[1] = MAX( ( Heights[0] + 1u ) / 2u, 1u );
    Widths[2]  = MAX( ( Widths[1] + 1u ) / 2u, 1u );
    Heights[2] = MAX( ( Heights[1] + 1u ) / 2u, 1u );

    const rtarget_format Format = Frame.Desc.Formats.BloomFormat;

    if( ( Post.m_GlowResources.BufferWidth != Widths[2] ) || ( Post.m_GlowResources.BufferHeight != Heights[2] ) ||
        ( Post.m_GlowResources.BufferFormat != Format ) )
    {
        Post.m_GlowResources.BufferWidth  = Widths[2];
        Post.m_GlowResources.BufferHeight = Heights[2];
        Post.m_GlowResources.BufferFormat = Format;
    }

    static const char* sDownsampleNames[3] = { "BloomDownsample0", "BloomDownsample1", "BloomDownsample2" };

    for( s32 i = 0; i < 3; ++i )
    {
        RenderResourceDesc ResourceData;
        ResourceData.Extent = RenderExtent( Widths[i], Heights[i] );
        ResourceData.Format = Format;
        ResourceData.Usage =
            RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC;
        ResourceData.pDebugName = sDownsampleNames[i];

        Post.m_GlowDownsample[i] = Graph.CreateResource( ResourceData );

        if( !Post.m_GlowDownsample[i].IsValid() )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < 2; ++i )
    {
        RenderResourceDesc ResourceData;
        ResourceData.Extent = RenderExtent( Widths[2], Heights[2] );
        ResourceData.Format = Format;
        ResourceData.Usage =
            RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC;
        ResourceData.pDebugName = i == 0 ? "BloomBlurHorizontal" : "BloomBlurVertical";

        Post.m_GlowBlur[i] = Graph.CreateResource( ResourceData );

        if( !Post.m_GlowBlur[i].IsValid() )
        {
            return FALSE;
        }
    }

    RenderResourceDesc Combined;
    Combined.Extent     = RenderExtent( Widths[2], Heights[2] );
    Combined.Format     = Format;
    Combined.Usage      = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC;
    Combined.pDebugName = "BloomCombined";

    Post.m_GlowCombined = Graph.CreateResource( Combined );

    if( !Post.m_GlowCombined.IsValid() )
    {
        return FALSE;
    }

    // Reuse temporal history only for views with a stable identity.
    Post.m_GlowHistoryKey               = RenderHistoryKey( kGlowHistoryFeatureKey, Frame );
    Post.m_GlowResources.isHistoryValid = FALSE;

    if( Post.m_GlowHistoryKey != 0 )
    {
        Post.m_GlowResources.isHistoryValid =
            Post.PrepareTemporalHistory( Post.m_GlowHistoryKey, Combined.Extent, Combined.Format, Frame );

        RenderResourceDesc History = Combined;
        History.Lifetime           = RENDER_RESOURCE_PERSISTENT;
        History.PersistentKey      = Post.m_GlowHistoryKey;
        History.Usage |= RENDER_RESOURCE_USAGE_COPY_DST;
        History.pDebugName = "BloomHistory";

        Post.m_GlowHistory = Graph.CreateResource( History );

        if( !Post.m_GlowHistory.IsValid() )
        {
            return FALSE;
        }
    }

    Post.m_shouldStoreGlowHistory = Post.m_GlowHistoryKey != 0;
    Post.m_GlowHistoryRetention   = 0.0125f;
    Post.m_GlowCurrentWeight      = 1.0f;

    // Mutant glow history must survive when the last glowing actor leaves the view.
    if( Post.m_hasForcedGlow || Post.m_Glow.MotionBlurIntensity > 0.0f )
    {
        Post.m_GlowResources.PulseScale += Post.m_GlowResources.PulseDirection * Post.m_FrameDeltaTime;

        if( Post.m_GlowResources.PulseScale >= kGlowPulseMax )
        {
            Post.m_GlowResources.PulseScale     = kGlowPulseMax;
            Post.m_GlowResources.PulseDirection = -kGlowPulseRate;
        }
        else if( Post.m_GlowResources.PulseScale <= kGlowPulseMin )
        {
            Post.m_GlowResources.PulseScale     = kGlowPulseMin;
            Post.m_GlowResources.PulseDirection = kGlowPulseRate;
        }

        const f32 ReferenceRetention  = Post.m_GlowResources.PulseScale;
        const f32 ReferenceFrameCount = Post.m_FrameDeltaTime * kGlowReferenceFrameRate;
        Post.m_GlowHistoryRetention   = x_pow( ReferenceRetention, ReferenceFrameCount );
        Post.m_GlowCurrentWeight      = ( 1.0f - Post.m_GlowHistoryRetention ) / ( 1.0f - ReferenceRetention );
    }
    else
    {
        Post.m_GlowResources.PulseScale     = 1.0f;
        Post.m_GlowResources.PulseDirection = kGlowPulseRate;
    }

    if( !Post.m_shouldStoreGlowHistory )
    {
        Post.m_GlowResources.isHistoryValid = FALSE;

        if( Post.m_GlowHistoryKey != 0 )
        {
            Post.InvalidateTemporalHistoryKey( Post.m_GlowHistoryKey );
        }
    }

    // Downsample, blur, then combine the bloom contribution.
    RenderResourceHandle PassSource = Source;

    for( s32 i = 0; i < 3; ++i )
    {
        RenderPassDesc PassDesc;
        PassDesc.pDebugName  = sDownsampleNames[i];
        PassDesc.Color       = Post.m_GlowDownsample[i];
        PassDesc.ColorLoadOp = RTARGET_LOAD_DONT_CARE;

        const RenderPassId PassData = Graph.AddPass( PassDesc, PostProcessFeature::ExecuteGlowDownsamplePass, &Post );

        if( PassData == INVALID_RENDER_PASS )
        {
            return FALSE;
        }

        Graph.Read( PassData, PassSource );

        if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY,
                                             i == 0 ? &Post.m_GlowResources.PrefilterPS
                                                    : &Post.m_GlowResources.DownsamplePS ) )
        {
            return FALSE;
        }

        PassSource = Post.m_GlowDownsample[i];
    }

    for( s32 i = 0; i < 2; ++i )
    {
        RenderPassDesc PassDesc;
        PassDesc.pDebugName  = i == 0 ? "BloomBlurHorizontal" : "BloomBlurVertical";
        PassDesc.Color       = Post.m_GlowBlur[i];
        PassDesc.ColorLoadOp = RTARGET_LOAD_DONT_CARE;

        const RenderPassId PassData = Graph.AddPass( PassDesc, PostProcessFeature::ExecuteGlowBlurPass, &Post );

        if( PassData == INVALID_RENDER_PASS )
        {
            return FALSE;
        }

        Graph.Read( PassData, i == 0 ? Post.m_GlowDownsample[2] : Post.m_GlowBlur[0] );

        if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY,
                                             i == 0 ? &Post.m_GlowResources.BlurHPS : &Post.m_GlowResources.BlurVPS ) )
        {
            return FALSE;
        }
    }

    Post.m_GlowResult = Post.m_GlowBlur[1];

    if( Post.m_shouldStoreGlowHistory )
    {
        RenderPassDesc PassDesc;
        PassDesc.pDebugName  = "BloomHistoryCombine";
        PassDesc.Color       = Post.m_GlowCombined;
        PassDesc.ColorLoadOp = RTARGET_LOAD_DONT_CARE;

        const RenderPassId PassData = Graph.AddPass( PassDesc, PostProcessFeature::ExecuteGlowCombinePass, &Post );

        if( PassData == INVALID_RENDER_PASS )
        {
            return FALSE;
        }

        Graph.Read( PassData, Post.m_GlowBlur[1] );

        if( Post.m_GlowResources.isHistoryValid )
        {
            Graph.Read( PassData, Post.m_GlowHistory );
        }

        if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &Post.m_GlowResources.CombinePS ) )
        {
            return FALSE;
        }

        Post.m_GlowResult = Post.m_GlowCombined;
    }

    if( Post.m_shouldStoreGlowHistory )
    {
        RenderPassDesc PassDesc;
        PassDesc.pDebugName  = "BloomHistoryUpdate";
        PassDesc.Type        = RENDER_PASS_COPY;
        PassDesc.Source      = Post.m_GlowResult;
        PassDesc.Destination = Post.m_GlowHistory;

        const RenderPassId PassData = Graph.AddPass( PassDesc, PostProcessFeature::ExecuteGlowHistoryPass, &Post );

        if( PassData == INVALID_RENDER_PASS )
        {
            return FALSE;
        }

        Graph.KeepAlive( PassData );
    }

    return Graph.GetLastError() == NULL;
}

} // namespace render
