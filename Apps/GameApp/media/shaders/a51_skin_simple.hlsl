//==============================================================================
//
//  a51_skin_simple.hlsl
//
//  Simple skinned geometry shader for A51.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/material_flags.hlsl"

#include "common/geom_shader_features.hlsl"

//==============================================================================
//  GEOMETRY RESOURCES
//==============================================================================

#define GEOM_USE_SKIN_INSTANCE_DATA 1
#include "common/frame_constants.hlsl"
#if defined(A51_SHADER_STAGE_VERTEX)
    #include "common/skin_instance_buffers.hlsl"
#endif


//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float4 PosIndex  : TEXCOORD0;
    float4 NormIndex : TEXCOORD1;
    float4 UVWeights : TEXCOORD2;
    uint   InstanceIndex : TEXCOORD3;
    uint   BoneRemapOffset : TEXCOORD4;
};

//------------------------------------------------------------------------------

#include "common/geom_surface_interface.hlsl"

//==============================================================================
//  SHADERS
//==============================================================================

#if defined(A51_SHADER_STAGE_VERTEX)
GEOM_PIXEL_INPUT VSMain( VS_INPUT Input )
{
    GEOM_PIXEL_INPUT Output;
    const uint InstanceID = Input.InstanceIndex;
    const uint BoneRemapOffset = Input.BoneRemapOffset;

    uint  Index1  = (uint)Input.PosIndex.w;
    uint  Index2  = (uint)Input.NormIndex.w;
    float Weight1 = Input.UVWeights.z;
    float Weight2 = Input.UVWeights.w;

    const float4x4 Bone1 = SkinGetBoneL2W(InstanceID,BoneRemapOffset,Index1);
    const float4x4 Bone2 = SkinGetBoneL2W(InstanceID,BoneRemapOffset,Index2);
    const float3 Pos1 = mul(Bone1,float4(Input.PosIndex.xyz,1)).xyz;
    const float3 Pos2 = mul(Bone2,float4(Input.PosIndex.xyz,1)).xyz;
    const float4 WorldPos = float4(Pos1*Weight1+Pos2*Weight2,1);
    const float4 ViewPos = mul(View,WorldPos);
    precise float4 ClipPos = mul(Projection,ViewPos);
    Output.Pos = ClipPos;
#if A51_SURFACE_INTERFACE & 1
    Output.UV = Input.UVWeights.xy + UVAnim.xy;
#endif
#if A51_SURFACE_INTERFACE & 2
    Output.Color = float4(0,0,0,1);
#endif
#if A51_SURFACE_INTERFACE & 4
    Output.WorldPos = WorldPos.xyz;
#endif
#if A51_SURFACE_INTERFACE & 8
    Output.Normal = normalize(mul((float3x3)Bone1,Input.NormIndex.xyz)*Weight1 + mul((float3x3)Bone2,Input.NormIndex.xyz)*Weight2);
#endif
#if A51_SURFACE_INTERFACE & 16
    Output.ViewVector = WorldPos.xyz - CameraPosition.xyz;
#endif
#if A51_SURFACE_INTERFACE & 32
    Output.ViewDepth = ViewPos.z;
#endif
#if A51_SURFACE_INTERFACE & 64
    Output.DrawFlags = SkinInstances[InstanceID].ShaderFlags;
#endif
#if A51_SURFACE_INTERFACE & 128
    Output.LightingIndex = SkinInstances[InstanceID].LightingIndex;
#endif
#if A51_SURFACE_INTERFACE & 256
    Output.FadeAlpha = SkinInstances[InstanceID].FadeAlpha;
#endif
    return Output;
}
#endif
