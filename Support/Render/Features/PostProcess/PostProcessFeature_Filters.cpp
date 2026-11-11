//=========================================================================
//
//  PostProcessFeature_Filters.cpp
//
//  Screen-space post-processing module.
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
// Constants
static const s32 kMipRampSampleCount      = 256;
static const f32 kPainBlurReferenceWidth  = 256.0f;
static const f32 kPainBlurReferenceHeight = 192.0f;

//=========================================================================

static xbool BindFilterConstants( const shader& ShaderData, const PostFilterConstants& Constants )
{
    return shader_PushUniformData( SHADER_STAGE_PIXEL, ShaderData.ResourceSlots[A51Resource_FilterParams], &Constants,
                                   sizeof( Constants ) );
}

//=========================================================================

static void BuildFilterConstants( PostFilterConstants& Constants,
                                  f32                  MotionIntensity,
                                  f32                  Zoom,
                                  radian               Angle,
                                  f32                  AlphaSub,
                                  f32                  AlphaScale,
                                  f32                  CenterU,
                                  f32                  CenterV )
{
    x_memset( &Constants, 0, sizeof( Constants ) );

    Constants.Params0.Set( x_clamp( MotionIntensity, 0.0f, 1.0f ), Zoom, x_sin( Angle ), x_cos( Angle ) );
    Constants.Params1.Set( AlphaSub / 255.0f, AlphaScale / 255.0f, 0.0f, 0.0f );
    Constants.Params6.Set( CenterU, CenterV, 0.0f, 0.0f );
}

//=========================================================================

static void BuildScreenWarpConstants( PostFilterConstants&        Constants,
                                      const PostScreenWarpParams& ScreenWarp,
                                      u32                         CopyWidth,
                                      u32                         CopyHeight,
                                      const rtarget*              pSource,
                                      const RenderFrameContext&   Frame,
                                      const RenderView*           pRenderView )
{
    x_memset( &Constants, 0, sizeof( Constants ) );

    Constants.Params2.Set( 0.0f, ( CopyWidth > 0 ) ? ( 1.0f / static_cast<f32>( CopyWidth ) ) : 0.0f,
                           ( CopyHeight > 0 ) ? ( 1.0f / static_cast<f32>( CopyHeight ) ) : 0.0f, 0.0f );

    if( !pSource )
    {
        return;
    }

    const view*    pView = pRenderView ? pRenderView->GetSourceView() : NULL;
    rdraw_viewport SceneViewport;

    if( !pView || !Frame.GetViewport( SceneViewport, *pSource, pRenderView->ViewIndex ) ||
        ( pRenderView->Viewport.Width <= 0.0f ) || ( pRenderView->Viewport.Height <= 0.0f ) )
    {
        return;
    }

    // Map view pixels to the scene render target.
    const f32 SceneScaleX = SceneViewport.Width / pRenderView->Viewport.Width;
    const f32 SceneScaleY = SceneViewport.Height / pRenderView->Viewport.Height;

    f32 NearZ, FarZ;
    pView->GetZLimits( NearZ, FarZ );
    Constants.Params5.Set( NearZ, FarZ, 0.0f, 0.0f );

    const vector3 Origin = pView->PointToScreen( vector3( 0.0f, 0.0f, 1.0f ), view::VIEW );
    const vector3 AxisX  = pView->PointToScreen( vector3( 1.0f, 0.0f, 1.0f ), view::VIEW );
    const vector3 AxisY  = pView->PointToScreen( vector3( 0.0f, 1.0f, 1.0f ), view::VIEW );
    const f32     ScaleX = ( AxisX.GetX() - Origin.GetX() ) * SceneScaleX;
    const f32     ScaleY = ( AxisY.GetY() - Origin.GetY() ) * SceneScaleY;

    if( ( x_abs( ScaleX ) < 0.001f ) || ( x_abs( ScaleY ) < 0.001f ) )
    {
        return;
    }

    Constants.Params6.Set( Origin.GetX() * SceneScaleX, Origin.GetY() * SceneScaleY, 1.0f / ScaleX, 1.0f / ScaleY );

    // Project visible warp spheres into the scene viewport.
    s32 WarpCount = 0;

    for( s32 i = 0; ( i < ScreenWarp.Count ) && ( WarpCount < MAX_POST_SCREEN_WARPS ); ++i )
    {
        if( pView->SphereInView( ScreenWarp.WorldPos[i], ScreenWarp.Radius[i], view::WORLD ) == view::VISIBLE_NONE )
        {
            continue;
        }

        const vector3 Center = pView->ConvertW2V( ScreenWarp.WorldPos[i] );
        const f32     Radius = ScreenWarp.Radius[i];

        if( Center.GetZ() + Radius <= NearZ )
        {
            continue;
        }

        const f32 ProjectionZ  = MAX( Center.GetZ(), NearZ );
        const f32 CenterX      = Constants.Params6.GetX() + Center.GetX() * ScaleX / ProjectionZ;
        const f32 CenterY      = Constants.Params6.GetY() + Center.GetY() * ScaleY / ProjectionZ;
        const f32 ScreenRadius = MAX( x_abs( ScaleX ), x_abs( ScaleY ) ) * Radius / MAX( Center.GetZ(), Radius );
        Constants.ScreenWarps[WarpCount].Set( CenterX, CenterY, ScreenRadius, ScreenWarp.Amount[i] );
        Constants.WarpSpheres[WarpCount].Set( Center.GetX(), Center.GetY(), Center.GetZ(), Radius );
        Constants.WarpSphereConstants[WarpCount / 4][WarpCount % 4] = Center.Dot( Center ) - Radius * Radius;
        WarpCount++;
    }

    Constants.Params2.GetX() = static_cast<f32>( WarpCount );
}
} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

const PostFilterConstants* PostProcessFeature::GetScreenWarpConstants( const RenderFrameContext& Frame,
                                                                       const RenderView*         pView,
                                                                       const rtarget&            Source )
{
    if( !pView )
    {
        return NULL;
    }

    for( s32 i = 0; i < m_PreparedWarpViews.GetCount(); ++i )
    {
        if( m_PreparedWarpViews[i].ViewIndex == pView->ViewIndex )
        {
            return &m_PreparedWarpViews[i].Constants;
        }
    }

    PreparedWarpView& Prepared = m_PreparedWarpViews.Append();
    Prepared.ViewIndex         = pView->ViewIndex;
    BuildScreenWarpConstants( Prepared.Constants, m_ScreenWarp, Source.Desc.Width, Source.Desc.Height, &Source, Frame,
                              pView );

    return &Prepared.Constants;
}

