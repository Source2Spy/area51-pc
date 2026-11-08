//==============================================================================
//
//  geom_shadow_spot.hlsl
//
//  Spot-light shadow sampling helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_SPOT_HLSL
#define GEOM_SHADOW_SPOT_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float SampleFaceShadowSource( uint SourceIndex,
                              float3 WorldPos,
                              float3 GeometricNormal,
                              float3 PointToLightDir,
                              float LightDistance )
{
    if( SourceIndex >= GetFaceShadowCount() )
    {
        return 1.0f;
    }

    const float4 LightPosRadius  = GetFaceShadowLightPosRadius( SourceIndex );
    const float4 LightDirRadial = GetFaceShadowLightDirRadial( SourceIndex );
    const float4 LightData       = GetFaceShadowLightData( SourceIndex );
    const float3 LightDir        = LightDirRadial.xyz;
    const float  CosOuter        = LightData.x;
    const float  NearZ           = LightData.y;
    const bool   IsPointFace     = FaceShadowIsPointFace( SourceIndex );

    if( IsPointFace )
    {
        return 1.0f;
    }

    if( LightDistance <= 1e-4f )
    {
        return 1.0f;
    }

    const float ShadowInfluence = GeomComputeRadialAttenuation( LightDistance, float2( LightData.z, LightDirRadial.w ) );
    if( ShadowInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float  ConeCos         = dot( LightDir, -PointToLightDir );
    const float  ConeRange       = max( 1.0f - CosOuter, 1e-4f );
    const float  ConeInfluence   = saturate( ( ConeCos - CosOuter ) / ConeRange );
    if( ConeInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float NearInfluence = ComputeShadowNearInfluence( LightDistance, NearZ );
    if( NearInfluence <= 0.0f )
    {
        return 1.0f;
    }

    float Visibility;
    if( !TrySampleFaceShadow( SourceIndex, WorldPos, GeometricNormal, PointToLightDir, Visibility ) )
    {
        return 1.0f;
    }

    return lerp( 1.0f, Visibility, NearInfluence );
}

//==============================================================================
#endif // GEOM_SHADOW_SPOT_HLSL
//==============================================================================
