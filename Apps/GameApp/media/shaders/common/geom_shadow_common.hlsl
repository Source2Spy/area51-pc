//==============================================================================
//
//  geom_shadow_common.hlsl
//
//  Shared local-shadow atlas sampling and projection helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_COMMON_HLSL
#define GEOM_SHADOW_COMMON_HLSL

#include "shadow_evsm.hlsl"

//==============================================================================
//  FUNCTIONS
//==============================================================================

float GetFaceShadowResolution( uint SourceIndex )
{
    return GetFaceShadowProjectionData( SourceIndex ).w;
}

//==============================================================================

float ComputeFaceShadowWorldTexelSize( uint SourceIndex, float3 WorldPos )
{
    const float3 LightPosition = GetFaceShadowLightPosRadius( SourceIndex ).xyz;
    const float3 FaceDirection = GetFaceShadowLightDirRadial( SourceIndex ).xyz;
    const float FaceDepth = max( dot( WorldPos - LightPosition, FaceDirection ), 0.0f );
    return FaceDepth * GetFaceShadowProjectionData( SourceIndex ).z;
}

//==============================================================================

float3 ComputeNormalBiasedShadowWorldPos( uint SourceIndex, float3 WorldPos, float3 GeometricNormal, float3 PointToLightDir )
{
    const float3 Normal          = GeometricNormal;
    const float  NormalCos       = saturate( dot( Normal, PointToLightDir ) );
    const float  NormalSin       = sqrt( saturate( 1.0f - NormalCos * NormalCos ) );
    const float  WorldTexelSize  = ComputeFaceShadowWorldTexelSize( SourceIndex, WorldPos );
    const float4 ShadowParams    = GetShadowParams();

    return WorldPos + Normal * ( WorldTexelSize * ShadowParams.y * NormalSin );
}

//==============================================================================

bool FaceShadowIsPointFace( uint SourceIndex )
{
    static const float KFaceShadowSourceTypePointFace = 1.0f;
    return GetFaceShadowLightData( SourceIndex ).w == KFaceShadowSourceTypePointFace;
}

//==============================================================================

bool ProjectFaceShadowSource( uint SourceIndex,
                              float3 ShadowWorldPos,
                              out float3 ShadowUVW )
{
    ShadowUVW = 0.0f;

    const float3 RelativePos = ShadowWorldPos - GetFaceShadowLightPosRadius( SourceIndex ).xyz;
    const float2 NearFar = GetFaceShadowProjectionData( SourceIndex ).xy;
    float4 ShadowPos = mul( FaceShadowMatrices[SourceIndex], float4( RelativePos, 0.0f ) );
    ShadowPos.z = ( NearFar.y * ShadowPos.w - NearFar.x * NearFar.y ) / ( NearFar.y - NearFar.x );
    if( ShadowPos.w <= 0.0f )
    {
        return false;
    }

    ShadowUVW.xy = ShadowPos.xy / ShadowPos.w;
    ShadowUVW.z  = ShadowPos.z / ShadowPos.w;

    const float  HalfTexel = GetShadowFilterParams().x * 0.5f;
    const float4 AtlasRect = GetFaceShadowAtlasRect( SourceIndex );
    if( ShadowUVW.x < AtlasRect.x - HalfTexel || ShadowUVW.x > AtlasRect.z + HalfTexel ||
        ShadowUVW.y < AtlasRect.y - HalfTexel || ShadowUVW.y > AtlasRect.w + HalfTexel ||
        ShadowUVW.z < 0.0f || ShadowUVW.z > 1.0f )
    {
        return false;
    }

    return true;
}

//==============================================================================

float SampleFaceShadowEVSM( uint SourceIndex, float3 ShadowWorldPos, float3 ShadowUVW )
{
    static const float KDepthBias = 1.0e-4f;

    const float4 AtlasRect = GetFaceShadowAtlasRect( SourceIndex );
    const float4 EvsmParams = GetShadowFilterParams();
    const float2 Exponents = EvsmParams.yz;
    const float4 Moments = txFaceShadowAtlas.SampleLevel(
        samFaceShadow, clamp( ShadowUVW.xy, AtlasRect.xy, AtlasRect.zw ), 0.0f );
    const float4 LightPosRadius = GetFaceShadowLightPosRadius( SourceIndex );
    const float ReceiverDepth =
        saturate( length( ShadowWorldPos - LightPosRadius.xyz ) / max( LightPosRadius.w, 1.0e-4f ) );
    const float2 WarpedDepth = WarpEVSMDepth( ReceiverDepth, Exponents );
    const float2 DepthScale = KDepthBias * Exponents * abs( WarpedDepth );
    const float2 MinimumVariance = DepthScale * DepthScale;
    const float PositiveVisibility =
        ComputeEVSMChebyshevBound( Moments.xy, WarpedDepth.x, MinimumVariance.x );
    const float NegativeVisibility =
        ComputeEVSMChebyshevBound( Moments.zw, WarpedDepth.y, MinimumVariance.y );
    const float Visibility = min( PositiveVisibility, NegativeVisibility );
    return saturate( ( Visibility - EvsmParams.w ) / max( 1.0f - EvsmParams.w, 1.0e-4f ) );
}