//=========================================================================
//  FILTER RESOURCE MANAGEMENT
//=========================================================================

PostProcessFeature::FilterResources::FilterResources()
{
    MotionBlurPS         = shader();
    MipReducePS          = shader();
    MipDownsamplePS      = shader();
    PainBlurPS           = shader();
    MipCompositePS       = shader();
    MipCompositeCustomPS = shader();
    RadialBlurPS         = shader();
    ScreenWarpPS         = shader();
    WarpMaskPS           = shader();
    WarpMaskBlurPS       = shader();
    NoisePS              = shader();
    ScreenFadePS         = shader();
    MipRampTexture       = vram_texture();
    LinearSampler        = rstate_sampler();
    PointSampler         = rstate_sampler();
    isHistoryValid       = FALSE;
}

//=========================================================================

xbool PostProcessFeature::FilterResources::Initialize( void )
{
    Shutdown();

    shader_LoadComboStage( MotionBlurPS, A51FixedPrograms[A51FixedProgram_post_filters_motion_blur].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_motion_blur].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( MipReducePS, A51FixedPrograms[A51FixedProgram_post_filters_mip_reduce].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_mip_reduce].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( MipDownsamplePS, A51FixedPrograms[A51FixedProgram_post_filters_mip_downsample].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_mip_downsample].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( PainBlurPS, A51FixedPrograms[A51FixedProgram_post_filters_pain_blur].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_pain_blur].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( MipCompositePS, A51FixedPrograms[A51FixedProgram_post_filters_mip_composite].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_mip_composite].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( MipCompositeCustomPS,
                           A51FixedPrograms[A51FixedProgram_post_filters_mip_composite_custom].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_mip_composite_custom].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( RadialBlurPS, A51FixedPrograms[A51FixedProgram_post_filters_radial_blur].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_radial_blur].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( ScreenWarpPS, A51FixedPrograms[A51FixedProgram_post_filters_screen_warp].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_screen_warp].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( WarpMaskPS, A51FixedPrograms[A51FixedProgram_post_filters_warp_mask].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_warp_mask].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( WarpMaskBlurPS, A51FixedPrograms[A51FixedProgram_post_filters_warp_mask_blur].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_warp_mask_blur].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( NoisePS, A51FixedPrograms[A51FixedProgram_post_filters_noise].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_noise].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( ScreenFadePS, A51FixedPrograms[A51FixedProgram_post_filters_screen_fade].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_post_filters_screen_fade].Key,
                           SHADER_STAGE_PIXEL );
    rstate_CreateSampler( LinearSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostFilterLinear" );
    rstate_CreateSampler( PointSampler, RSTATE_SAMPLER_PRESET_POINT_CLAMP, "PostFilterPoint" );

    if( !MotionBlurPS || !MipReducePS || !MipDownsamplePS || !PainBlurPS || !MipCompositePS || !MipCompositeCustomPS ||
        !RadialBlurPS || !ScreenWarpPS || !WarpMaskPS || !WarpMaskBlurPS || !NoisePS || !ScreenFadePS ||
        !LinearSampler || !PointSampler )
    {
        RENDER_THROW( "PostProcessFeature", "failed to initialize filter resources" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void PostProcessFeature::FilterResources::Shutdown( void )
{
    vram_DestroyTexture( MipRampTexture );
    rstate_DestroySampler( LinearSampler );
    rstate_DestroySampler( PointSampler );

    shader_Destroy( MotionBlurPS );
    shader_Destroy( MipReducePS );
    shader_Destroy( MipDownsamplePS );
    shader_Destroy( PainBlurPS );
    shader_Destroy( MipCompositePS );
    shader_Destroy( MipCompositeCustomPS );
    shader_Destroy( RadialBlurPS );
    shader_Destroy( ScreenWarpPS );
    shader_Destroy( WarpMaskPS );
    shader_Destroy( WarpMaskBlurPS );
    shader_Destroy( NoisePS );
    shader_Destroy( ScreenFadePS );

    isHistoryValid = FALSE;
}

//=========================================================================

void PostProcessFeature::FilterResources::InvalidateHistory( void )
{
    isHistoryValid = FALSE;
}

//=========================================================================

xbool PostProcessFeature::FilterResources::UpdateMipRampTexture( const xbitmap* pBitmap )
{
    if( !pBitmap )
    {
        return FALSE;
    }

    if( !vram_IsValid( MipRampTexture ) )
    {
        vram_texture_desc Desc;
        Desc.Width      = kMipRampSampleCount;
        Desc.Height     = 1;
        Desc.Format     = VRAM_TEXTURE_FORMAT_RGBA8;
        Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
        Desc.pDebugName = "PostFilterMipRamp";

        if( !vram_CreateTexture( MipRampTexture, Desc ) )
        {
            return FALSE;
        }
    }

    const s32 Width  = pBitmap->GetWidth();
    const s32 Height = pBitmap->GetHeight();
    const s32 Count  = Width * Height;

    if( Count <= 0 )
    {
        return FALSE;
    }

    u8 Ramp[kMipRampSampleCount * 4];
    x_memset( Ramp, 0, sizeof( Ramp ) );

    for( s32 i = 0; i < kMipRampSampleCount; ++i )
    {
        const f32    SampleT   = ( kMipRampSampleCount > 1 )
                                     ? ( static_cast<f32>( i ) / static_cast<f32>( kMipRampSampleCount - 1 ) )
                                     : 0.0f;
        const s32    SourceIx  = MIN( static_cast<s32>( SampleT * static_cast<f32>( Count - 1 ) + 0.5f ), Count - 1 );
        const s32    SourceX   = SourceIx % Width;
        const s32    SourceY   = SourceIx / Width;
        const xcolor ColorData = pBitmap->GetPixelColor( SourceX, SourceY );

        Ramp[i * 4 + 0] = ColorData.R;
        Ramp[i * 4 + 1] = ColorData.G;
        Ramp[i * 4 + 2] = ColorData.B;
        Ramp[i * 4 + 3] = ColorData.A;
    }

    vram_texture_upload_desc Upload;
    Upload.Region.Width  = kMipRampSampleCount;
    Upload.Region.Height = 1;
    Upload.Region.Depth  = 1;
    Upload.pData         = Ramp;
    Upload.Size          = kMipRampSampleCount * 4;
    Upload.RowPitch      = kMipRampSampleCount * 4;
    Upload.SlicePitch    = kMipRampSampleCount * 4;
    Upload.bCycle        = TRUE;

    return vram_UploadTexture( MipRampTexture, Upload );
}

//=========================================================================
//  EFFECT IMPLEMENTATIONS
//=========================================================================

xbool PostProcessFeature::ExecuteMotionBlur( const rtarget* pHistory )
{
    if( !m_FilterResources.MotionBlurPS )
    {
        return FALSE;
    }

    if( !m_FilterResources.isHistoryValid )
    {
        return TRUE;
    }

    if( !pHistory || !rtarget_HasShaderResource( *pHistory ) )
    {
        return FALSE;
    }

    PostFilterConstants Constants;
    BuildFilterConstants( Constants, m_MotionBlur.Intensity, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f );

    if( !BindFilterConstants( m_FilterResources.MotionBlurPS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pHistory, COMPOSITE_BLEND_ALPHA_RGB, 1.0f, &m_FilterResources.MotionBlurPS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource );
}

//=========================================================================

xbool PostProcessFeature::ExecuteRadialBlur( const rtarget*            pSource,
                                             const rtarget*            pBlurSource,
                                             const RenderFrameContext& Frame )
{
    if( !m_FilterResources.RadialBlurPS || !pSource || !pBlurSource || ( pSource->Desc.Width == 0 ) ||
        ( pSource->Desc.Height == 0 ) )
    {
        return FALSE;
    }

    rdraw_viewport SceneViewport;

    if( !Frame.GetViewport( SceneViewport, *pSource, 0 ) )
    {
        return FALSE;
    }

    const f32 CenterU = x_clamp(
        ( SceneViewport.TopLeftX + SceneViewport.Width * 0.5f ) / static_cast<f32>( pSource->Desc.Width ), 0.0f, 1.0f );
    const f32 CenterV =
        x_clamp( ( SceneViewport.TopLeftY + SceneViewport.Height * 0.5f ) / static_cast<f32>( pSource->Desc.Height ),
                 0.0f, 1.0f );

    if( !composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, NULL, RSTATE_SAMPLER_PRESET_POINT_CLAMP ) )
    {
        return FALSE;
    }

    PostFilterConstants Constants;
    BuildFilterConstants( Constants, 0.0f, m_RadialBlur.Zoom, m_RadialBlur.Angle, m_RadialBlur.AlphaSub,
                          m_RadialBlur.AlphaScale, CenterU, CenterV );

    if( !BindFilterConstants( m_FilterResources.RadialBlurPS, Constants ) )
    {
        return FALSE;
    }

    if( !composite_Blit( *pBlurSource, COMPOSITE_BLEND_ALPHA, 1.0f, &m_FilterResources.RadialBlurPS,
                         RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource ) )
    {
        return FALSE;
    }

    BuildFilterConstants( Constants, 0.0f, m_RadialBlur.Zoom, -m_RadialBlur.Angle, m_RadialBlur.AlphaSub,
                          m_RadialBlur.AlphaScale, CenterU, CenterV );

    if( !BindFilterConstants( m_FilterResources.RadialBlurPS, Constants ) )
    {
        return FALSE;
    }

    if( !composite_Blit( *pBlurSource, COMPOSITE_BLEND_ALPHA, 1.0f, &m_FilterResources.RadialBlurPS,
                         RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::ExecuteScreenWarps( const rtarget*            pSource,
                                              const rtarget*            pSceneDepth,
                                              const RenderFrameContext& Frame,
                                              const RenderView*         pRenderView,
                                              const rtarget*            pMask0,
                                              const rtarget*            pMask1 )
{
    if( !m_FilterResources.ScreenWarpPS || !pSource || !pSceneDepth || !rtarget_HasShaderResource( *pSceneDepth ) ||
        !pMask0 || !pMask1 || ( m_ScreenWarp.Count <= 0 ) )
    {
        return FALSE;
    }

    const PostFilterConstants* pConstants = GetScreenWarpConstants( Frame, pRenderView, *pSource );

    if( !pConstants || !BindFilterConstants( m_FilterResources.ScreenWarpPS, *pConstants ) )
    {
        return FALSE;
    }

    composite_extra_source Sources[3];
    Sources[0].ResourceSemantic = A51Resource_FilterSource1;
    Sources[0].pResource        = rtarget_GetShaderResource( *pSceneDepth );
    Sources[0].SamplerMode      = RSTATE_SAMPLER_PRESET_POINT_CLAMP;

    Sources[1].ResourceSemantic = A51Resource_FilterRamp;
    Sources[1].pResource        = rtarget_GetShaderResource( *pMask0 );
    Sources[1].SamplerMode      = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;

    Sources[2].ResourceSemantic = A51Resource_WarpMask1;
    Sources[2].pResource        = rtarget_GetShaderResource( *pMask1 );
    Sources[2].SamplerMode      = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &m_FilterResources.ScreenWarpPS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource, Sources, 3 );
}

//=========================================================================

xbool PostProcessFeature::ExecuteMipComposite( const rtarget* pMipTarget,
                                               const rtarget* pSceneColor,
                                               const rtarget* pSceneDepth )
{
    const s32 Palette = m_MipFilter.PaletteIndex;

    if( ( Palette < 0 ) || ( Palette >= 4 ) )
    {
        return FALSE;
    }

    const s32 Count = MIN( m_MipFilter.Count[Palette], MAX_POST_MIPS );

    if( !pMipTarget || !pSceneColor || !pSceneDepth || !rtarget_HasShaderResource( *pMipTarget ) ||
        !rtarget_HasShaderResource( *pSceneDepth ) )
    {
        return FALSE;
    }

    const xbool   BUseCustom  = ( m_MipFilter.Fn[Palette] == render::FALLOFF_CUSTOM );
    const shader& PixelShader = BUseCustom ? m_FilterResources.MipCompositeCustomPS : m_FilterResources.MipCompositePS;

    if( !PixelShader )
    {
        return FALSE;
    }

    if( BUseCustom )
    {
        if( !m_pMipTexture || !m_FilterResources.UpdateMipRampTexture( m_pMipTexture ) )
        {
            return FALSE;
        }
    }

    PostFilterConstants CbData;
    x_memset( &CbData, 0, sizeof( CbData ) );
    CbData.Params3.Set( m_MipFilter.Color[Palette].R / 128.0f, m_MipFilter.Color[Palette].G / 128.0f,
                        m_MipFilter.Color[Palette].B / 128.0f, 0.0f );
    CbData.Params4.Set( static_cast<f32>( m_MipFilter.Fn[Palette] ), m_MipFilter.Param1[Palette],
                        m_MipFilter.Param2[Palette], m_MipFilter.Offset[Palette] );
    CbData.Params5.Set( m_PostNearZ, m_PostFarZ,
                        ( pSceneColor->Desc.Width > 0 ) ? ( 1.0f / static_cast<f32>( pSceneColor->Desc.Width ) ) : 0.0f,
                        ( pSceneColor->Desc.Height > 0 ) ? ( 1.0f / static_cast<f32>( pSceneColor->Desc.Height ) )
                                                         : 0.0f );
    CbData.Params6.Set( static_cast<f32>( Count ), 0.0f, 0.0f, 0.0f );

    if( !BindFilterConstants( PixelShader, CbData ) )
    {
        return FALSE;
    }

    composite_extra_source ExtraSources[2];
    u32                    ExtraSourceCount = 1;
    ExtraSources[0].ResourceSemantic        = A51Resource_FilterSource1;
    ExtraSources[0].pResource               = rtarget_GetShaderResource( *pSceneDepth );
    ExtraSources[0].SamplerMode             = RSTATE_SAMPLER_PRESET_POINT_CLAMP;

    if( BUseCustom )
    {
        ExtraSources[1].ResourceSemantic = A51Resource_FilterRamp;
        ExtraSources[1].pResource        = vram_GetShaderResource( m_FilterResources.MipRampTexture );
        ExtraSources[1].SamplerMode      = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;

        if( !ExtraSources[1].pResource )
        {
            return FALSE;
        }

        ExtraSourceCount = 2;
    }

    return composite_Blit( *pMipTarget, COMPOSITE_BLEND_ALPHA_RGB, 1.0f, &PixelShader,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource, ExtraSources, ExtraSourceCount );
}

//=========================================================================

xbool PostProcessFeature::ExecuteNoiseFilter( void )
{
    if( !m_FilterResources.NoisePS )
    {
        return FALSE;
    }

    PostFilterConstants CbData;
    x_memset( &CbData, 0, sizeof( CbData ) );

    const f32 ANorm = m_Simple.NoiseColor.A / 255.0f;
    CbData.Params3.Set( m_Simple.NoiseColor.R / 255.0f, m_Simple.NoiseColor.G / 255.0f, m_Simple.NoiseColor.B / 255.0f,
                        ANorm * ANorm * ANorm );

    CbData.Params6.Set( static_cast<f32>( x_rand() ), static_cast<f32>( x_rand() ), 0.0f, 0.0f );

    if( !BindFilterConstants( m_FilterResources.NoisePS, CbData ) )
    {
        return FALSE;
    }

    return composite_Draw( m_FilterResources.NoisePS, COMPOSITE_BLEND_ALPHA_RGB );
}

//=========================================================================
//  SCREEN FADE
//=========================================================================

xbool PostProcessFeature::DeclareScreenFadePass( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !m_Requests.DoScreenFade )
    {
        return TRUE;
    }

    if( !m_FilterResources.ScreenFadePS )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade was requested without a valid shader" );
        return FALSE;
    }

    const RenderResourceDesc* pFinalDesc = Graph.GetDesc( Frame.FinalColor );

    if( !pFinalDesc )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade composition descriptor is unavailable" );
        return FALSE;
    }

    RenderResourceDesc SourceData = *pFinalDesc;
    SourceData.Lifetime           = RENDER_RESOURCE_TRANSIENT;
    SourceData.PersistentKey      = 0;
    SourceData.PreserveContents   = FALSE;
    SourceData.Usage = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_DST;
    SourceData.pDebugName = "ScreenFadeSource";
    m_ScreenFadeSource    = Graph.CreateResource( SourceData );

    if( !m_ScreenFadeSource.IsValid() )
    {
        RENDER_THROW( "PostProcessFeature", "failed to allocate screen fade source" );
        return FALSE;
    }

    RenderPassDesc Capture;
    Capture.pDebugName  = "ScreenFadeCapture";
    Capture.Type        = RENDER_PASS_COPY;
    Capture.Source      = Frame.FinalColor;
    Capture.Destination = m_ScreenFadeSource;

    const RenderPassId CapturePass = Graph.AddPass( Capture, ExecuteScreenFadeCapture, this );

    if( CapturePass == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare screen fade capture pass" );
        return FALSE;
    }

    RenderPassDesc Desc;
    Desc.pDebugName  = "ScreenFade";
    Desc.Color       = Frame.FinalColor;
    Desc.ColorLoadOp = RTARGET_LOAD_LOAD;

    const RenderPassId PassData = Graph.AddPass( Desc, ExecuteScreenFadePass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare screen fade pass" );
        return FALSE;
    }

    Graph.Read( PassData, m_ScreenFadeSource );

    if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_ALPHA, &m_FilterResources.ScreenFadePS ) )
    {
        return FALSE;
    }

    return Graph.GetLastError() == NULL;
}

