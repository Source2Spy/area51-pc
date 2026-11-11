//=========================================================================
//
//  PostProcessFeature_Glow.cpp
//
//  Glow post-processing module.
//
//=========================================================================

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "a51_shader_catalog.hpp"

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PostProcessFeature.hpp"

namespace render
{

//=========================================================================
//  FILE-LOCAL TYPES AND HELPERS
//=========================================================================

namespace
{
static const f32 kGlowPulseRate           = 0.1500f;
static const f32 kGlowReferenceBlurHeight = 68.0f;

//=========================================================================

static xbool BindGlowConstants( const shader& ShaderData,
                                f32           Cutoff,
                                f32           HistoryRetention,
                                f32           StepX,
                                f32           StepY,
                                f32           CurrentWeight = 1.0f,
                                f32           CurrentAlpha  = 0.0f,
                                xbool         HistoryValid  = FALSE,
                                xbool         ForcedGlow    = FALSE )
{
    PostGlowConstants Constants;
    Constants.Params0.Set( Cutoff, HistoryRetention, CurrentWeight, HistoryValid ? 1.0f : 0.0f );
    Constants.Params1.Set( StepX, StepY, CurrentAlpha, ForcedGlow ? 1.0f : 0.0f );

    return shader_PushUniformData( SHADER_STAGE_PIXEL, ShaderData.ResourceSlots[A51Resource_GlowParams], &Constants,
                                   sizeof( Constants ) );
}

} // namespace

//=========================================================================
//  GLOW RESOURCE MANAGEMENT
//=========================================================================

PostProcessFeature::GlowResources::GlowResources()
{
    BufferWidth    = 0;
    BufferHeight   = 0;
    BufferFormat   = RTARGET_FORMAT_COUNT;
    isHistoryValid = FALSE;
    PulseScale     = 1.0f;
    PulseDirection = kGlowPulseRate;
    PrefilterPS    = shader();
    DownsamplePS   = shader();
    BlurHPS        = shader();
    BlurVPS        = shader();
    CombinePS      = shader();
    CompositePS    = shader();
    AuxSampler     = rstate_sampler();
}

//=========================================================================

xbool PostProcessFeature::GlowResources::Initialize( void )
{
    Shutdown();
    ResetFrame();

    shader_LoadComboStage( PrefilterPS, A51FixedPrograms[A51FixedProgram_post_glow_prefilter].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_glow_prefilter].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( DownsamplePS, A51FixedPrograms[A51FixedProgram_post_glow_downsample].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_glow_downsample].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( BlurHPS, A51FixedPrograms[A51FixedProgram_post_glow_blur_horizontal].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_glow_blur_horizontal].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( BlurVPS, A51FixedPrograms[A51FixedProgram_post_glow_blur_vertical].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_glow_blur_vertical].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( CombinePS, A51FixedPrograms[A51FixedProgram_post_glow_combine].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_glow_combine].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( CompositePS, A51FixedPrograms[A51FixedProgram_post_glow_composite].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_glow_composite].Key,
                           SHADER_STAGE_PIXEL );
    rstate_CreateSampler( AuxSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostGlowAux" );

    if( !PrefilterPS || !DownsamplePS || !BlurHPS || !BlurVPS || !CombinePS || !CompositePS || !AuxSampler )
    {
        RENDER_THROW( "PostProcessFeature", "failed to initialize glow resources" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void PostProcessFeature::GlowResources::Shutdown( void )
{
    BufferWidth    = 0;
    BufferHeight   = 0;
    BufferFormat   = RTARGET_FORMAT_COUNT;
    isHistoryValid = FALSE;
    PulseScale     = 1.0f;
    PulseDirection = kGlowPulseRate;
    ResetFrame();

    rstate_DestroySampler( AuxSampler );
    shader_Destroy( PrefilterPS );
    shader_Destroy( DownsamplePS );
    shader_Destroy( BlurHPS );
    shader_Destroy( BlurVPS );
    shader_Destroy( CombinePS );
    shader_Destroy( CompositePS );
}

//=========================================================================

void PostProcessFeature::GlowResources::ResetFrame( void )
{
}

//=========================================================================

void PostProcessFeature::GlowResources::InvalidateHistory( void )
{
    ResetFrame();
    isHistoryValid = FALSE;
    PulseScale     = 1.0f;
    PulseDirection = kGlowPulseRate;
}

//=========================================================================
//  GLOW PASS EXECUTION
//=========================================================================

xbool PostProcessFeature::ExecuteGlowDownsamplePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost )
    {
        return FALSE;
    }

    s32 Index = -1;

    for( s32 i = 0; i < 3; ++i )
    {
        if( Context.GetPass().Color.Index == pPost->m_GlowDownsample[i].Index )
        {
            Index = i;
        }
    }

    if( Index < 0 )
    {
        return FALSE;
    }

    const RenderResourceHandle Source = Index == 0 ? Context.GetFrame().SceneColor : pPost->m_GlowDownsample[Index - 1];
    const rtarget*             pSource = Context.Read( Source );

    if( !pSource || !Context.Write( pPost->m_GlowDownsample[Index] ) )
    {
        return FALSE;
    }

    shader*   pPixel = Index == 0 ? &pPost->m_GlowResources.PrefilterPS : &pPost->m_GlowResources.DownsamplePS;
    const f32 Cutoff = 0.0f;

    if( !BindGlowConstants( *pPixel, Cutoff, 0.0f, 1.0f / static_cast<f32>( pSource->Desc.Width ),
                            1.0f / static_cast<f32>( pSource->Desc.Height ) ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, pPixel, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP,
                           A51Resource_GlowSource );
}

//=========================================================================

xbool PostProcessFeature::ExecuteGlowBlurPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost )
    {
        return FALSE;
    }

    const xbool                Horizontal = Context.GetPass().Color.Index == pPost->m_GlowBlur[0].Index;
    const RenderResourceHandle Source     = Horizontal ? pPost->m_GlowDownsample[2] : pPost->m_GlowBlur[0];
    const rtarget*             pSource    = Context.Read( Source );
    const shader& PixelShader = Horizontal ? pPost->m_GlowResources.BlurHPS : pPost->m_GlowResources.BlurVPS;
    const RenderFrameContext& Frame = Context.GetFrame();
    const RenderResourceDesc* pScene = Frame.pGraph->GetDesc( Frame.SceneColor );

    if( !pSource || !pScene || !pScene->Extent.IsValid() || !Context.Write( Context.GetPass().Color ) )
    {
        return FALSE;
    }

    const f32 StepY = 1.0f / kGlowReferenceBlurHeight;
    const f32 StepX = StepY * static_cast<f32>( pScene->Extent.Height ) / pScene->Extent.Width;
    if( !BindGlowConstants( PixelShader, 0.0f, 0.0f, Horizontal ? StepX : 0.0f, Horizontal ? 0.0f : StepY ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &PixelShader, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP,
                           A51Resource_GlowSource );
}

//=========================================================================

xbool PostProcessFeature::ExecuteGlowCombinePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const xbool    PulsingGlow = pPost && ( pPost->m_hasForcedGlow || pPost->m_Glow.MotionBlurIntensity > 0.0f );
    const rtarget* pSource     = pPost ? Context.Read( pPost->m_GlowBlur[1] ) : NULL;
    const rtarget* pHistory =
        pPost && pPost->m_GlowResources.isHistoryValid ? Context.Read( pPost->m_GlowHistory ) : pSource;

    if( !pPost || !pSource || !pHistory || !Context.Write( pPost->m_GlowCombined ) ||
        !BindGlowConstants( pPost->m_GlowResources.CombinePS, static_cast<f32>( pPost->m_Glow.Cutoff ) / 255.0f,
                            pPost->m_GlowHistoryRetention, 1.0f / static_cast<f32>( pSource->Desc.Width ),
                            1.0f / static_cast<f32>( pSource->Desc.Height ), pPost->m_GlowCurrentWeight,
                            PulsingGlow ? x_clamp( pPost->m_Glow.MotionBlurIntensity, 0.0f, 1.0f ) : 0.0125f,
                            pPost->m_GlowResources.isHistoryValid, PulsingGlow ) )
    {
        return FALSE;
    }

    composite_extra_source GlowAux;
    GlowAux.ResourceSemantic = A51Resource_GlowAux;
    GlowAux.pResource        = rtarget_GetShaderResource( *pHistory );
    GlowAux.SamplerMode      = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;

    if( !GlowAux.pResource )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &pPost->m_GlowResources.CombinePS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_GlowSource, &GlowAux, 1 );
}

