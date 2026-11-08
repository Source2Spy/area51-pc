//==============================================================================
//
//  a51_post_filters.hlsl
//
//  Post-processing filter shaders.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D PostSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D FilterSource1 A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLED_TEXTURE_ATTR(2, 2) Texture2D FilterRamp A51_SAMPLED_TEXTURE_BIND(2, 2);
A51_SAMPLED_TEXTURE_ATTR(3, 3) Texture2D WarpMask1 A51_SAMPLED_TEXTURE_BIND(3, 3);

A51_SAMPLER_ATTR(0, 0) SamplerState samPostSource A51_SAMPLER_BIND(0, 0);
#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLER_ATTR(1, 1) SamplerState samFilterSource1 A51_SAMPLER_BIND(1, 1);
    A51_SAMPLER_ATTR(2, 2) SamplerState samFilterRamp A51_SAMPLER_BIND(2, 2);
    A51_SAMPLER_ATTR(3, 3) SamplerState samWarpMask1 A51_SAMPLER_BIND(3, 3);
#else
    #define samFilterSource1 samPostSource
    #define samFilterRamp    samPostSource
    #define samWarpMask1     samPostSource
#endif

//------------------------------------------------------------------------------

A51_CBUFFER_ATTR(4, 0) cbuffer FilterParams A51_CBUFFER_BIND(4, 0)
{
    A51_ABI_FIELDS_PostFilterData
};

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_MotionBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 Color = PostSource.SampleLevel( samPostSource, UV, 0.0f );
    Color.a = saturate( FilterParams0.x );
    return Color;
}

//==============================================================================

float4 PS_RadialBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 Center = FilterParams6.xy;
    const float2 Delta  = UV - Center;
    const float2 Scaled = Delta * FilterParams0.y;
    const float2 Rotated = float2( Scaled.x * FilterParams0.w - Scaled.y * FilterParams0.z,
                                   Scaled.x * FilterParams0.z + Scaled.y * FilterParams0.w );
    const float2 SampleUV = Center + Rotated;

    float4 Color = PostSource.SampleLevel( samPostSource, SampleUV, 0.0f );
    float  Dist  = length( Delta ) * 2.0f;
    Color.a = saturate( Dist * FilterParams1.y - FilterParams1.x );
    return Color;
}

//==============================================================================

float WarpVolumeVisibility( float3 Ray, float4 Sphere, float SphereConstant, float Depth )
{
    float A = dot( Ray, Ray );
    float B = dot( Ray, Sphere.xyz );
    float Discriminant = B * B - A * SphereConstant;
    if( Discriminant <= 0.0f )
        return 0.0f;

    float Root = sqrt( Discriminant );
    float Entry = max( (B - Root) / A, FilterParams5.x );
    float Exit = (B + Root) / A;
    if( Exit <= Entry )
        return 0.0f;

    float SceneZ = lerp( FilterParams5.x, FilterParams5.y, saturate( Depth ) );
    float VisibleLength = max( min( SceneZ, Exit ) - Entry, 0.0f );
    return smoothstep( 0.0f, min( Sphere.w * 0.1f, 10.0f ), VisibleLength );
}

float3 WarpViewRay( float2 Pixel )
{
    return float3( ( Pixel - FilterParams6.xy ) * FilterParams6.zw, 1.0f );
}

float WarpSphereConstant( int Index )
{
    return WarpSphereConstants[Index / 4][Index % 4];
}

float SampleWarpDepth( float2 UV )
{
#if defined(A51_SHADER_BINDING_SDL)
    float Depth = FilterSource1.SampleLevel( samFilterSource1, UV, 0.0f ).r;
#else
    uint Width, Height;
    FilterSource1.GetDimensions( Width, Height );
    int2 DepthPixel = clamp( int2( UV * float2( Width, Height ) ), int2( 0, 0 ),
                            int2( Width - 1, Height - 1 ) );
    float Depth = FilterSource1.Load( int3( DepthPixel, 0 ) ).r;
#endif
    return Depth;
}

float WarpVisibility( float3 Ray, float2 UV, float4 Sphere, float SphereConstant )
{
    return WarpVolumeVisibility( Ray, Sphere, SphereConstant, SampleWarpDepth( UV ) );
}

float4 PS_WarpMask( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 Mask = 0.0f;
    int First = (int)FilterParams4.x;
    const float3 Ray = WarpViewRay( UV / FilterParams2.yz );
    float Depth = PostSource.SampleLevel( samPostSource, UV, 0.0f ).r;
    [unroll]
    for( int Channel = 0; Channel < 4; ++Channel )
    {
        int Index = First + Channel;
        if( Index < (int)FilterParams2.x )
            Mask[Channel] = WarpVolumeVisibility( Ray, WarpSpheres[Index], WarpSphereConstant( Index ), Depth );
    }
    return Mask;
}