//=========================================================================

xbool PostProcessFeature::ExecuteScreenFadeCapture( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade capture received a null feature" );
        return FALSE;
    }

    return Context.Copy( pPost->m_ScreenFadeSource, Context.GetFrame().FinalColor );
}

//=========================================================================

xbool PostProcessFeature::ExecuteScreenFadePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pSourceTarget = pPost ? Context.Read( pPost->m_ScreenFadeSource ) : NULL;
    const rtarget* pBackBuffer   = Context.Write( Context.GetFrame().FinalColor );

    if( !pPost )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade received a null feature" );
        return FALSE;
    }

    if( !pPost->m_FilterResources.ScreenFadePS )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade shader is unavailable during execution" );
        return FALSE;
    }

    if( !pSourceTarget || !pBackBuffer || ( pSourceTarget == pBackBuffer ) ||
        !rtarget_HasShaderResource( *pSourceTarget ) || !rtarget_HasRenderTarget( *pBackBuffer ) )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade source or destination resource is invalid" );
        return FALSE;
    }

    PostFilterConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.Params3.Set( pPost->m_Simple.FadeColor.R / 255.0f, pPost->m_Simple.FadeColor.G / 255.0f,
                           pPost->m_Simple.FadeColor.B / 255.0f, pPost->m_Simple.FadeColor.A / 255.0f );

    if( !BindFilterConstants( pPost->m_FilterResources.ScreenFadePS, Constants ) )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade constants upload failed" );
        return FALSE;
    }

    rdraw_scissor ViewScissor = {};
    ViewScissor.X             = pPost->m_PostViewL;
    ViewScissor.Y             = pPost->m_PostViewT;
    ViewScissor.Width         = pPost->m_PostViewR - pPost->m_PostViewL;
    ViewScissor.Height        = pPost->m_PostViewB - pPost->m_PostViewT;

    if( !rdraw_SetScissor( ViewScissor ) )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade viewport scissor setup failed" );
        return FALSE;
    }

    const xbool BlitOk = composite_Blit( *pSourceTarget, COMPOSITE_BLEND_ALPHA, 1.0f,
                                         &pPost->m_FilterResources.ScreenFadePS, RSTATE_SAMPLER_PRESET_POINT_CLAMP );

    rdraw_scissor FullScissor = {};
    FullScissor.X             = 0;
    FullScissor.Y             = 0;
    FullScissor.Width         = static_cast<s32>( pBackBuffer->Desc.Width );
    FullScissor.Height        = static_cast<s32>( pBackBuffer->Desc.Height );

    if( !rdraw_SetScissor( FullScissor ) )
    {
        RENDER_THROW( "PostProcessFeature", "screen fade failed to restore full scissor" );
        return FALSE;
    }

    return BlitOk;
}

