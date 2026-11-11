//==============================================================================
//
//  geom_pixel_distortion.hlsl
//
//  Distortion helpers for geometry shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_DISTORTION_HLSL
#define GEOM_PIXEL_DISTORTION_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float3 GeomGetDistortionViewNormal( float3 WorldNormal )
{
    float3 DistortionViewNormal = mul( (float3x3)DistortionNormalMatrix, WorldNormal );
    return normalize( DistortionViewNormal );
}

//==============================================================================

float2 GeomGetDistortionSceneUV( GEOM_PIXEL_INPUT Input, float3 ViewNormal )
{
    float2 SceneUV  = Input.Pos.xy * DistortionParams.zw;
    float2 Offset   = ViewNormal.xy;
    float  LengthSq = dot( Offset, Offset );

    if( LengthSq > 1e-5f )
    {
        Offset *= rsqrt( LengthSq );
    }
    else
    {
        Offset = 0.0f;
    }

    SceneUV += Offset * DistortionParams.x * DistortionParams.zw;
    return saturate( SceneUV );
}

//==============================================================================

float4 GeomShadeDistortionPixel( GEOM_PIXEL_INPUT Input, uint MaterialFlags, float FadeAlpha )
{
    const float3 ViewNormal = GeomGetDistortionViewNormal(Input.Normal);
    const float2 SceneUV = GeomGetDistortionSceneUV(Input, ViewNormal);
    const float2 SceneDx = ddx( SceneUV );
    const float2 SceneDy = ddy( SceneUV );
#if A51_RECIPE_ENV == 2
    const float3 EnvCoordinate = normalize(Input.ViewVector);
    // Keep implicit cube derivatives ahead of the coverage discard.
    const float3 EnvColor = txEnvironmentCube.Sample( samEnvironmentCube, EnvCoordinate ).rgb;
#elif A51_RECIPE_ENV == 1
    const float2 EnvCoordinate = saturate(ViewNormal.xy * 0.5f + 0.5f);
    const float2 EnvDx = ddx(EnvCoordinate);
    const float2 EnvDy = ddy(EnvCoordinate);
#endif
#if A51_GEOM_ALPHA_TEST
    const float2 UVDx = ddx( Input.UV );
    const float2 UVDy = ddy( Input.UV );
    if( txDiffuse.SampleGrad( samDiffuse, Input.UV, UVDx, UVDy ).a < AlphaRef ) discard;
#endif
    const float3 SceneColor = txDistortionScene.SampleGrad( samDistortion, SceneUV, SceneDx, SceneDy ).rgb;

    float3 FinalColor = SceneColor;

#if A51_RECIPE_ENV != 0
    if( MaterialFlags & MATERIAL_FLAG_DISTORTION_PERPOLY_ENV )
    {
        const float  EnvBlend    = saturate( EnvParams.x );
        const float  EnvStrength = EnvBlend * (1.0f - EnvBlend);
#if A51_RECIPE_ENV == 1
        const float3 EnvColor = txEnvironment.SampleGrad(samEnvironment, EnvCoordinate, EnvDx, EnvDy).rgb;
#endif

        FinalColor = saturate( SceneColor + EnvColor * EnvStrength );
    }

#endif
    FinalColor = lerp( SceneColor, FinalColor, FadeAlpha );

    return float4( FinalColor, FadeAlpha );
}

//==============================================================================
#endif // GEOM_PIXEL_DISTORTION_HLSL
//==============================================================================