float4 PS_WarpMaskBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float Weights[7] = { 0.069045151f, 0.133406734f, 0.116219167f, 0.090668638f, 0.063345329f, 0.039632240f, 0.022205317f };
    const float Offsets[6] = { 1.489584840f, 3.475713571f, 5.461879674f, 7.448104233f, 9.434407975f, 11.420811147f };
    float4 Mask = PostSource.SampleLevel( samPostSource, UV, 0.0f ) * Weights[0];
    [unroll]
    for( int Tap = 1; Tap < 7; ++Tap )
    {
        float2 Offset = FilterParams4.yz * Offsets[Tap - 1];
        Mask += PostSource.SampleLevel( samPostSource, UV - Offset, 0.0f ) * Weights[Tap];
        Mask += PostSource.SampleLevel( samPostSource, UV + Offset, 0.0f ) * Weights[Tap];
    }
    return Mask;
}

float WarpSoftVisibility( float2 UV, int Index )
{
    float4 Mask;
    if( Index < 4 )
        Mask = FilterRamp.SampleLevel( samFilterRamp, UV, 0.0f );
    else
        Mask = WarpMask1.SampleLevel( samWarpMask1, UV, 0.0f );
    return smoothstep( 0.5f, 1.0f, Mask[Index % 4] );
}

float WarpOffsetVisibility( float2 Pixel, float3 Ray, int Index, float SphereConstant )
{
    float2 UV = Pixel * FilterParams2.yz;
    if( any( UV < 0.0f ) || any( UV > 1.0f ) )
        return 0.0f;
    return WarpVisibility( Ray, UV, WarpSpheres[Index], SphereConstant );
}

float WarpOffsetScale( float2 Pixel, float3 Ray, float2 Displacement, float Visibility, int Index, float SphereConstant )
{
    if( dot( Displacement, Displacement ) < 0.25f )
        return 1.0f;

    const float3 RayDisplacement = float3( Displacement * FilterParams6.zw, 0.0f );
    float Low = 0.0f;
    float High = 2.0f;
    bool Blocked = false;
    [loop]
    for( int Probe = 1; Probe <= 4; ++Probe )
    {
        float Distance = Probe * 0.5f;
        if( WarpOffsetVisibility( Pixel + Displacement * Distance, Ray + RayDisplacement * Distance, Index, SphereConstant ) < Visibility * 0.99f )
        {
            High = Distance;
            Blocked = true;
            break;
        }
        Low = Distance;
    }
    if( !Blocked )
        return 1.0f;

    float DisplacementLength = length( Displacement );
    [loop]
    for( int Step = 0; Step < 12; ++Step )
    {
        if( (High - Low) * DisplacementLength < 0.125f )
            break;
        float Middle = (Low + High) * 0.5f;
        if( WarpOffsetVisibility( Pixel + Displacement * Middle, Ray + RayDisplacement * Middle, Index, SphereConstant ) >= Visibility * 0.99f )
            Low = Middle;
        else
            High = Middle;
    }
    return smoothstep( 0.0f, 2.0f, Low );
}