//=========================================================================
//  SUPPORT FUNCTIONS
//=========================================================================

void PostProcessFeature::BuildMipPalette( render::post_falloff_fn Fn,
                                          xcolor                  ColorData,
                                          f32                     Param1,
                                          f32                     Param2,
                                          s32                     PaletteIndex )
{
    if( ( m_MipFilter.Fn[PaletteIndex] == Fn ) && ( m_MipFilter.Param1[PaletteIndex] == Param1 ) &&
        ( m_MipFilter.Param2[PaletteIndex] == Param2 ) && ( m_MipFilter.Color[PaletteIndex] == ColorData ) )
    {
        return;
    }

    m_MipFilter.Fn[PaletteIndex]     = Fn;
    m_MipFilter.Param1[PaletteIndex] = Param1;
    m_MipFilter.Param2[PaletteIndex] = Param2;
    m_MipFilter.Color[PaletteIndex]  = ColorData;
    m_MipFilter.PaletteIndex         = PaletteIndex;
}

//=========================================================================

RenderResourceHandle PostProcessFeature::CreateFilterTarget( FrameGraphBuilder&  Graph,
                                                             const RenderExtent& Extent,
                                                             rtarget_format      Format,
                                                             const char*         pDebugName )
{
    RenderResourceDesc Desc;
    Desc.Extent = Extent;
    Desc.Format = Format;
    Desc.Usage  = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_SRC |
                 RENDER_RESOURCE_USAGE_COPY_DST;
    Desc.Lifetime   = RENDER_RESOURCE_TRANSIENT;
    Desc.pDebugName = pDebugName;

    return Graph.CreateResource( Desc );
}

