//==============================================================================
//
//  geom_pixel_surface.hlsl
//
//  Surface sampling, environment and glow helpers for geometry shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_SURFACE_HLSL
#define GEOM_PIXEL_SURFACE_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

GeomDiffuseResult GeomEvaluateDiffuse( GEOM_PIXEL_INPUT Input, uint MaterialFlags, float AlphaRef, float2 UVDx, float2 UVDy )
{
    GeomDiffuseResult Result;
    Result.Sample = txDiffuse.SampleGrad( samDiffuse, Input.UV, UVDx, UVDy );
    Result.Color  = (A51Color4)Result.Sample;
    Result.Damage = 1;
#if A51_RECIPE_DAMAGE
    Result.Damage = txDamage.SampleGrad(samDamage, Input.UV, UVDx, UVDy).r;
    ClothDiscardDamagedPixel( Result.Sample.a, Result.Damage, CoverageUsesTextureAlpha != 0 );
#endif

#if A51_RECIPE_DETAIL
    if( MaterialFlags & MATERIAL_FLAG_DETAIL )
    {
        float  DetailScale = max( UVAnim.z, 1.0f );
        float4 DetailColor = txDetail.SampleGrad( samDetail, Input.UV * DetailScale, UVDx * DetailScale, UVDy * DetailScale );
        Result.Color = (A51Color4)( Result.Color * ( DetailColor * 2.0f ) );
    }

#endif
#if GEOM_HAS_VERTEX_COLOR
    if( MaterialFlags & MATERIAL_FLAG_VERTEX_COLOR )
    {
#if A51_COLOR_ARITHMETIC
        Result.Color.a = (A51Color)( Result.Color.a * Input.Color.a );
#else
        Result.Color.a *= Input.Color.a;
#endif
    }
#endif

#if A51_GEOM_ALPHA_TEST
    if( !A51_RECIPE_DAMAGE && (MaterialFlags & MATERIAL_FLAG_ALPHA_TEST) )
    {
        if( Result.Sample.a < AlphaRef )
        {
            discard;
        }
    }

#endif
    Result.Alpha = Result.Color.a;
    return Result;
}

//==============================================================================

#if !A51_GEOM_SURFACE_ONLY
void GeomApplyEnvironment( inout GeomDiffuseResult Diffuse, uint MaterialFlags, float3 Normal, float3 EnvironmentColor, GeomSurfaceGradients Gradients )
{
    float AlphaValue = Diffuse.Alpha;

    if( MaterialFlags & MATERIAL_FLAG_ENVIRONMENT )
    {
        float3 WorldNormal = Normal;
        float3 EnvColor    = 0.0f;

#if A51_RECIPE_ENV == 2
        if( MaterialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
        {
            EnvColor = EnvironmentColor;
        }
#endif
#if A51_RECIPE_ENV == 1
        {
            float3 EnvVector;
            float3 EnvDx = Gradients.NormalDx;
            float3 EnvDy = Gradients.NormalDy;

            if( MaterialFlags & MATERIAL_FLAG_ENV_VIEWSPACE )
            {
                EnvVector = mul( (float3x3)View, WorldNormal );
                EnvDx = mul( (float3x3)View, EnvDx );
                EnvDy = mul( (float3x3)View, EnvDy );
            }
            else
            {
                EnvVector = WorldNormal;
            }

            EnvDx = GeomNormalizedGradient( EnvVector, EnvDx );
            EnvDy = GeomNormalizedGradient( EnvVector, EnvDy );
            EnvVector = normalize( EnvVector );

            float2 EnvUV = EnvVector.xy * 0.5f + 0.5f;
            EnvColor = txEnvironment.SampleGrad( samEnvironment, EnvUV, EnvDx.xy * 0.5f, EnvDy.xy * 0.5f ).rgb;
        }

#endif
        float EnvStrength = 1.0f;
        if( MaterialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
        {
            EnvStrength = EnvParams.y;
        }

        float EnvBlend = 0.0f;

        if( MaterialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
        {
            EnvBlend = Diffuse.Sample.a * EnvStrength;
        }
        else if( MaterialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ENV )
        {
            EnvBlend = EnvParams.x * EnvStrength;
        }

        if( EnvBlend > 0.0f )
        {
            if( MaterialFlags & MATERIAL_FLAG_ADDITIVE )
            {
                Diffuse.Color.rgb = (A51Color3)saturate( Diffuse.Color.rgb + EnvColor * EnvBlend );
            }
            else
            {
                Diffuse.Color.rgb = (A51Color3)lerp( Diffuse.Color.rgb, EnvColor, EnvBlend );
            }
        }
    }

#if A51_COLOR_ARITHMETIC
    Diffuse.Color.a = (A51Color)AlphaValue;
#else
    Diffuse.Color.a = AlphaValue;
#endif
}
#endif

//==============================================================================

float4 GeomComputeGlow( GEOM_PIXEL_INPUT Input,
                        uint MaterialFlags,
                        inout float4 FinalColor,
                        float4 DiffuseSample )
{
    float4 Glow = float4( 0.0f, 0.0f, 0.0f, 0.0f );
    const float4 Ambient = GeomGetLightAmbCol( Input );

    // Forward SceneColor alpha is the Xbox glow mask.  Ordinary surface
    // coverage/alpha must not turn every opaque pixel into bloom.
    Glow.a = 0.0f;

    if( MaterialFlags & GEOM_INST_FLAG_GLOWING )
    {
        const float3 ForcedGlowColor = saturate( Ambient.rgb *
                                                 DiffuseSample.rgb * 2.0f );
        FinalColor.rgb = ForcedGlowColor;
        Glow.rgb = ForcedGlowColor;
        Glow.a   = 1.0f;
        return Glow;
    }

    const bool BUseDiffuse           = IlluminationUsesDiffuse != 0;
    const bool BDiffusePerPixelIllum = ( MaterialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ILLUM ) != 0;
    const bool BAlphaPerPixelIllum   = ( MaterialFlags & MATERIAL_FLAG_ALPHA_PERPIXEL_ILLUM ) != 0;
    const bool BAlphaPerPolyIllum    = ( MaterialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ILLUM ) != 0;

    if( BAlphaPerPixelIllum )
    {
        Glow = DiffuseSample;

        if( !BUseDiffuse )
        {
            FinalColor.rgb = DiffuseSample.rgb;
        }
    }
    else if( BDiffusePerPixelIllum || BAlphaPerPolyIllum )
    {
        const float Intensity = saturate( DiffuseSample.a );
        const float3 IllumRgb = lerp( FinalColor.rgb, DiffuseSample.rgb, Intensity );

        Glow.rgb = IllumRgb;
        Glow.a   = BAlphaPerPolyIllum ? saturate( EnvParams.x ) : Intensity;

        if( !BUseDiffuse )
        {
            FinalColor.rgb = IllumRgb;
        }
    }

    return Glow;
}

//==============================================================================
#endif // GEOM_PIXEL_SURFACE_HLSL
//==============================================================================
