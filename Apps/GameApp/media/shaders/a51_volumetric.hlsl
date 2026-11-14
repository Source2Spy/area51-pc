//==============================================================================
//
//  a51_volumetric.hlsl
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  DEFINES
//==============================================================================

#ifndef A51_VOLUMETRIC_MSAA
    #define A51_VOLUMETRIC_MSAA 0
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

#if A51_VOLUMETRIC_MSAA
    A51_STORAGE_TEXTURE_ATTR(0, 2) Texture2DMS<float> SceneDepthSource A51_STORAGE_TEXTURE_BIND(0, 2);
    A51_SAMPLED_TEXTURE_ATTR(1, 0) Texture2D<float4> VolumeMask A51_SAMPLED_TEXTURE_BIND(1, 0);
    A51_SAMPLER_ATTR(1, 0) SamplerState samVolumeMask A51_SAMPLER_BIND(1, 0);
    A51_SAMPLED_TEXTURE_ATTR(2, 1) Texture2D<float> VolumeShadowMap A51_SAMPLED_TEXTURE_BIND(2, 1);
    A51_SAMPLER_ATTR(2, 1) SamplerState samVolumeShadowMap A51_SAMPLER_BIND(2, 1);
#else
    A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D<float> SceneDepthSource A51_SAMPLED_TEXTURE_BIND(0, 0);
    A51_SAMPLER_ATTR(0, 0) SamplerState samSceneDepthSource A51_SAMPLER_BIND(0, 0);
    A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D<float4> VolumeMask A51_SAMPLED_TEXTURE_BIND(1, 1);
    A51_SAMPLER_ATTR(1, 1) SamplerState samVolumeMask A51_SAMPLER_BIND(1, 1);
    A51_SAMPLED_TEXTURE_ATTR(2, 2) Texture2D<float> VolumeShadowMap A51_SAMPLED_TEXTURE_BIND(2, 2);
    A51_SAMPLER_ATTR(2, 2) SamplerState samVolumeShadowMap A51_SAMPLER_BIND(2, 2);
#endif

//------------------------------------------------------------------------------