//=========================================================================
//  FILTER GRAPH DECLARATION
//=========================================================================

xbool PostProcessFeature::DeclarePainFilterPasses( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    const s32 Palette = m_PainFilter.PaletteIndex;

    if( Palette < 0 )
    {
        return TRUE;
    }

    if( ( Palette >= 4 ) || ( m_PainFilter.Count[Palette] <= 0 ) )
    {
        return TRUE;
    }

    const RenderResourceDesc* pSceneColor = Graph.GetDesc( Frame.SceneColor );

    if( !pSceneColor )
    {
        return FALSE;
    }

    if( x_abs( m_PainFilter.Offset[Palette] ) < 0.001f )
    {
        return TRUE;
    }

    const RenderExtent Extent = pSceneColor->Extent;
    m_FilterPain              = CreateFilterTarget( Graph, Extent, pSceneColor->Format, "PostFilterPain" );
    m_FilterPainScratch       = CreateFilterTarget( Graph, Extent, pSceneColor->Format, "PostFilterPainScratch" );

    if( !m_FilterPain.IsValid() || !m_FilterPainScratch.IsValid() )
    {
        return FALSE;
    }

    RenderPassDesc Downsample;
    Downsample.pDebugName = "PainDownsample";
    Downsample.Color      = m_FilterPain;

    RenderPassId PassData = Graph.AddPass( Downsample, ExecutePainDownsamplePass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare PainDownsample pass" );
        return FALSE;
    }

    Graph.Read( PassData, Frame.SceneColor );

    if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_FilterResources.MipDownsamplePS ) )
    {
        return FALSE;
    }

    RenderPassDesc Horizontal;
    Horizontal.pDebugName = "PainBlurHorizontal";
    Horizontal.Color      = m_FilterPainScratch;
    PassData              = Graph.AddPass( Horizontal, ExecutePainHorizontalPass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare PainBlurHorizontal pass" );
        return FALSE;
    }

    Graph.Read( PassData, m_FilterPain );

    if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_FilterResources.PainBlurPS ) )
    {
        return FALSE;
    }

    RenderPassDesc Vertical;
    Vertical.pDebugName  = "PainBlurVertical";
    Vertical.Color       = Frame.SceneColor;
    Vertical.ColorLoadOp = RTARGET_LOAD_LOAD;
    PassData             = Graph.AddPass( Vertical, ExecutePainVerticalPass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare PainBlurVertical pass" );
        return FALSE;
    }

    Graph.Read( PassData, m_FilterPainScratch );

    if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY_RGB, &m_FilterResources.PainBlurPS ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool PostProcessFeature::DeclareMipFilterPasses( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    const s32 Palette = m_MipFilter.PaletteIndex;

    if( ( Palette < 0 ) || ( Palette >= 4 ) )
    {
        return FALSE;
    }

    const s32 Count = MIN( m_MipFilter.Count[Palette], MAX_POST_MIPS );

    if( Count <= 0 )
    {
        return TRUE;
    }

    const RenderResourceDesc* pSceneColor = Graph.GetDesc( Frame.SceneColor );

    if( !pSceneColor )
    {
        return FALSE;
    }

    m_nFilterMips     = Count;
    m_FilterMipSource = Frame.SceneColor;

    u32                Width                 = MAX( ( pSceneColor->Extent.Width + 1u ) / 2u, 1u );
    u32                Height                = MAX( ( pSceneColor->Extent.Height + 1u ) / 2u, 1u );
    static const char* sNames[MAX_POST_MIPS] = { "PostFilterMip0", "PostFilterMip1", "PostFilterMip2" };

    for( s32 i = 0; i < Count; ++i )
    {
        m_FilterMip[i] = CreateFilterTarget( Graph, RenderExtent( Width, Height ), pSceneColor->Format, sNames[i] );

        if( !m_FilterMip[i].IsValid() )
        {
            return FALSE;
        }

        RenderPassDesc Downsample;
        Downsample.pDebugName = sNames[i];
        Downsample.Color      = m_FilterMip[i];

        const RenderPassId PassData = Graph.AddPass( Downsample, ExecuteMipDownsamplePass, this );

        if( PassData == INVALID_RENDER_PASS )
        {
            RENDER_THROW( "PostProcessFeature", "failed to declare mip pass '%s'", sNames[i] );
            return FALSE;
        }

        Graph.Read( PassData, ( i == 0 ) ? Frame.SceneColor : m_FilterMip[i - 1] );

        if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_FilterResources.MipReducePS ) )
        {
            return FALSE;
        }

        Width  = MAX( ( Width + 1u ) / 2u, 1u );
        Height = MAX( ( Height + 1u ) / 2u, 1u );
    }

    RenderPassDesc Composite;
    Composite.pDebugName  = "MipComposite";
    Composite.Color       = Frame.SceneColor;
    Composite.ColorLoadOp = RTARGET_LOAD_LOAD;

    const RenderPassId PassData = Graph.AddPass( Composite, ExecuteMipCompositePass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "PostProcessFeature", "failed to declare MipComposite pass" );
        return FALSE;
    }

    Graph.Read( PassData, m_FilterMip[Count - 1] );
    Graph.Read( PassData, vram_GetShaderResource( m_FilterResources.MipRampTexture ) );
    Graph.Read( PassData, Frame.SceneDepth );

    const shader* pMipCompositeShader = &m_FilterResources.MipCompositePS;

    if( ( Palette >= 0 ) && ( Palette < 4 ) && ( m_MipFilter.Fn[Palette] == render::FALLOFF_CUSTOM ) )
    {
        pMipCompositeShader = &m_FilterResources.MipCompositeCustomPS;
    }

    if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_ALPHA_RGB, pMipCompositeShader ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  WARP MASKS
