//=========================================================================
//
//  VolumetricFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "VolumetricFeature.hpp"

#include "../../Pipeline/RenderPipeline.hpp"
#include "../../Texture.hpp"
#include "e_Engine.hpp"
#include "a51_shader_catalog.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void SubmitVolumetric( volumetric_draw_desc const& Draw )
{
    RenderSubmission& Submission = GetRenderPipeline().GetSubmission();
    Submission.ValidateOpen( "SubmitVolumetric" );

    if( !Submission.IsOpen() )
    {
        return;
    }

    if( !Draw.pTexture || !Draw.LocalToWorld.IsValid() || !Draw.StartColor.IsValid() ||
        !Draw.EndColor.IsValid() || !x_isvalid( Draw.StartSize ) || !x_isvalid( Draw.EndSize ) ||
        !x_isvalid( Draw.Length ) || !x_isvalid( Draw.Intensity ) ||
        !x_isvalid( Draw.EdgeSoftness ) || !x_isvalid( Draw.StartFade ) || !x_isvalid( Draw.EndFade ) ||
        Draw.EdgeSoftness < 0.0f || Draw.EdgeSoftness > 1.0f || Draw.StartFade < 0.0f ||
        Draw.StartFade > 1.0f || Draw.EndFade < 0.0f || Draw.EndFade > 1.0f || Draw.StartSize <= 0.0f ||
        Draw.EndSize <= 0.0f || Draw.Length <= 0.0f || Draw.Intensity < 0.0f )
    {
        RENDER_THROW( "Volumetric", "invalid authored volume description" );
        return;
    }

    if( Draw.Intensity == 0.0f )
    {
        return;
    }

    Submission.Volumetrics().Append() = Draw;
}

//=========================================================================

