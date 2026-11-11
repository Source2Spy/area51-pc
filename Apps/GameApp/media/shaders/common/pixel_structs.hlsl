//==============================================================================
//
//  pixel_structs.hlsl
//
//  Shared pixel shader output structures.
//
//==============================================================================

#ifndef GEOM_PIXEL_STRUCTS_HLSL
#define GEOM_PIXEL_STRUCTS_HLSL

//==============================================================================
//  TYPES
//==============================================================================

struct GeomPixelResult
{
    float4 Color;
    float4 Bloom;
};

//------------------------------------------------------------------------------

struct GeomSceneOutput
{
    float4 Color : SV_Target0;
#if defined(A51_SCENE_DEPTH_TARGET)
    float Depth : SV_Target1;
#elif A51_GEOM_NORMAL_TARGET
    float4 Normal : SV_Target1;
#endif
};

//==============================================================================
#endif // GEOM_PIXEL_STRUCTS_HLSL
//==============================================================================