float4 PS_ScreenWarp( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 Original = PostSource.SampleLevel( samPostSource, UV, 0.0f );
    float4 Color = Original;
    float BaseDepth = 0.0f;
    bool DepthSampled = false;
    const float3 BaseRay = WarpViewRay( Pos.xy );

    [fastopt]
    [loop]
    for( int I = 0; I < 8; ++I )
    {
        if( I >= (int)FilterParams2.x )
            break;

        float2 Delta = Pos.xy - ScreenWarps[I].xy;
        float DistanceSquared = dot( Delta, Delta );
        float Radius = ScreenWarps[I].z;
        if( Radius <= 0.0f || DistanceSquared >= Radius * Radius )
            continue;

        if( !DepthSampled )
        {
            BaseDepth = SampleWarpDepth( UV );
            DepthSampled = true;
        }
        const float SphereConstant = WarpSphereConstant( I );
        float Visibility = WarpVolumeVisibility( BaseRay, WarpSpheres[I], SphereConstant, BaseDepth );
        if( Visibility <= 0.0f )
            continue;

        float Dist = sqrt( DistanceSquared );
        float2 Dir = (Dist > 0.0001f) ? (Delta / Dist) : float2( 0.0f, 0.0f );
        float T = saturate( Dist / Radius );
        float WarpedRadius = pow( T, max( ScreenWarps[I].w, 0.001f ) ) * Radius;
        float SoftVisibility = WarpSoftVisibility( UV, I );
        float2 Displacement = Dir * (WarpedRadius - Dist) * Visibility * SoftVisibility;
        Displacement *= WarpOffsetScale( Pos.xy, BaseRay, Displacement, Visibility, I, SphereConstant );
        float2 SamplePixel = Pos.xy + Displacement;
        float2 SampleUV = SamplePixel * FilterParams2.yz;
        if( any( SampleUV < 0.0f ) || any( SampleUV > 1.0f ) )
            continue;

        float SampleVisibility = WarpVisibility( WarpViewRay( SamplePixel ), SampleUV, WarpSpheres[I], SphereConstant );
        float Strength = SoftVisibility * WarpSoftVisibility( SampleUV, I ) * SampleVisibility;
        float3 Warped = PostSource.SampleLevel( samPostSource, SampleUV, 0.0f ).rgb;
        Color.rgb = lerp( Color.rgb, Warped, Strength );
    }

    Color.a = Original.a;
    return Color;
}

//==============================================================================

float4 PS_MipDownsample( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 Texel = FilterParams5.zw;
    const float2 FirstPixel = floor( UV / Texel - 2.0f );
    const float2 Phase = frac( UV / Texel - 2.0f );
    const float2 W0 = 0.125f * ( 1.0f - Phase );
    const float2 W1 = 0.375f - 0.25f * Phase;
    const float2 W2 = 0.375f.xx;
    const float2 W3 = 0.125f + 0.25f * Phase;
    const float2 W4 = 0.125f * Phase;
    const float2 Weights[3] = { W0 + W1, W2 + W3, W4 };
    const float2 Offsets[3] = { W1 / Weights[0], 2.0f + W3 / Weights[1], 4.0f.xx };
    float4 Result = 0.0f;

    // Collapse the five phase-dependent texel weights into three bilinear pairs.
    [unroll]
    for( int Y = 0; Y < 3; ++Y )
    {
        [unroll]
        for( int X = 0; X < 3; ++X )
        {
            const float2 SampleUV = ( FirstPixel + float2( Offsets[X].x, Offsets[Y].y ) + 0.5f ) * Texel;
            Result += PostSource.SampleLevel( samPostSource, SampleUV, 0.0f ) * Weights[X].x * Weights[Y].y;
        }
    }

    return Result;
}

//==============================================================================

float4 PS_MipReduce( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 Offset = FilterParams5.zw * 0.5f;
    float4 Color = PostSource.SampleLevel( samPostSource, UV, 0.0f ) * 4.0f;
    Color += PostSource.SampleLevel( samPostSource, UV + float2(  Offset.x,  Offset.y ), 0.0f );
    Color += PostSource.SampleLevel( samPostSource, UV + float2( -Offset.x,  Offset.y ), 0.0f );
    Color += PostSource.SampleLevel( samPostSource, UV + float2( -Offset.x, -Offset.y ), 0.0f );
    Color += PostSource.SampleLevel( samPostSource, UV + float2(  Offset.x, -Offset.y ), 0.0f );
    return Color * 0.125f;
}

//==============================================================================

