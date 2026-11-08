//==============================================================================
//
//  a51_shadow_cast_rigid.hlsl
//
//  Shadow depth caster shader for rigid geometry.
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

A51_CBUFFER_ATTR(0, 0) cbuffer cbShadowCast A51_CBUFFER_BIND(0, 0)
{
    A51_ABI_FIELDS_ShadowCastData
};

A51_STORAGE_BUFFER_ATTR(0, 0)
StructuredBuffer<float4x4> ShadowRigidWorld
    A51_STORAGE_BUFFER_BIND(0, 0);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 UV       : TEXCOORD2;
    uint   InstanceIndex : TEXCOORD3;
};

//------------------------------------------------------------------------------

#define VS_OUTPUT A51_SHADOW_PIXEL_INPUT

//==============================================================================
//  SHADERS
//==============================================================================

VS_OUTPUT VSMain( VS_INPUT Input )
{
    VS_OUTPUT Output;
    float4 WorldPos = mul( ShadowRigidWorld[Input.InstanceIndex],
                           float4( Input.Position, 1.0f ) );
    WorldPos.xyz -= ShadowOrigin.xyz;
    Output.Position = mul( ShadowViewProjection, WorldPos );
#if A51_SHADOW_MASKED
    Output.UV       = Input.UV;
#endif
    return Output;
}