//=========================================================================

xbool PostProcessFeature::ExecuteWarpMaskPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost )
    {
        return FALSE;
    }

    s32 Group = -1;

    for( s32 i = 0; i < 2; ++i )
    {
        if( Context.GetPass().Color.Index == pPost->m_WarpMaskRaw[i].Index )
        {
            Group = i;
        }
    }

    const rtarget* pDepth = Context.Read( Context.GetFrame().SceneDepth );

    if( ( Group < 0 ) || !pDepth || !Context.Write( pPost->m_WarpMaskRaw[Group] ) )
    {
        return FALSE;
    }

    const PostFilterConstants* pPrepared =
        pPost->GetScreenWarpConstants( Context.GetFrame(), Context.GetView(), *pDepth );

    if( !pPrepared )
    {
        return FALSE;
    }

    PostFilterConstants Constants = *pPrepared;
    Constants.Params4.GetX()      = static_cast<f32>( Group * 4 );

    if( !BindFilterConstants( pPost->m_FilterResources.WarpMaskPS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pDepth, COMPOSITE_BLEND_COPY, 1.0f, &pPost->m_FilterResources.WarpMaskPS,
                           RSTATE_SAMPLER_PRESET_POINT_CLAMP, A51Resource_PostSource );
}

//=========================================================================

xbool PostProcessFeature::ExecuteWarpMaskBlurPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost )
    {
        return FALSE;
    }

    RenderResourceHandle Source;
    xbool                Horizontal = FALSE;

    for( s32 i = 0; i < 2; ++i )
    {
        if( Context.GetPass().Color.Index == pPost->m_WarpMaskScratch[i].Index )
        {
            Source     = pPost->m_WarpMaskRaw[i];
            Horizontal = TRUE;
            break;
        }
        else if( Context.GetPass().Color.Index == pPost->m_WarpMask[i].Index )
        {
            Source = pPost->m_WarpMaskScratch[i];
            break;
        }
    }

    const rtarget* pSource = Source.IsValid() ? Context.Read( Source ) : NULL;

    if( !pSource || !Context.Write( Context.GetPass().Color ) )
    {
        return FALSE;
    }

    // Keep the blur radius in output pixels when RenderScale changes.
    const RenderExtent ExtentData = Context.GetFrame().Desc.Output.Extent;
    f32                Width = ExtentData.IsValid() ? static_cast<f32>( ExtentData.Width ) : pSource->Desc.Width * 2.0f;
    f32 Height = ExtentData.IsValid() ? static_cast<f32>( ExtentData.Height ) : pSource->Desc.Height * 2.0f;
    PostFilterConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.Params4.Set( 0.0f, Horizontal ? 2.0f / Width : 0.0f, Horizontal ? 0.0f : 2.0f / Height, 0.0f );

    if( !BindFilterConstants( pPost->m_FilterResources.WarpMaskBlurPS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &pPost->m_FilterResources.WarpMaskBlurPS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource );
}

//=========================================================================
//  WARP GRAPH DECLARATION
//=========================================================================