//==============================================================================

bool ComputeFaceShadowReceiverPlane( uint SourceIndex,
                                     float3 ShadowUVW,
                                     float3 WorldPos,
                                     float3 GeometricNormal,
                                     out float2 DepthGradient,
                                     out float DepthError )
{
    static const float KFloatEpsilon = 1.192092896e-7f;

    const float4x4 ShadowMatrix = FaceShadowMatrices[SourceIndex];
    const float3 U = ShadowMatrix[0].xyz - ShadowUVW.x * ShadowMatrix[3].xyz;
    const float3 V = ShadowMatrix[1].xyz - ShadowUVW.y * ShadowMatrix[3].xyz;
    const float3 Z = ShadowMatrix[2].xyz - ShadowUVW.z * ShadowMatrix[3].xyz;
    const float3 TangentU = cross( V, GeometricNormal );
    const float3 TangentV = cross( GeometricNormal, U );
    const float Determinant = dot( U, TangentU );
    const float Scale = dot( U, U ) * dot( V, V );

    DepthGradient = 0.0f;
    DepthError = 0.0f;
    if( Determinant * Determinant <= 1.0e-10f * Scale )
    {
        return false;
    }

    DepthGradient = float2( dot( Z, TangentU ), dot( Z, TangentV ) ) / Determinant;
    const float3 RelativePos = WorldPos - GetFaceShadowLightPosRadius( SourceIndex ).xyz;
    const float FaceDepth = dot( RelativePos, GetFaceShadowLightDirRadial( SourceIndex ).xyz );
    const float3 PlaneDepth = Z - DepthGradient.x * U - DepthGradient.y * V;
    DepthError = 4.0f * KFloatEpsilon * dot( abs( PlaneDepth ), abs( WorldPos ) ) / max( FaceDepth, 1.0e-4f );
    return true;
}

//==============================================================================

float SampleFaceShadowHard( uint SourceIndex, float3 ShadowUVW, float2 DepthGradient, float DepthError )
{
    static const float KFloatEpsilon = 1.192092896e-7f;

    const float4 AtlasRect = GetFaceShadowAtlasRect( SourceIndex );
    const float TexelSize = GetShadowParams().x;
    const float2 SampleUV = clamp( ShadowUVW.xy, AtlasRect.xy, AtlasRect.zw );
    const float2 TexelUV = ( floor( SampleUV / TexelSize ) + 0.5f ) * TexelSize;
    const float ShadowDepth = txFaceShadowAtlas.SampleLevel(
        samFaceShadow, TexelUV, 0.0f ).x;
    const float2 SampleOffset = TexelUV - ShadowUVW.xy;
    float ReceiverDepth = ShadowUVW.z + dot( DepthGradient, SampleOffset );
    const float ArithmeticScale = abs( ShadowUVW.z ) + dot( abs( DepthGradient ), abs( SampleOffset ) );
    DepthError += 8.0f * KFloatEpsilon * ArithmeticScale;
    ReceiverDepth -= DepthError;
    return saturate( ReceiverDepth ) <= ShadowDepth ? 1.0f : 0.0f;
}

//==============================================================================

bool TrySampleFaceShadow( uint SourceIndex,
                          float3 WorldPos,
                          float3 GeometricNormal,
                          float3 PointToLightDir,
                          out float Visibility )
{
    float3 ShadowUVW;
    if( !ProjectFaceShadowSource( SourceIndex, WorldPos, ShadowUVW ) )
    {
        Visibility = 1.0f;
        return false;
    }

    if( GetShadowParams().w > 0.5f )
    {
        Visibility = SampleFaceShadowEVSM( SourceIndex, WorldPos, ShadowUVW );
    }
    else
    {
        float2 DepthGradient = 0.0f;
        float DepthError = 0.0f;
        if( !ComputeFaceShadowReceiverPlane( SourceIndex, ShadowUVW, WorldPos, GeometricNormal,
                                             DepthGradient, DepthError ) )
        {
            const float3 BiasedWorldPos =
                ComputeNormalBiasedShadowWorldPos( SourceIndex, WorldPos, GeometricNormal, PointToLightDir );
            if( !ProjectFaceShadowSource( SourceIndex, BiasedWorldPos, ShadowUVW ) )
            {
                Visibility = 1.0f;
                return false;
            }
        }

        Visibility = SampleFaceShadowHard( SourceIndex, ShadowUVW, DepthGradient, DepthError );
    }
    return true;
}

//==============================================================================

float ComputeShadowNearInfluence( float LightDistance, float NearZ )
{
    const float NearFadeRange = max( NearZ, 0.05f );
    return saturate( ( LightDistance - NearZ ) / NearFadeRange );
}

//==============================================================================
#endif // GEOM_SHADOW_COMMON_HLSL
//==============================================================================