float4 PS_PainBlur( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 StepUV = FilterParams5.zw;
    float3 Result = 0.0f;
    float TotalWeight = 0.0f;

    [unroll]
    for( int Tap = -8; Tap <= 8; ++Tap )
    {
        const float TapPosition = (float)Tap;
        const float Weight = exp( -0.5f * TapPosition * TapPosition / 16.0f );
        Result += PostSource.SampleLevel( samPostSource, UV + StepUV * TapPosition, 0.0f ).rgb * Weight;
        TotalWeight += Weight;
    }
    Result /= TotalWeight;

    if( FilterParams6.x > 0.5f )
    {
        const float Blend = saturate( FilterParams3.w );
        const float TintWeight = pow( 1.0f - Blend, 4.0f );
        Result *= lerp( 1.0f.xxx, FilterParams3.rgb, TintWeight );
    }

    return float4( Result, 1.0f );
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

float ComputeMipVisibility( float ViewZ, float NearZ, float FarZ )
{
    const int   Fn        = (int)(FilterParams4.x + 0.5f);
    const float Param1    = FilterParams4.y;
    const float Param2    = FilterParams4.z;
    const float Range     = max( FarZ - NearZ, 1e-5f );
    float Visibility      = 1.0f;

    switch( Fn )
    {
        case 1:
        {
            const float ClampedZ = clamp( ViewZ, Param1, Param2 );
            Visibility = ( Param2 - ClampedZ ) / max( Param2 - Param1, 1e-5f );
            break;
        }

        case 2:
        {
            const float D = ( ViewZ - NearZ ) / Range;
            Visibility = 1.0f / exp( D * Param1 );
            break;
        }

        case 3:
        {
            float D = ( ViewZ - NearZ ) / Range;
            D *= Param1;
            Visibility = 1.0f / exp( D * D );
            break;
        }
    }

    return Visibility;
}

//==============================================================================

float3 SampleMipSource( float2 UV )
{
    const float Offset = FilterParams4.w;
    if( abs( Offset ) < 0.001f )
    {
        return PostSource.SampleLevel( samPostSource, UV, 0.0f ).rgb;
    }

    const float2 Jitter = float2( Offset * FilterParams5.z, Offset * FilterParams5.w );

    float3 Color = 0.0f;
    Color += PostSource.SampleLevel( samPostSource, UV + float2( -Jitter.x, -Jitter.y ), 0.0f ).rgb;
    Color += PostSource.SampleLevel( samPostSource, UV + float2(  Jitter.x, -Jitter.y ), 0.0f ).rgb;
    Color += PostSource.SampleLevel( samPostSource, UV + float2(  Jitter.x,  Jitter.y ), 0.0f ).rgb;
    Color += PostSource.SampleLevel( samPostSource, UV + float2( -Jitter.x,  Jitter.y ), 0.0f ).rgb;
    return Color * 0.25f;
}

//==============================================================================

float SampleMipDepth( float4 Pos, float2 UV )
{
#if defined(A51_SHADER_BINDING_SDL)
    const float Depth = FilterSource1.SampleLevel( samFilterSource1, UV, 0.0f ).r;
#else
    const float Depth = FilterSource1.Load( int3( int2( Pos.xy ), 0 ) ).r;
#endif
    return saturate( Depth );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_MipComposite( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float  LinearDepth = SampleMipDepth( Pos, UV );
    const int    Fn = (int)(FilterParams4.x + 0.5f);

    if( ( Fn != 0 ) && ( LinearDepth >= 0.999f ) )
    {
        return 0.0f.xxxx;
    }

    const float NearZ      = FilterParams5.x;
    const float FarZ       = FilterParams5.y;
    const float ViewZ      = lerp( NearZ, FarZ, LinearDepth );
    const float Visibility = saturate( ComputeMipVisibility( ViewZ, NearZ, FarZ ) );
    const float PaletteAlpha = (128.0f - floor( Visibility * 128.0f )) / 255.0f;
    const float PassCount    = max( floor( FilterParams6.x + 0.5f ), 1.0f );
    const float Alpha        = 1.0f - pow( 1.0f - PaletteAlpha, PassCount );

    const float3 Blurred = SampleMipSource( UV );
    return float4( Blurred * FilterParams3.rgb, Alpha );
}

//==============================================================================

float4 PS_MipCompositeCustom( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float  LinearDepth = SampleMipDepth( Pos, UV );

    if( LinearDepth >= 0.999f )
    {
        return 0.0f.xxxx;
    }

    const float3 Blurred = SampleMipSource( UV );
    const float4 Ramp = FilterRamp.SampleLevel( samFilterRamp,
                                                 float2( LinearDepth, 0.5f ),
                                                 0.0f );
    return float4( Blurred * Ramp.rgb, saturate( Ramp.a ) );
}

//==============================================================================

uint PCGHash( uint2 Pixel )
{
    uint State = Pixel.x * 1664525u + Pixel.y * 1013904223u;
    State ^= (State >> 17u);
    State *= 0xbf324c81u;
    State ^= (State >> 11u);
    State *= 0x9f324c81u;
    State ^= (State >> 16u);
    return State;
}

//==============================================================================

float4 PS_Noise( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const uint2 ScreenPixel = (uint2)Pos.xy + uint2( FilterParams6.xy );
    const float Noise       = (float)(PCGHash( ScreenPixel ) & 255u) / 255.0f;
    const float Intensity  = saturate( FilterParams3.w );
    const float Alpha      = saturate( (1.0f - Noise) * Intensity );
    const float3 GrainColor = saturate( FilterParams3.xyz );

    return float4( GrainColor, Alpha );
}

//==============================================================================

float4 PS_ScreenFade( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return FilterParams3;
}
