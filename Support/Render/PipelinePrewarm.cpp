//=========================================================================
//
//  PipelinePrewarm.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PipelinePrewarm.hpp"
#include "Pipeline/RenderPipeline.hpp"
#include "Backend/Frame/FrameCompiler.hpp"
#include "Backend/Primitive/PrimitiveEncoder.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

PipelinePrewarmContext::PipelinePrewarmContext( const RenderScopeDesc& Scope )
    : m_ScenePass(),
      m_isShadowsEnabled( Scope.EnableShadows )
{
    const RenderQualityProfile Quality = RenderQualityProfile::PlatformDefault();
    m_ScenePass.ColorFormat            = Quality.Formats.SceneColorFormat;
    m_ScenePass.DepthFormat            = RTARGET_FORMAT_DEPTH_STENCIL;
    m_ScenePass.SampleCount            = Scope.SampleCount ? Scope.SampleCount : GetRenderPipeline().GetSampleCount();
    m_ScenePass.WriteSceneDepth        = TRUE;
    m_ScenePass.EnableShadows          = Scope.EnableShadows;
}

//=========================================================================

xbool PipelinePrewarmContext::QueueGeometry( const geom& Geom, xbool Skin )
{
    FrameCompiler& Compiler = GetRenderPipeline().GetBackend().GetFrameCompiler();

    for( s32 i = 0; i < Geom.m_nMaterials; ++i )
    {
        const material& Material = Geom.GetPreparedMaterial( Geom.m_pMaterial[i].iVirtualMat );

        if( !Material.GetDesc().Valid )
        {
            return FALSE;
        }

        if( !Compiler.QueueMaterialPipelines( Material, Skin, m_ScenePass ) )
        {
            return FALSE;
        }

        if( m_isShadowsEnabled && GetRenderPipeline().GetShadowMapAtlas().GetShadowMode() != ShadowMode::Disabled &&
            !GetRenderPipeline().GetShadowFeatureRef().Backend().QueueGeometryPipeline(
                Skin, Material.UsesShadowAlpha(), !Material.CullsBackFaces() ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool PipelinePrewarmContext::QueuePrimitive( const primitive_draw_desc& Desc )
{
    PrimitiveStorage::PipelineDesc Pipeline;

    if( !PrimitiveEncoder::BuildPipelineState( Pipeline, Desc ) )
    {
        return FALSE;
    }

    Pipeline.Pass.ColorFormat     = m_ScenePass.ColorFormat;
    Pipeline.Pass.DepthFormat     = m_ScenePass.DepthFormat;
    Pipeline.Pass.SampleCount     = m_ScenePass.SampleCount;
    Pipeline.Pass.WriteSceneDepth = m_ScenePass.WriteSceneDepth && Desc.Blend == PRIMITIVE_BLEND_OPAQUE;

    for( u32 Fog = 0; Fog < 2; ++Fog )
    {
        Pipeline.Pass.SceneFogMode = Fog ? RENDER_FOG_FORWARD_COLOR : RENDER_FOG_NONE;
        Pipeline.Pass.BloomFogMode = Fog ? RENDER_FOG_FORWARD_EMISSION : RENDER_FOG_NONE;

        if( !GetRenderPipeline().GetPrimitiveStorage().QueuePipeline( Pipeline ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool PipelinePrewarmContext::QueueDecal( decal_blend_mode Blend, u32 Flags )
{
    DecalEncoder::PassDesc Pass;
    Pass.EnableShadows   = m_isShadowsEnabled;
    Pass.ColorFormat     = m_ScenePass.ColorFormat;
    Pass.DepthFormat     = m_ScenePass.DepthFormat;
    Pass.SampleCount     = m_ScenePass.SampleCount;
    Pass.WriteSceneDepth = FALSE;

    for( u32 Fog = 0; Fog < 2; ++Fog )
    {
        Pass.SceneFogMode = Fog ? RENDER_FOG_FORWARD_COLOR : RENDER_FOG_NONE;
        Pass.BloomFogMode = Fog ? RENDER_FOG_FORWARD_EMISSION : RENDER_FOG_NONE;

        if( !GetRenderPipeline().GetDecalEncoder().QueuePipeline( Blend, Flags, Pass ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool PipelinePrewarmContext::QueueDynamic( u32 RenderFlags )
{
    if( m_isShadowsEnabled && GetRenderPipeline().GetShadowMapAtlas().GetShadowMode() != ShadowMode::Disabled &&
        !GetRenderPipeline().GetShadowFeatureRef().Backend().QueueDynamicPipeline() )
    {
        return FALSE;
    }

    return GetRenderPipeline().GetBackend().GetFrameCompiler().QueueDynamicPipeline( m_ScenePass, RenderFlags );
}

//=========================================================================

xbool PipelinePrewarmContext::QueueBuiltins( void )
{
    if( !GetRenderPipeline().GetBackend().GetFrameCompiler().QueueDefaultMaterialPipelines( m_ScenePass ) )
    {
        return FALSE;
    }

    if( m_isShadowsEnabled && GetRenderPipeline().GetShadowMapAtlas().GetShadowMode() != ShadowMode::Disabled &&
        GetRenderPipeline().GetShadowMapAtlas().GetShadowFilterType() == ShadowFilterType::Evsm &&
        !GetRenderPipeline().GetShadowFeatureRef().Backend().QueueFilterPipelines() )
    {
        return FALSE;
    }

    PrimitiveStorage::PipelineDesc Pipeline;
    Pipeline.Pass.ColorFormat     = m_ScenePass.ColorFormat;
    Pipeline.Pass.DepthFormat     = m_ScenePass.DepthFormat;
    Pipeline.Pass.SampleCount     = m_ScenePass.SampleCount;
    Pipeline.Pass.WriteSceneDepth = m_ScenePass.WriteSceneDepth;
    PrimitiveEncoder::BuildDepthRectPipelineState( Pipeline );

    return GetRenderPipeline().GetPrimitiveStorage().QueuePipeline( Pipeline );
}

} // namespace render