//=========================================================================

xbool PostProcessFeature::ExecuteGlowHistoryPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost || !Context.Copy( pPost->m_GlowHistory, pPost->m_GlowResult ) )
    {
        return FALSE;
    }

    pPost->m_GlowResources.isHistoryValid = TRUE;

    const RenderResourceDesc* pHistory = Context.GetFrame().pGraph->GetDesc( pPost->m_GlowHistory );

    if( !pHistory )
    {
        return FALSE;
    }

    pPost->MarkTemporalHistoryValid( pPost->m_GlowHistoryKey, pHistory->Extent, pHistory->Format, Context.GetFrame() );

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::ExecuteBloomCompositePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pResult = pPost ? Context.Read( pPost->m_GlowResult ) : NULL;

    if( !pResult || !Context.Write( Context.GetFrame().SceneColor ) )
    {
        return FALSE;
    }

    return composite_Blit( *pResult, COMPOSITE_BLEND_ADDITIVE_RGB, 1.0f, &pPost->m_GlowResources.CompositePS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_GlowSource );
}

//=========================================================================

void PostProcessFeature::UpdateGlowStageBegin( void )
{
    m_GlowResources.ResetFrame();

    for( s32 i = 0; i < 3; ++i )
    {
        m_GlowDownsample[i] = RenderResourceHandle();
    }

    for( s32 i = 0; i < 2; ++i )
    {
        m_GlowBlur[i] = RenderResourceHandle();
    }

    m_GlowCombined           = RenderResourceHandle();
    m_GlowHistory            = RenderResourceHandle();
    m_GlowResult             = RenderResourceHandle();
    m_shouldStoreGlowHistory = FALSE;
}

} // namespace render