xbool PostProcessFeature::DeclareWarpPasses( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( m_ScreenWarp.Count > 0 && m_PreparedWarpViews.GetCapacity() < static_cast<s32>( Frame.Desc.Views.ViewCount ) )
    {
        m_PreparedWarpViews.SetCapacity( Frame.Desc.Views.ViewCount );
    }

    const RenderResourceDesc* pSceneColor = Graph.GetDesc( Frame.SceneColor );

    if( !pSceneColor )
    {
        return FALSE;
    }

    const RenderExtent ExtentData = pSceneColor->Extent;
    m_FilterPost[0]               = CreateFilterTarget( Graph, ExtentData, pSceneColor->Format, "PostFilterPost0" );

    if( !m_FilterPost[0].IsValid() )
    {
        return FALSE;
    }

    m_FilterPost[1] = RenderResourceHandle();

    if( m_Requests.DoRadialBlur && m_ScreenWarp.Count > 0 )
    {
        m_FilterPost[1] = CreateFilterTarget( Graph, ExtentData, pSceneColor->Format, "PostFilterPost1" );

        if( !m_FilterPost[1].IsValid() )
        {
            return FALSE;
        }
    }

    RenderPassId         PassData = INVALID_RENDER_PASS;
    RenderResourceHandle Current  = Frame.SceneColor;

    if( m_Requests.DoRadialBlur )
    {
        m_FilterRadialMip = CreateFilterTarget( Graph,
                                                RenderExtent( MAX( ( ExtentData.Width * 2u + 2u ) / 5u, 1u ),
                                                              MAX( ( ExtentData.Height * 2u + 2u ) / 5u, 1u ) ),
                                                pSceneColor->Format, "PostRadialMip" );

        if( !m_FilterRadialMip.IsValid() )
        {
            return FALSE;
        }

        RenderPassDesc Mip;
        Mip.pDebugName = "RadialMip";
        Mip.Color      = m_FilterRadialMip;
        PassData       = Graph.AddPass( Mip, ExecuteRadialMipPass, this );

        if( PassData == INVALID_RENDER_PASS )
        {
            RENDER_THROW( "PostProcessFeature", "failed to declare RadialMip pass" );
            return FALSE;
        }

        Graph.Read( PassData, Frame.SceneColor );

        if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_FilterResources.MipDownsamplePS ) )
        {
            return FALSE;
        }

        RenderPassDesc Radial;
        Radial.pDebugName = "RadialBlur";
        Radial.Color      = m_FilterPost[0];
        PassData          = Graph.AddPass( Radial, ExecuteRadialPass, this );

        if( PassData == INVALID_RENDER_PASS )
        {
            RENDER_THROW( "PostProcessFeature", "failed to declare RadialBlur pass" );
            return FALSE;
        }

        Graph.Read( PassData, Frame.SceneColor );
        Graph.Read( PassData, m_FilterRadialMip );

        if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY ) ||
            !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_ALPHA, &m_FilterResources.RadialBlurPS ) )
        {
            return FALSE;
        }

        Current = m_FilterPost[0];
    }

    if( m_ScreenWarp.Count > 0 )
    {
        const s32            Groups     = ( m_ScreenWarp.Count + 3 ) / 4;
        const rtarget_format MaskFormat = ( m_ScreenWarp.Count == 1 ) ? RTARGET_FORMAT_R8 : RTARGET_FORMAT_RGBA8;
        const RenderExtent   MaskExtent( MAX( ( ExtentData.Width + 1u ) / 2u, 1u ),
                                         MAX( ( ExtentData.Height + 1u ) / 2u, 1u ) );

        for( s32 i = 0; i < Groups; ++i )
        {
            m_WarpMaskRaw[i]     = CreateFilterTarget( Graph, MaskExtent, MaskFormat, "WarpMaskRaw" );
            m_WarpMaskScratch[i] = CreateFilterTarget( Graph, MaskExtent, MaskFormat, "WarpMaskScratch" );
            m_WarpMask[i]        = CreateFilterTarget( Graph, MaskExtent, MaskFormat, "WarpMask" );

            if( !m_WarpMaskRaw[i].IsValid() || !m_WarpMaskScratch[i].IsValid() || !m_WarpMask[i].IsValid() )
            {
                return FALSE;
            }

            RenderPassDesc Mask;
            Mask.pDebugName = "WarpMask";
            Mask.Color      = m_WarpMaskRaw[i];
            PassData        = Graph.AddPass( Mask, ExecuteWarpMaskPass, this );

            if( PassData == INVALID_RENDER_PASS )
            {
                return FALSE;
            }

            Graph.Read( PassData, Frame.SceneDepth );

            if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_FilterResources.WarpMaskPS ) )
            {
                return FALSE;
            }

            for( s32 Axis = 0; Axis < 2; ++Axis )
            {
                RenderPassDesc Blur;
                Blur.pDebugName = Axis ? "WarpMaskBlurY" : "WarpMaskBlurX";
                Blur.Color      = Axis ? m_WarpMask[i] : m_WarpMaskScratch[i];
                PassData        = Graph.AddPass( Blur, ExecuteWarpMaskBlurPass, this );

                if( PassData == INVALID_RENDER_PASS )
                {
                    return FALSE;
                }

                Graph.Read( PassData, Axis ? m_WarpMaskScratch[i] : m_WarpMaskRaw[i] );

                if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY,
                                                     &m_FilterResources.WarpMaskBlurPS ) )
                {
                    return FALSE;
                }
            }
        }

        if( Groups == 1 )
        {
            m_WarpMask[1] = m_WarpMask[0];
        }

        m_FilterWarpResult = ( Current.Index == m_FilterPost[0].Index ) ? m_FilterPost[1] : m_FilterPost[0];
        m_FilterWarpSource = Current;

        RenderPassDesc WarpData;
        WarpData.pDebugName = "Warp";
        WarpData.Color      = m_FilterWarpResult;
        PassData            = Graph.AddPass( WarpData, ExecuteWarpPass, this );

        if( PassData == INVALID_RENDER_PASS )
        {
            RENDER_THROW( "PostProcessFeature", "failed to declare Warp pass" );
            return FALSE;
        }

        Graph.Read( PassData, Current );
        Graph.Read( PassData, Frame.SceneDepth );
        Graph.Read( PassData, m_WarpMask[0] );

        if( m_WarpMask[1].Index != m_WarpMask[0].Index )
        {
            Graph.Read( PassData, m_WarpMask[1] );
        }

        if( !Graph.DeclareCompositePipeline( PassData, COMPOSITE_BLEND_COPY, &m_FilterResources.ScreenWarpPS ) )
        {
            return FALSE;
        }

        Current = m_FilterWarpResult;
    }

    m_FilterWarpResult = Current;

    return TRUE;
}

//=========================================================================
//  FILTER PASS EXECUTION
//=========================================================================

xbool PostProcessFeature::ExecuteMipDownsamplePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost || !Context.Write( Context.GetPass().Color ) )
    {
        return FALSE;
    }

    s32 IndexData = -1;

    for( s32 i = 0; i < pPost->m_nFilterMips; ++i )
    {
        if( pPost->m_FilterMip[i].Index == Context.GetPass().Color.Index )
        {
            IndexData = i;
        }
    }

    if( IndexData < 0 )
    {
        return FALSE;
    }

    const RenderResourceHandle Source =
        ( IndexData == 0 ) ? pPost->m_FilterMipSource : pPost->m_FilterMip[IndexData - 1];
    const rtarget* pSource = Context.Read( Source );

    if( !pSource )
    {
        return FALSE;
    }

    PostFilterConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.Params5.Set( 0.0f, 0.0f, 1.0f / static_cast<f32>( pSource->Desc.Width ),
                           1.0f / static_cast<f32>( pSource->Desc.Height ) );

    if( !pPost->m_FilterResources.MipReducePS ||
        !BindFilterConstants( pPost->m_FilterResources.MipReducePS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &pPost->m_FilterResources.MipReducePS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource );
}

//=========================================================================

xbool PostProcessFeature::ExecuteRadialMipPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pSource = pPost ? Context.Read( Context.GetFrame().SceneColor ) : NULL;

    if( !pPost || !pSource || !Context.Write( pPost->m_FilterRadialMip ) || !pPost->m_FilterResources.MipDownsamplePS )
    {
        return FALSE;
    }

    PostFilterConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.Params5.Set( 0.0f, 0.0f, 1.0f / static_cast<f32>( pSource->Desc.Width ),
                           1.0f / static_cast<f32>( pSource->Desc.Height ) );

    if( !BindFilterConstants( pPost->m_FilterResources.MipDownsamplePS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &pPost->m_FilterResources.MipDownsamplePS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource );
}

