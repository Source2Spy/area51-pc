//=========================================================================
//
//  PostProcessFeature_Fog.cpp
//
//  Fog post-processing module.
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
#include "../../LeastSquares/LeastSquares.hpp"

namespace render
{

//=========================================================================
//  FILE-LOCAL TYPES AND HELPERS
//=========================================================================

namespace
{
// Constants
static const s32 kFogSourcePaletteCount    = 64;
static const s32 kFogPaletteCount          = 256;
static const s32 kFogGeneratedPaletteIndex = 2;
static const f32 kDepthBufferZScale        = static_cast<f32>( 1 << DEPTH_BUFFER_BITS );
static const f32 kDepthBufferZConst        = 0.5f * kDepthBufferZScale;

//=========================================================================

void WritePaletteColor( u8* pPalette, s32 Index, xcolor Color )
{
    ASSERT( Index >= 0 );
    ASSERT( Index < kFogPaletteCount );

    pPalette[Index * 4 + 0] = Color.R;
    pPalette[Index * 4 + 1] = Color.G;
    pPalette[Index * 4 + 2] = Color.B;
    pPalette[Index * 4 + 3] = Color.A;
}

//=========================================================================

static xcolor ReadPaletteColor( const u8* pPalette, s32 Index )
{
    ASSERT( pPalette );
    ASSERT( Index >= 0 );

    return xcolor( pPalette[Index * 4 + 0], pPalette[Index * 4 + 1], pPalette[Index * 4 + 2], pPalette[Index * 4 + 3] );
}

//=========================================================================

static f32 ComputeFogFalloff( render::post_falloff_fn Fn, f32 ViewZ, f32 NearZ, f32 FarZ, f32 Param1, f32 Param2 )
{
    const f32 FarMinusNear = MAX( FarZ - NearZ, 0.001f );

    switch( Fn )
    {
        case render::FALLOFF_LINEAR:
        {
            ASSERT( Param2 > Param1 );
            const f32 ClampedZ = MINMAX( Param1, ViewZ, Param2 );
            return ( Param2 - ClampedZ ) / MAX( Param2 - Param1, 0.001f );
        }

        case render::FALLOFF_EXP:
        {
            const f32 D = ( ViewZ - NearZ ) / FarMinusNear;
            return 1.0f / x_exp( D * Param1 );
        }

        case render::FALLOFF_EXP2:
        {
            f32 D = ( ViewZ - NearZ ) / FarMinusNear;
            D *= Param1;
            return 1.0f / x_exp( D * D );
        }

        default:
        {
            break;
        }
    }

    return 1.0f;
}

//=========================================================================

static f32 EvaluatePolynomialFogAlpha( f32 LinearDepth, f32 NearZ, f32 FarZ, f32 FogStart, const vector4& FogCoeff )
{
    const f32 ViewZ   = NearZ + LinearDepth * MAX( FarZ - NearZ, 0.00001f );
    const f32 Q       = FarZ / MAX( FarZ - NearZ, 0.00001f );
    const f32 XboxZ   = ViewZ * Q - NearZ * Q;
    const f32 StartZ2 = FogStart * FogStart;
    const f32 StartZ3 = StartZ2 * FogStart;
    const f32 StartAlpha =
        FogCoeff.GetX() + FogCoeff.GetY() * FogStart + FogCoeff.GetZ() * StartZ2 + FogCoeff.GetW() * StartZ3;

    if( XboxZ <= FogStart )
    {
        return 0.0f;
    }

    const f32 Z2       = XboxZ * XboxZ;
    const f32 Z3       = Z2 * XboxZ;
    const f32 RawAlpha = FogCoeff.GetX() + FogCoeff.GetY() * XboxZ + FogCoeff.GetZ() * Z2 + FogCoeff.GetW() * Z3;

    return MINMAX( 0.0f, ( RawAlpha - StartAlpha ) / MAX( 1.0f - StartAlpha, 0.00001f ), 1.0f );
}

//=========================================================================

static s32 GetPaletteIndexFromDepth( f32 LinearDepth )
{
    const f32 ClampedDepth = MINMAX( 0.0f, LinearDepth, 1.0f );
    return static_cast<s32>( ClampedDepth * static_cast<f32>( kFogPaletteCount - 1 ) + 0.5f );
}

//=========================================================================

static void ExpandCustomFogPalette( const u8* pSourcePalette, u8* pExpandedPalette, f32 NearZ, f32 FarZ )
{
    ASSERT( pSourcePalette );
    ASSERT( pExpandedPalette );

    const f32 Range = MAX( FarZ - NearZ, 0.001f );

    for( s32 i = 0; i < kFogPaletteCount; ++i )
    {
        const f32 LinearDepth = static_cast<f32>( i ) / static_cast<f32>( kFogPaletteCount - 1 );
        const f32 ViewZ       = NearZ + LinearDepth * Range;

        f32 ScreenZ = ( ViewZ * ( FarZ + NearZ ) / Range );
        ScreenZ -= ( ( 2.0f * FarZ * NearZ ) / Range );
        ScreenZ /= ViewZ;
        ScreenZ *= -kDepthBufferZConst;
        ScreenZ += kDepthBufferZConst;

        const u32 ScreenZFixed = static_cast<u32>( ScreenZ * 16.0f + 0.5f );

        s32 ClutIx = 0;

        if( ScreenZFixed <= 0xFFFF )
        {
            ClutIx = static_cast<s32>( ( ( ScreenZFixed >> 8 ) & 0xFF ) / 4 );
        }

        ClutIx = MINMAX( 0, ClutIx, kFogSourcePaletteCount - 1 );
        WritePaletteColor( pExpandedPalette, i, ReadPaletteColor( pSourcePalette, ClutIx ) );
    }

    pExpandedPalette[3] = 0;
}
} // namespace

//=========================================================================
//  FOG RESOURCE MANAGEMENT
//=========================================================================

PostProcessFeature::FogResources::FogResources()
{
    PaletteTexture    = vram_texture();
    CompositePS       = shader();
    PaletteSampler    = rstate_sampler();
    Constants         = RenderFogConstants();
}

//=========================================================================

xbool PostProcessFeature::FogResources::Initialize( void )
{
    Shutdown();

    shader_LoadComboStage( CompositePS, A51FixedPrograms[A51FixedProgram_post_fog].PackageFile, SHADER_COMBO_FIXED,
                           A51FixedPrograms[A51FixedProgram_post_fog].Key, SHADER_STAGE_PIXEL );
    rstate_CreateSampler( PaletteSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostFogPalette" );

    if( !CompositePS || !PaletteSampler )
    {
        RENDER_THROW( "PostProcessFeature", "failed to initialize fog resources" );
        return FALSE;
    }

    u8 EmptyPalette[kFogPaletteCount * 4];
    x_memset( EmptyPalette, 0, sizeof( EmptyPalette ) );

    vram_texture_desc Desc;
    Desc.Width      = kFogPaletteCount;
    Desc.Height     = 1;
    Desc.Format     = VRAM_TEXTURE_FORMAT_RGBA8;
    Desc.pDebugName = "PostFogPalette";

    vram_texture_upload_desc Upload;
    Upload.pData         = EmptyPalette;
    Upload.Size          = sizeof( EmptyPalette );
    Upload.RowPitch      = sizeof( EmptyPalette );
    Upload.Region.Width  = kFogPaletteCount;
    Upload.Region.Height = 1;

    if( !vram_CreateTexture( PaletteTexture, Desc ) || !vram_UploadTexture( PaletteTexture, Upload ) )
    {
        RENDER_THROW( "PostProcessFeature", "failed to initialize fog palette texture" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void PostProcessFeature::FogResources::Shutdown( void )
{
    vram_DestroyTexture( PaletteTexture );
    rstate_DestroySampler( PaletteSampler );
    shader_Destroy( CompositePS );
}

//=========================================================================

void PostProcessFeature::FogResources::UpdateConstants( const RenderFogConstants& NewConstants )
{
    Constants = NewConstants;
}

//=========================================================================

xbool PostProcessFeature::FogResources::UpdatePaletteTexture( FrameGraphBuilder& Graph, const u8* pPalette )
{
    if( !pPalette || !vram_IsValid( PaletteTexture ) )
    {
        return FALSE;
    }

    vram_texture_upload_desc Upload;
    Upload.Region.Width  = kFogPaletteCount;
    Upload.Region.Height = 1;
    Upload.Region.Depth  = 1;
    Upload.pData         = pPalette;
    Upload.Size          = kFogPaletteCount * 4;
    Upload.RowPitch      = kFogPaletteCount * 4;
    Upload.SlicePitch    = kFogPaletteCount * 4;
    Upload.bCycle        = TRUE;

    return Graph.QueueUpload( PaletteTexture, Upload );
}

//=========================================================================

xbool PostProcessFeature::FogResources::BindConstants( const shader& PixelShader ) const
{
    u32 FogConstantsSlot = PixelShader.ResourceSlots[A51Resource_cbFogConstants];

    if( FogConstantsSlot == 0xFFFFFFFFu ||
        !shader_PushUniformData( SHADER_STAGE_PIXEL, FogConstantsSlot, &Constants, sizeof( Constants ) ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

const shader_resource* PostProcessFeature::FogResources::GetPaletteResource( void ) const
{
    return vram_IsValid( PaletteTexture ) ? vram_GetShaderResource( PaletteTexture ) : NULL;
}

//=========================================================================

const rstate_sampler* PostProcessFeature::FogResources::GetPaletteSampler( void ) const
{
    return PaletteSampler ? &PaletteSampler : NULL;
}

//=========================================================================
//  FOG PROCESSING
//=========================================================================

void PostProcessFeature::SetCustomFogPalette( const texture::handle& TextureHandle,
                                              xbool                  ImmediateSwitch,
                                              s32                    PaletteIndex,
                                              const view*            pRenderView )
{
    ASSERT( PaletteIndex >= 0 );
    ASSERT( PaletteIndex < 5 );
    if( PaletteIndex == kFogGeneratedPaletteIndex )
    {
        m_isGeneratedFogValid = FALSE;
    }

    u8 TargetPalette[kFogSourcePaletteCount * 4];
    x_memset( TargetPalette, 0, sizeof( TargetPalette ) );

    texture* pTexture = TextureHandle.GetPointer();

    if( pTexture )
    {
        const xbitmap& Bitmap = pTexture->m_bitmap;
        const s32      Width  = Bitmap.GetWidth();
        const s32      Height = Bitmap.GetHeight();
        const s32      Count  = Width * Height;

        if( Count > 0 )
        {
            for( s32 i = 0; i < kFogSourcePaletteCount; ++i )
            {
                const f32 SampleT = ( kFogSourcePaletteCount > 1 )
                                        ? ( static_cast<f32>( i ) / static_cast<f32>( kFogSourcePaletteCount - 1 ) )
                                        : 0.0f;
                const s32 SourceIx =
                    MIN( static_cast<s32>( SampleT * static_cast<f32>( Count - 1 ) + 0.5f ), Count - 1 );
                const s32    SourceX = SourceIx % Width;
                const s32    SourceY = SourceIx / Width;
                const xcolor Color   = Bitmap.GetPixelColor( SourceX, SourceY );

                WritePaletteColor( TargetPalette, i, Color );
            }
        }
    }

    // Blend the source palette before expanding it for the GPU.
    u8* pCurrentPalette = m_FogSourcePalette[PaletteIndex];

    for( s32 i = 0; i < ( kFogSourcePaletteCount * 4 ); ++i )
    {
        if( ImmediateSwitch )
        {
            pCurrentPalette[i] = TargetPalette[i];
        }
        else if( pCurrentPalette[i] < TargetPalette[i] )
        {
            pCurrentPalette[i]++;
        }
        else if( pCurrentPalette[i] > TargetPalette[i] )
        {
            pCurrentPalette[i]--;
        }
    }

    if( pRenderView )
    {
        f32 NearZ, FarZ;
        pRenderView->GetZLimits( NearZ, FarZ );
        ExpandCustomFogPalette( m_FogSourcePalette[PaletteIndex], m_FogPalette[PaletteIndex], NearZ, FarZ );

        u32 ColorTotal[4] = { 0, 0, 0, 0 };

        for( s32 i = 0; i < kFogSourcePaletteCount * 4; i += 4 )
        {
            ColorTotal[0] += m_FogSourcePalette[PaletteIndex][i + 0];
            ColorTotal[1] += m_FogSourcePalette[PaletteIndex][i + 1];
            ColorTotal[2] += m_FogSourcePalette[PaletteIndex][i + 2];
            ColorTotal[3] += m_FogSourcePalette[PaletteIndex][i + 3];
        }

        ColorTotal[0] /= kFogSourcePaletteCount;
        ColorTotal[1] /= kFogSourcePaletteCount;
        ColorTotal[2] /= kFogSourcePaletteCount;
        ColorTotal[3] /= kFogSourcePaletteCount;

        m_FogColor[PaletteIndex].Set(
            static_cast<f32>( ColorTotal[0] ) / 255.0f, static_cast<f32>( ColorTotal[1] ) / 255.0f,
            static_cast<f32>( ColorTotal[2] ) / 255.0f, static_cast<f32>( ColorTotal[3] ) / 255.0f );

        // Fit the palette alpha in projected-Z space; the shader converts linear depth before evaluating it.
        const f32 Numer        = ( 2.0f * kDepthBufferZConst * NearZ * FarZ ) / ( FarZ - NearZ );
        const f32 DenomOffset  = -kDepthBufferZConst + ( kDepthBufferZConst * ( FarZ + NearZ ) ) / ( FarZ - NearZ );
        const f32 DepthScaleQ  = FarZ / ( FarZ - NearZ );
        static const f32 Scale = 1.0f / 16.0f;

        const f32 DepthBufferZValue = static_cast<f32>( 0xFFFF ) * Scale;
        const f32 Denom             = DepthBufferZValue + DenomOffset;
        ASSERT( x_abs( Denom ) > 0.001f );
        const f32 ViewZValue      = Numer / Denom;
        const f32 ProjectedZValue = ViewZValue * DepthScaleQ - NearZ * DepthScaleQ;
        ASSERT( ViewZValue > 0.001f );
        m_FogStart[PaletteIndex] = ProjectedZValue;

        LeastSquares AlphaApprox;
        AlphaApprox.Setup( 3 );

        static const f32 nSamples = 512.0f;
        const f32        StepSize = ( FarZ - NearZ ) / nSamples;

        for( f32 ViewZ = NearZ; ViewZ <= FarZ; ViewZ += StepSize )
        {
            f32 ScreenZ = ( ViewZ * ( FarZ + NearZ ) / ( FarZ - NearZ ) );
            ScreenZ -= ( ( 2.0f * FarZ * NearZ ) / ( FarZ - NearZ ) );
            ScreenZ /= ViewZ;
            ScreenZ *= -kDepthBufferZConst;
            ScreenZ += kDepthBufferZConst;

            const f32 ProjectedScreenZ = ( ViewZ * DepthScaleQ ) - ( NearZ * DepthScaleQ );

            f32       Alpha        = 0.0f;
            const u32 ScreenZFixed = static_cast<u32>( ScreenZ * 16.0f + 0.5f );

            if( ScreenZFixed <= 0xFFFF )
            {
                const s32 ClutIx =
                    MINMAX( 0, static_cast<s32>( ( ( ScreenZFixed >> 8 ) & 0xFF ) / 4 ), kFogSourcePaletteCount - 1 );
                Alpha = static_cast<f32>( m_FogSourcePalette[PaletteIndex][ClutIx * 4 + 3] ) / 255.0f;
            }

            AlphaApprox.AddSample( ProjectedScreenZ, Alpha );
        }

        if( !AlphaApprox.Solve() )
        {
            AlphaApprox.SetCoeff( 0, static_cast<f32>( m_FogSourcePalette[PaletteIndex][3] ) / 255.0f );
            AlphaApprox.SetCoeff( 1, 0.0f );
            AlphaApprox.SetCoeff( 2, 0.0f );
            AlphaApprox.SetCoeff( 3, 0.0f );
        }

        m_FogConst[PaletteIndex].Set( AlphaApprox.GetCoeff( 0 ), AlphaApprox.GetCoeff( 1 ), AlphaApprox.GetCoeff( 2 ),
                                      AlphaApprox.GetCoeff( 3 ) );
    }

    m_isFogValid[PaletteIndex] = TRUE;
}

//=========================================================================

void PostProcessFeature::BuildFogPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    m_FogFilter.PaletteIndex = kFogGeneratedPaletteIndex;

    m_FogFilter.Fn[m_FogFilter.PaletteIndex] = Fn;

    if( m_isGeneratedFogValid && m_isFogValid[kFogGeneratedPaletteIndex] &&
        m_GeneratedFog.Fn[kFogGeneratedPaletteIndex] == Fn && m_GeneratedFog.GeneratedColor == Color &&
        m_GeneratedFog.GeneratedParam1 == Param1 && m_GeneratedFog.GeneratedParam2 == Param2 &&
        m_GeneratedFogNearZ == m_PostNearZ && m_GeneratedFogFarZ == m_PostFarZ )
    {
        return;
    }

    m_GeneratedFog.Fn[kFogGeneratedPaletteIndex] = Fn;
    m_GeneratedFog.GeneratedColor                = Color;
    m_GeneratedFog.GeneratedParam1               = Param1;
    m_GeneratedFog.GeneratedParam2               = Param2;
    m_GeneratedFogNearZ                          = m_PostNearZ;
    m_GeneratedFogFarZ                           = m_PostFarZ;
    m_isGeneratedFogValid                        = TRUE;

    const f32 NearZ = m_PostNearZ;
    const f32 FarZ  = m_PostFarZ;
    const f32 Range = MAX( FarZ - NearZ, 0.001f );

    for( s32 i = 0; i < kFogPaletteCount; ++i )
    {
        xcolor EntryColor( Color.R, Color.G, Color.B, Color.A );

        if( Fn == render::FALLOFF_CONSTANT )
        {
            EntryColor.A = Color.A;
        }
        else
        {
            const f32 LinearDepth = static_cast<f32>( i ) / static_cast<f32>( kFogPaletteCount - 1 );
            const f32 ViewZ       = NearZ + LinearDepth * Range;
            const f32 F           = MINMAX( 0.0f, ComputeFogFalloff( Fn, ViewZ, NearZ, FarZ, Param1, Param2 ), 1.0f );
            EntryColor.A          = static_cast<u8>( MINMAX( 0.0f, 128.0f - ( F * 128.0f ), 255.0f ) );

            if( i == 0 )
            {
                EntryColor.A = 0;
            }
        }

        WritePaletteColor( m_FogPalette[m_FogFilter.PaletteIndex], i, EntryColor );
    }

    m_FogColor[m_FogFilter.PaletteIndex].Set(
        static_cast<f32>( Color.R ) / 255.0f, static_cast<f32>( Color.G ) / 255.0f,
        static_cast<f32>( Color.B ) / 255.0f, static_cast<f32>( Color.A ) / 255.0f );
    m_isFogValid[m_FogFilter.PaletteIndex] = TRUE;
}

//=========================================================================

xbool PostProcessFeature::ExecuteZFogFilter( const rtarget* pSceneDepth )
{
    const s32 PaletteIndex = m_FogFilter.PaletteIndex;

    if( ( PaletteIndex < 0 ) || ( PaletteIndex >= 5 ) || !m_isFogValid[PaletteIndex] )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass has no valid palette index=%d", PaletteIndex );
        return FALSE;
    }

    if( !pSceneDepth || !rtarget_HasShaderResource( *pSceneDepth ) )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass SceneDepth is unavailable or not shader-readable" );
        return FALSE;
    }

    const shader& PixelShader = m_FogResources.CompositePS;

    if( !PixelShader )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass shader is unavailable" );
        return FALSE;
    }

    RenderFogConstants Constants;
    GetFogConstants( Constants );
    m_FogResources.UpdateConstants( Constants );

    if( !m_FogResources.BindConstants( PixelShader ) )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass shader bindings failed" );
        return FALSE;
    }

    composite_extra_source FogPalette;
    FogPalette.ResourceSemantic = A51Resource_FogPalette;
    FogPalette.pResource        = m_FogResources.GetPaletteResource();
    FogPalette.SamplerMode      = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;

    if( !FogPalette.pResource )
    {
        RENDER_THROW( "PostProcessFeature", "fog pass palette resource is unavailable" );
        return FALSE;
    }

    return composite_Blit( *pSceneDepth, COMPOSITE_BLEND_ALPHA_RGB, 1.0f, &PixelShader,
                           RSTATE_SAMPLER_PRESET_POINT_CLAMP, A51Resource_SceneDepthSource, &FogPalette, 1 );
}

//=========================================================================

void PostProcessFeature::GetFogConstants( RenderFogConstants& Constants ) const
{
    Constants                = RenderFogConstants();
    Constants.XboxFogEnabled = m_isXboxFogEnabled ? 1u : 0u;
    Constants.Depth.Set( m_PostNearZ, m_PostFarZ, 0.0f, 0.0f );

    const s32 PaletteIndex = m_FogFilter.PaletteIndex;

    if( m_Requests.Override || ( PaletteIndex < 0 ) || ( PaletteIndex >= 5 ) || !m_isFogValid[PaletteIndex] )
    {
        return;
    }

    Constants.Color = m_FogColor[PaletteIndex];

    if( m_FogFilter.Fn[PaletteIndex] == render::FALLOFF_CUSTOM )
    {
        Constants.PreparePolynomial( m_PostNearZ, m_PostFarZ, m_FogStart[PaletteIndex], m_FogConst[PaletteIndex] );
    }
}

//=========================================================================

const shader_resource* PostProcessFeature::GetFogPaletteResource( void ) const
{
    return m_FogResources.GetPaletteResource();
}

//=========================================================================

const rstate_sampler* PostProcessFeature::GetFogPaletteSampler( void ) const
{
    return m_FogResources.GetPaletteSampler();
}

//=========================================================================

xcolor PostProcessFeature::GetFogValue( const vector3& WorldPos, s32 PaletteIndex, const view* pRenderView )
{
    if( ( PaletteIndex < 0 ) || ( PaletteIndex >= 5 ) || !m_isFogValid[PaletteIndex] )
    {
        return xcolor( 255, 255, 255, 0 );
    }

    if( !pRenderView )
    {
        return xcolor( 255, 255, 255, 0 );
    }

    f32 NearZ, FarZ;
    pRenderView->GetZLimits( NearZ, FarZ );

    vector4 ViewPos( WorldPos );
    ViewPos.GetW() = 1.0f;
    ViewPos        = pRenderView->GetW2V() * ViewPos;

    const f32 LinearDepth = ( ViewPos.GetZ() - NearZ ) / MAX( FarZ - NearZ, 0.001f );

    if( m_FogFilter.Fn[PaletteIndex] == render::FALLOFF_CUSTOM )
    {
        const f32 FogIntensity =
            EvaluatePolynomialFogAlpha( LinearDepth, NearZ, FarZ, m_FogStart[PaletteIndex], m_FogConst[PaletteIndex] );
        return xcolor( 255, 255, 255, static_cast<u8>( FogIntensity * 255.0f ) );
    }

    const s32 PaletteIx = GetPaletteIndexFromDepth( LinearDepth );
    const u8  Alpha     = m_FogPalette[PaletteIndex][PaletteIx * 4 + 3];

    return xcolor( 255, 255, 255, Alpha );
}

} // namespace render
