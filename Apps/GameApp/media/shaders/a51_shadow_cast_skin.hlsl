//==============================================================================
//
//  a51_shadow_cast_skin.hlsl
//
//  Shadow depth caster shader for skinned geometry.
//
//==============================================================================

//==============================================================================
//  DEFINES
//==============================================================================

#define MAX_SKIN_BONES 96

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbShadowCast A51_CBUFFER_BIND(0, 0)
{
    A51_ABI_FIELDS_ShadowCastData
};

A51_STORAGE_BUFFER_ATTR(0, 0)
StructuredBuffer<float4x4> ShadowSkinBones
    A51_STORAGE_BUFFER_BIND(0, 0);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float4 PosIndex  : TEXCOORD0;
    float4 NormIndex : TEXCOORD1;
    float4 UVWeights : TEXCOORD2;
    uint   BoneBase  : TEXCOORD3;
};

//------------------------------------------------------------------------------

#define VS_OUTPUT A51_SHADOW_PIXEL_INPUT

//==============================================================================
//  SHADERS
//==============================================================================

VS_OUTPUT VSMain( VS_INPUT Input )
{
    VS_OUTPUT Output;
    int   Index1  = (int)Input.PosIndex.w;
    int   Index2  = (int)Input.NormIndex.w;
    float Weight1 = Input.UVWeights.z;
    float Weight2 = Input.UVWeights.w;

    float3 Pos1 = mul( ShadowSkinBones[Input.BoneBase + Index1], float4( Input.PosIndex.xyz, 1.0f ) ).xyz;
    float3 Pos2 = mul( ShadowSkinBones[Input.BoneBase + Index2], float4( Input.PosIndex.xyz, 1.0f ) ).xyz;
    float3 SkinnedPos = Pos1 * Weight1 + Pos2 * Weight2;

    Output.Position = mul( ShadowViewProjection, float4( SkinnedPos - ShadowOrigin.xyz, 1.0f ) );
#if A51_SHADOW_MASKED
    Output.UV       = Input.UVWeights.xy;
#endif
    return Output;
}