// Exactly 272 bytes; CPU matrices use mul(Matrix, vector).
A51_CBUFFER_ATTR(0, 0) cbuffer VolumetricParams A51_CBUFFER_BIND(0, 0)
{
    column_major float4x4 ViewToLocal;
    float4               CameraLocal;
    float4               RayMulAdd;
    float4               Shape;
    float4               StartColor;
    float4               EndColor;
    float4               DepthInfo;
    float4               FadeParams; // radial softness, start/end fade fractions
    column_major float4x4 LocalToShadow;
    float4               ShadowRect; // texel-inset atlas UV bounds
    float4               ShadowInfo; // enabled, normalized depth bias, atlas texel size
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float ShadowVisibility( float3 Local )
{
    if( ShadowInfo.x == 0.0f )
    {
        return 1.0f;
    }

    float4 Projected = mul( LocalToShadow, float4( Local, 1.0f ) );
    float3 Coord     = Projected.xyz / Projected.w;

    if( Coord.z < 0.0f || Coord.z > 1.0f ||
        any( Coord.xy < ShadowRect.xy ) || any( Coord.xy > ShadowRect.zw ) )
    {
        return 1.0f;
    }

    // Small PCF kernel, clamped inside this volume's atlas tile.
    float Visibility = 0.0f;
    [unroll]
    for( int Y = -1; Y <= 1; ++Y )
    {
        [unroll]
        for( int X = -1; X <= 1; ++X )
        {
            float2 UV   = clamp( Coord.xy + float2( X, Y ) * ShadowInfo.z, ShadowRect.xy, ShadowRect.zw );
            float Depth = VolumeShadowMap.SampleLevel( samVolumeShadowMap, UV, 0.0f );

            Visibility += Coord.z - ShadowInfo.y <= Depth ? 1.0f : 0.0f;
        }
    }
    return Visibility / 9.0f;
}

//==============================================================================

// Explicit parallel slabs avoid reciprocal-zero NaNs, including on box faces.
bool ClipSlab( float Origin, float Direction, float MinBound, float MaxBound,
               inout float Enter, inout float Exit )
{
    if( abs( Direction ) < 1.0e-8f )
    {
        return Origin >= MinBound && Origin <= MaxBound;
    }

    float T0 = ( MinBound - Origin ) / Direction;
    float T1 = ( MaxBound - Origin ) / Direction;

    Enter = max( Enter, min( T0, T1 ) );
    Exit  = min( Exit, max( T0, T1 ) );
    return Exit > Enter;
}

//==============================================================================

float4 IntegrateVolume( float4 Position, float SceneDepth )
{
    float MaxRadius = max( Shape.x, Shape.y );
    if( Shape.z <= 0.0f || MaxRadius <= 0.0f || Shape.w <= 0.0f )
    {
        return 0.0f.xxxx;
    }

    float2 UV       = Position.xy * DepthInfo.zw;
    float3 ViewRay  = float3( UV * RayMulAdd.xy + RayMulAdd.zw, 1.0f );
    float3 LocalRay = mul( (float3x3)ViewToLocal, ViewRay );

    // The unnormalized ray parameter is positive view Z, not ray distance.
    // Start at the camera, NOT nearZ: retain near-plane/inside-volume emission.
    float Enter = 0.0f;
    float Exit  = lerp( DepthInfo.x, DepthInfo.y, saturate( SceneDepth ) );

    if( Exit <= Enter )
    {
        return 0.0f.xxxx;
    }
    if( !ClipSlab( CameraLocal.x, LocalRay.x, -MaxRadius, MaxRadius, Enter, Exit ) )
    {
        return 0.0f.xxxx;
    }
    if( !ClipSlab( CameraLocal.y, LocalRay.y, -MaxRadius, MaxRadius, Enter, Exit ) )
    {
        return 0.0f.xxxx;
    }
    if( !ClipSlab( CameraLocal.z, LocalRay.z, 0.0f, Shape.z, Enter, Exit ) )
    {
        return 0.0f.xxxx;
    }

    const uint StepCount = 64;
    float      Dt        = ( Exit - Enter ) / float( StepCount );
    float      Ds        = length( ViewRay ) * Dt; // world distance for rigid L2W
    float3     AccumRGB  = 0.0f.xxx;

    [loop]
    for( uint Step = 0; Step < StepCount; ++Step )
    {
        float  T     = Enter + ( float( Step ) + 0.5f ) * Dt;
        float3 Local = CameraLocal.xyz + LocalRay * T;
        float  Axial = saturate( Local.z / Shape.z );
        float Radius = lerp( Shape.x, Shape.y, Axial );

        if( Radius <= 0.0f || dot( Local.xy, Local.xy ) > Radius * Radius )
        {
            continue;
        }

        float2 MaskUV = Local.xy / ( 2.0f * Radius ) + 0.5f;
        float4 Mask   = VolumeMask.SampleLevel( samVolumeMask, MaskUV, 0.0f );
        float4 Color  = lerp( StartColor, EndColor, Axial );

        float AxialFade = ( FadeParams.y > 0.0f ? smoothstep( 0.0f, FadeParams.y, Axial ) : 1.0f )
                        * ( FadeParams.z > 0.0f ? smoothstep( 0.0f, FadeParams.z, 1.0f - Axial ) : 1.0f );
        float Radial     = length( Local.xy ) / Radius;
        float RadialFade = FadeParams.x > 0.0f
                               ? 1.0f - smoothstep( 1.0f - FadeParams.x, 1.0f, Radial ) : 1.0f;
        float3 Q = Mask.rgb * Mask.a * Color.rgb * Color.a * Shape.w;

        AccumRGB += Q * ( AxialFade * RadialFade * ShadowVisibility( Local ) * Ds );
    }

    // Caller selects ONE/ONE RGB blending; preserve destination alpha.
    return float4( AccumRGB, 0.0f );
}

//==============================================================================
//  SHADERS
//==============================================================================

#if A51_VOLUMETRIC_MSAA
float4 PSMain( float4 Position : SV_POSITION, uint SampleIndex : SV_SampleIndex ) : SV_Target
{
    return IntegrateVolume( Position, SceneDepthSource.Load( int2( Position.xy ), SampleIndex ) );
}
#else
float4 PSMain( float4 Position : SV_POSITION ) : SV_Target
{
    float2 UV = Position.xy * DepthInfo.zw;
    return IntegrateVolume( Position, SceneDepthSource.SampleLevel( samSceneDepthSource, UV, 0.0f ) );
}
#endif