//=========================================================================

xbool PostProcessFeature::ExecutePainDownsamplePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pSource = Context.Read( Context.GetFrame().SceneColor );

    if( !pPost || !pSource || !Context.Write( pPost->m_FilterPain ) || !pPost->m_FilterResources.MipDownsamplePS )
    {
        return FALSE;
    }

    PostFilterConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.Params5.Set( 0.0f, 0.0f, 1.0f / static_cast<f32>( pSource->Desc.Width ),
                           1.0f / static_cast<f32>( pSource->Desc.Height ) );

    if( !BindFilterConstants( pPost->m_FilterResources.MipDownsamplePS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &pPost->m_FilterResources.MipDownsamplePS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource );
}

//=========================================================================

xbool PostProcessFeature::ExecutePainHorizontalPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pSource = pPost ? Context.Read( pPost->m_FilterPain ) : NULL;
    const s32      Palette = pPost ? pPost->m_PainFilter.PaletteIndex : -1;

    if( !pPost || !pSource || ( Palette < 0 ) || !Context.Write( pPost->m_FilterPainScratch ) )
    {
        return FALSE;
    }

    const xcolor&       ColorData = pPost->m_PainFilter.Color[Palette];
    PostFilterConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.Params3.Set( ColorData.R / 255.0f, ColorData.G / 255.0f, ColorData.B / 255.0f,
                           static_cast<f32>( ColorData.A / 2 ) / 255.0f );
    Constants.Params5.Set( 0.0f, 0.0f, pPost->m_PainFilter.Offset[Palette] / ( kPainBlurReferenceWidth * 8.0f ), 0.0f );

    if( !BindFilterConstants( pPost->m_FilterResources.PainBlurPS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, &pPost->m_FilterResources.PainBlurPS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource );
}

//=========================================================================

xbool PostProcessFeature::ExecutePainVerticalPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pSource = pPost ? Context.Read( pPost->m_FilterPainScratch ) : NULL;
    const s32      Palette = pPost ? pPost->m_PainFilter.PaletteIndex : -1;

    if( !pPost || !pSource || ( Palette < 0 ) || !Context.Write( Context.GetFrame().SceneColor ) )
    {
        return FALSE;
    }

    const xcolor&       ColorData = pPost->m_PainFilter.Color[Palette];
    PostFilterConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.Params3.Set( ColorData.R / 255.0f, ColorData.G / 255.0f, ColorData.B / 255.0f,
                           static_cast<f32>( ColorData.A / 2 ) / 255.0f );
    Constants.Params5.Set( 0.0f, 0.0f, 0.0f,
                           pPost->m_PainFilter.Offset[Palette] / ( kPainBlurReferenceHeight * 8.0f ) );
    Constants.Params6.Set( 1.0f, 0.0f, 0.0f, 0.0f );

    if( !BindFilterConstants( pPost->m_FilterResources.PainBlurPS, Constants ) )
    {
        return FALSE;
    }

    return composite_Blit( *pSource, COMPOSITE_BLEND_COPY_RGB, 1.0f, &pPost->m_FilterResources.PainBlurPS,
                           RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, A51Resource_PostSource );
}

//=========================================================================

xbool PostProcessFeature::ExecuteMipCompositePass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    const rtarget* pSceneColor = Context.Write( Context.GetFrame().SceneColor );

    if( !pPost || ( pPost->m_nFilterMips <= 0 ) || !pSceneColor )
    {
        return FALSE;
    }

    return pPost->ExecuteMipComposite( Context.Read( pPost->m_FilterMip[pPost->m_nFilterMips - 1] ), pSceneColor,
                                       Context.Read( Context.GetFrame().SceneDepth ) );
}

//=========================================================================
//  TEMPORAL HISTORY
//=========================================================================

xbool PostProcessFeature::DeclareFilterHistoryPass( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !m_hasMotionBlur || !Frame.Desc.EnableTemporalHistory )
    {
        return TRUE;
    }

    const RenderResourceDesc* pTarget = Graph.GetDesc( Frame.ScenePostColor );

    if( !pTarget )
    {
        return FALSE;
    }

    if( m_FilterHistoryKey == 0 )
    {
        m_FilterHistoryKey = RenderHistoryKey( 0x504f535448495354ull, Frame );
    }

    m_FilterResources.isHistoryValid =
        PrepareTemporalHistory( m_FilterHistoryKey, pTarget->Extent, pTarget->Format, Frame );

    if( !m_FilterHistoryHandle.IsValid() )
    {
        RenderResourceDesc History;
        History.Extent   = pTarget->Extent;
        History.Format   = pTarget->Format;
        History.Usage    = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_DST;
        History.Lifetime = RENDER_RESOURCE_PERSISTENT;
        History.PersistentKey = m_FilterHistoryKey;
        History.pDebugName    = "PostFilterHistory";
        m_FilterHistoryHandle = Graph.CreateResource( History );
    }

    if( !m_FilterHistoryHandle.IsValid() )
    {
        return FALSE;
    }

    RenderPassDesc DescData;
    DescData.pDebugName  = "UpdatePostHistory";
    DescData.Type        = RENDER_PASS_COPY;
    DescData.Source      = Frame.ScenePostColor;
    DescData.Destination = m_FilterHistoryHandle;

    const RenderPassId PassData = Graph.AddPass( DescData, ExecuteHistoryPass, this );

    if( PassData != INVALID_RENDER_PASS )
    {
        Graph.KeepAlive( PassData );
    }

    return PassData != INVALID_RENDER_PASS;
}

//=========================================================================

xbool PostProcessFeature::ExecuteHistoryPass( RenderPassContext& Context, PostProcessFeature* pPost )
{
    if( !pPost || !Context.Copy( pPost->m_FilterHistoryHandle, Context.GetPass().Source ) )
    {
        return FALSE;
    }

    pPost->m_FilterResources.isHistoryValid = TRUE;

    const RenderResourceDesc* pSceneColor = Context.GetFrame().pGraph->GetDesc( Context.GetPass().Source );

    if( !pSceneColor )
    {
        return FALSE;
    }

    pPost->MarkTemporalHistoryValid( pPost->m_FilterHistoryKey, pSceneColor->Extent, pSceneColor->Format,
                                     Context.GetFrame() );

    return TRUE;
}

} // namespace render
