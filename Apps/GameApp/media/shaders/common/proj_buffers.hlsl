//==============================================================================
//
//  proj_buffers.hlsl
//
//  Projection texture matrices shared by geometry shaders.
//
//==============================================================================

#ifndef PROJ_BUFFERS_HLSL
#define PROJ_BUFFERS_HLSL

cbuffer cbProjTextures : register(b4)
{
    float4x4 ProjLightMatrix [MAX_PROJ_LIGHTS];
    float4x4 ProjShadowMatrix[MAX_PROJ_SHADOWS];
    uint     ProjLightCount;
    uint     ProjShadowCount;
    float    EdgeSize;
    float3   ProjPadding;
};

#endif // PROJ_BUFFERS_HLSL