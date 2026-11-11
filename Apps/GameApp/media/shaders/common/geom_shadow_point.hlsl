//==============================================================================
//
//  geom_shadow_point.hlsl
//
//  Point-light shadow sampling helpers.
//
//==============================================================================

#ifndef GEOM_SHADOW_POINT_HLSL
#define GEOM_SHADOW_POINT_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

#include "point_shadow_faces.hlsl"

//==============================================================================

float SamplePointShadowLight( uint LightIndex,
                              float3 WorldPos,
                              float3 GeometricNormal,
                              float3 PointToLightDir,
                              float LightDistance )
{
    const float4 LightPosRadius  = GetPointShadowLightPosRadius( LightIndex );
    const float4 LightData       = GetPointShadowLightData( LightIndex );
    const float4 LightParams     = GetPointShadowLightParams( LightIndex );
    const float  NearZ           = LightData.y;
    const uint   FirstFaceIndex  = min( (uint)LightParams.x,
                                       (uint)MAX_SHADOW_SOURCES );
    const uint   FaceCount       = min( (uint)LightParams.y,
                                       (uint)POINT_SHADOW_FACE_COUNT );

    if( ( FaceCount == 0u ) || ( FirstFaceIndex >= GetFaceShadowCount() ) )
    {
        return 1.0f;
    }

    if( LightDistance <= 1e-4f )
    {
        return 1.0f;
    }

    const float ShadowInfluence = GeomComputeRadialAttenuation( LightDistance, LightData.xz );
    if( ShadowInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float NearInfluence = ComputeShadowNearInfluence( LightDistance, NearZ );
    if( NearInfluence <= 0.0f )
    {
        return 1.0f;
    }

    const float3 LightDir = -PointToLightDir;

    // CPU face order is +Y, -Y, +Z, -Z, +X, -X, including partial prefixes.
    const PointShadowFacePair Yz = MergePointShadowFaces(
        PointShadowAxisFaces( LightDir.y, 0u, FaceCount ),
        PointShadowAxisFaces( LightDir.z, 2u, FaceCount ) );
    const PointShadowFacePair Faces = MergePointShadowFaces(
        Yz, PointShadowAxisFaces( LightDir.x, 4u, FaceCount ) );
    const uint BestSourceIndex = FirstFaceIndex + Faces.First;
    const uint SecondSourceIndex = Faces.Second < FaceCount
                                       ? FirstFaceIndex + Faces.Second : MAX_SHADOW_SOURCES;
    const float BestAlignment = Faces.FirstAlignment;
    const float SecondAlignment = Faces.SecondAlignment;

    if( BestSourceIndex >= GetFaceShadowCount() )
    {
        return 1.0f;
    }

    float BestVisibility;
    const bool BestValid =
        TrySampleFaceShadow( BestSourceIndex, WorldPos, GeometricNormal, PointToLightDir, BestVisibility );

    if( SecondSourceIndex >= GetFaceShadowCount() )
    {
        return BestValid ? lerp( 1.0f, BestVisibility, NearInfluence ) : 1.0f;
    }

    const float SeamBlendWidth = ( 2.0f * GetShadowParams().z ) / GetFaceShadowResolution( BestSourceIndex );
    const float AlignmentGap   = ( BestAlignment - SecondAlignment ) / max( SecondAlignment, 1.0e-4f );
    if( BestValid && AlignmentGap >= SeamBlendWidth )
    {
        return lerp( 1.0f, BestVisibility, NearInfluence );
    }

    float SecondVisibility;
    const bool SecondValid =
        TrySampleFaceShadow( SecondSourceIndex, WorldPos, GeometricNormal, PointToLightDir, SecondVisibility );
    if( !BestValid )
    {
        return SecondValid ? lerp( 1.0f, SecondVisibility, NearInfluence ) : 1.0f;
    }
    if( !SecondValid )
    {
        return lerp( 1.0f, BestVisibility, NearInfluence );
    }

    const float SeamWeight = smoothstep( 0.0f, SeamBlendWidth, AlignmentGap );
    const float Visibility = lerp( max( BestVisibility, SecondVisibility ), BestVisibility, SeamWeight );
    return lerp( 1.0f, Visibility, NearInfluence );
}

//==============================================================================
#endif // GEOM_SHADOW_POINT_HLSL
//==============================================================================