xbool VolumetricFeature::Init( void )
{
    Kill();

    auto Load = []( shader& Shader, u32 Program ) -> xbool
    {
        return shader_LoadComboStage( Shader, A51FixedPrograms[Program].PackageFile, SHADER_COMBO_FIXED,
                                     A51FixedPrograms[Program].Key, SHADER_STAGE_PIXEL );
    };

    if( !Load( m_PixelShader, A51FixedProgram_volumetric ) ||
        !Load( m_MultisampleShader, A51FixedProgram_volumetric_msaa ) ||
        !rstate_CreateSampler( m_PointSampler, RSTATE_SAMPLER_PRESET_POINT_CLAMP, "VolumeDepth" ) ||
        !rstate_CreateSampler( m_LinearSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "VolumeMask" ) )
    {
        Kill();
        RENDER_THROW( "Volumetric", "failed to initialize volume pipelines" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void VolumetricFeature::Kill( void )
{
    m_Passes.Clear();
    shader_Destroy( m_MultisampleShader );
    shader_Destroy( m_PixelShader );
    rstate_DestroySampler( m_LinearSampler );
    rstate_DestroySampler( m_PointSampler );
}

//=========================================================================

xbool VolumetricFeature::QueuePipelines( GraphicsPipelineCache&  Context,
                                       RenderFormatPolicy const& Formats,
                                       u32                       Samples )
{
    render_pipeline_desc Desc;
    shader const* pShader = Samples > 1 ? &m_MultisampleShader : &m_PixelShader;

    return composite_GetPipelineDesc( Desc, COMPOSITE_BLEND_PREMULT_ADDITIVE_RGB, Formats.SceneColorFormat,
                                     RTARGET_FORMAT_COUNT, Samples, pShader ) && Context.QueuePrewarm( Desc );
}

//=========================================================================

xbool VolumetricFeature::BuildGraphStage( RenderFeatureGraphStage Stage,
                                        FrameGraphBuilder&      Graph,
                                        RenderFrameContext&     Frame )
{
    if( Stage != RENDER_FEATURE_STAGE_FOG || !Frame.SceneRendered )
    {
        return TRUE;
    }

    auto const& Volumes = GetRenderPipeline().GetSubmission().GetVolumetrics();

    if( !Volumes.GetCount() )
    {
        return TRUE;
    }

    RenderView const*         pView   = Frame.GetView( 0 );
    view const*               pSource = pView ? pView->GetSourceView() : NULL;
    RenderResourceDesc const* pColor  = Graph.GetDesc( Frame.GetSceneColorTarget() );

    if( !pSource || !pColor || Frame.Desc.Views.ViewCount != 1 )
    {
        RENDER_THROW( "Volumetric", "volume pass requires a scene view and color target" );
        return FALSE;
    }

    RenderExtent const Extent = pColor->Extent;
    rdraw_viewport      Viewport;

    // The resource has not been allocated yet; scale the output viewport to the
    // scene extent. Retain projection signs: this engine mirrors view X.
    RenderExtent const Output = Frame.Desc.Output.Extent.IsValid() ? Frame.Desc.Output.Extent : Extent;
    Viewport          = pView->Viewport;
    Viewport.TopLeftX *= static_cast<f32>( Extent.Width ) / Output.Width;
    Viewport.TopLeftY *= static_cast<f32>( Extent.Height ) / Output.Height;
    Viewport.Width    *= static_cast<f32>( Extent.Width ) / Output.Width;
    Viewport.Height   *= static_cast<f32>( Extent.Height ) / Output.Height;

    matrix4 const& Projection = pSource->GetV2C();
    f32 const      TanX       = 1.0f / Projection( 0, 0 );
    f32 const      TanY       = 1.0f / Projection( 1, 1 );

    m_Passes.SetCount( Volumes.GetCount() );

    for( s32 Index = 0; Index < Volumes.GetCount(); ++Index )
    {
        volumetric_draw_desc const& Volume = Volumes[Index];
        shader_resource const*      pMask  = Volume.pTexture->GetShaderResource();

        if( !pMask )
        {
            RENDER_THROW( "Volumetric", "authored volume mask is not resident" );
            return FALSE;
        }

        PassData& Data = m_Passes[Index];
        static_assert( sizeof( Data.Uniforms ) == 272, "volume constants must match HLSL" );

        matrix4 WorldToLocal = Volume.LocalToWorld;

        if( !WorldToLocal.InvertSRT() )
        {
            RENDER_THROW( "Volumetric", "non-invertible volume transform" );
            return FALSE;
        }

        Data.Uniforms.ViewToLocal = WorldToLocal * pSource->GetV2W();
        vector3 const Camera = WorldToLocal * pSource->GetPosition();
        Data.Uniforms.CameraLocal.Set( Camera.GetX(), Camera.GetY(), Camera.GetZ(), 1.0f );
        Data.Uniforms.RayMulAdd.Set(
            2.0f * TanX * Extent.Width / Viewport.Width,
            -2.0f * TanY * Extent.Height / Viewport.Height,
            -TanX * ( 1.0f + 2.0f * Viewport.TopLeftX / Viewport.Width + Projection( 2, 0 ) ),
            TanY * ( 1.0f + 2.0f * Viewport.TopLeftY / Viewport.Height - Projection( 2, 1 ) ) );
        Data.Uniforms.Shape.Set( Volume.StartSize * 0.5f, Volume.EndSize * 0.5f,
                                Volume.Length, Volume.Intensity );
        Data.Uniforms.StartColor = Volume.StartColor;
        Data.Uniforms.EndColor   = Volume.EndColor;

        f32 NearZ, FarZ;
        pSource->GetZLimits( NearZ, FarZ );
        Data.Uniforms.DepthInfo.Set( NearZ, FarZ, 1.0f / Extent.Width, 1.0f / Extent.Height );
        Data.Uniforms.FadeParams.Set( Volume.EdgeSoftness, Volume.StartFade, Volume.EndFade, 0.0f );
        Data.Uniforms.LocalToShadow.Identity();
        Data.Uniforms.ShadowRect.Set( 0.0f, 0.0f, 1.0f, 1.0f );
        Data.Uniforms.ShadowInfo.Set( 0.0f, 0.0f, 0.0f, 0.0f );

        Data.pOwner      = this;
        Data.Multisample = pColor->SampleCount > 1;
        Data.Depth       = Data.Multisample ? Frame.SceneDepthMSAA : Frame.SceneDepth;
        Data.Mask        = Graph.ImportExternal( *pMask, "VolumetricMask" );

        // All reflected samplers need valid bindings even when shadowing is off.
        Data.Shadow = Data.Mask;

        ShadowMapAtlas const& Atlas = GetRenderPipeline().GetShadowMapAtlas();
        s32 const ShadowIndex = Volume.CastShadows && Frame.Desc.EnableShadows
                                    ? Atlas.FindVolumetricSource( Volume.OwnerGuid ) : -1;

        if( ShadowIndex >= 0 )
        {
            ShadowMapAtlas::ShadowSource const& Source = Atlas.GetSource( ShadowIndex );
            ShadowFeature const* pShadows = GetRenderPipeline().GetShadowFeature();
            RenderResourceHandle const ShadowDepth = pShadows ? pShadows->Backend().GetShadowDepthAtlasHandle()
                                                             : RenderResourceHandle();

            if( Source.AtlasWidth > 0 && ShadowDepth.IsValid() )
            {
                Data.Shadow                 = ShadowDepth;
                Data.Uniforms.LocalToShadow = Source.WorldToAtlas * Volume.LocalToWorld;

                f32 const InvSize = 1.0f / Atlas.GetAtlasSize();
                Data.Uniforms.ShadowRect.Set( ( Source.AtlasX + 0.5f ) * InvSize,
                                            ( Source.AtlasY + 0.5f ) * InvSize,
                                            ( Source.AtlasX + Source.AtlasWidth - 0.5f ) * InvSize,
                                            ( Source.AtlasY + Source.AtlasHeight - 0.5f ) * InvSize );
                Data.Uniforms.ShadowInfo.Set( 1.0f, 0.001f, InvSize, 0.0f );
            }
        }

        RenderPassDesc Pass;
        Pass.pDebugName  = "VolumetricRaymarch";
        Pass.Color       = Frame.GetSceneColorTarget();
        Pass.ColorLoadOp = RTARGET_LOAD_LOAD;

        RenderPassId const Id = Graph.AddPass( Pass, Execute, static_cast<PassData const*>( &Data ) );
        Graph.Read( Id, Data.Depth, Data.Multisample ? RENDER_RESOURCE_ACCESS_STORAGE_READ
                                                    : RENDER_RESOURCE_ACCESS_SAMPLED_READ );
        Graph.Read( Id, Data.Mask );
        Graph.Read( Id, Data.Shadow );

        if( !Graph.DeclareCompositePipeline( Id, COMPOSITE_BLEND_PREMULT_ADDITIVE_RGB,
                                            Data.Multisample ? &m_MultisampleShader : &m_PixelShader ) )
        {
            return FALSE;
        }
    }

    return !Graph.GetLastError();
}

//=========================================================================

xbool VolumetricFeature::Execute( RenderPassContext& Context, PassData const* pData )
{
    PassData const& Data    = *pData;
    shader const&  Shader  = Data.Multisample ? Data.pOwner->m_MultisampleShader : Data.pOwner->m_PixelShader;
    rtarget const*  pTarget = rtarget_GetCurrentTarget( 0 );
    render_pipeline_desc PipelineDesc;

    if( !pTarget || !composite_GetPipelineDesc( PipelineDesc, COMPOSITE_BLEND_PREMULT_ADDITIVE_RGB,
                                              pTarget->Desc.Format, RTARGET_FORMAT_COUNT,
                                              pTarget->Desc.SampleCount, &Shader ) )
    {
        return FALSE;
    }

    render_pipeline* pPipeline = composite_QueuePipeline( PipelineDesc );

    // Pipeline binding invalidates sampler/storage bindings. Bind it FIRST,
    // then the inputs, and draw directly with the composite fullscreen VS.
    if( !pPipeline || !*pPipeline || !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }

    shader_resource const* pDepth  = Data.Multisample ? Context.ReadStorageTexture( Data.Depth )
                                                   : Context.ReadTexture( Data.Depth );
    shader_resource const* pMask   = Context.ReadTexture( Data.Mask );
    shader_resource const* pShadow = Context.ReadTexture( Data.Shadow );

    if( !pDepth || !pMask || !pShadow ||
        !shader_PushUniformData( SHADER_STAGE_PIXEL, 0, &Data.Uniforms, sizeof( Data.Uniforms ) ) )
    {
        return FALSE;
    }

    if( Data.Multisample )
    {
        if( !shader_BindStorageTexture( Shader, SHADER_STAGE_PIXEL, "SceneDepthSource", pDepth ) )
        {
            return FALSE;
        }
    }
    else if( !shader_BindSampler( Shader, SHADER_STAGE_PIXEL, "SceneDepthSource", pDepth,
                                 &Data.pOwner->m_PointSampler ) )
    {
        return FALSE;
    }

    return shader_BindSampler( Shader, SHADER_STAGE_PIXEL, "VolumeShadowMap", pShadow,
                               &Data.pOwner->m_PointSampler ) &&
           shader_BindSampler( Shader, SHADER_STAGE_PIXEL, "VolumeMask", pMask,
                               &Data.pOwner->m_LinearSampler ) &&
           rdraw_Draw( 3 );
}

} // namespace render
