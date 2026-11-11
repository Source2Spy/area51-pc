//==============================================================================
//
//  a51_rigid_simple.hlsl
//
//  Simple rigid geometry shader.
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

#define GEOM_USE_RIGID_INSTANCE_DATA 1
#include "common/frame_constants.hlsl"
#if defined(A51_SHADER_STAGE_VERTEX)
    #include "common/rigid_instance_buffers.hlsl"
#endif


#define GEOM_HAS_VERTEX_COLOR 1

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position   : TEXCOORD0;
    float3 Normal     : TEXCOORD1;
    float2 UV         : TEXCOORD2;
    uint   InstanceIndex : TEXCOORD3;
    uint   VertexIndex   : TEXCOORD4;
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

    const float4 WorldPos = mul(RigidInstances[InstanceID].World, float4(Input.Position,1));
    const float4 ViewPos = mul(View,WorldPos);
    precise float4 ClipPos = mul(Projection,ViewPos);
    Output.Pos = ClipPos;
#if A51_SURFACE_INTERFACE & 1
    Output.UV = Input.UV + UVAnim.xy;
#endif
#if A51_SURFACE_INTERFACE & 2
    Output.Color = float4(0,0,0,1);
    if( RigidInstances[InstanceID].ColorOffset != 0xffffffffu )
        Output.Color = DecodeRigidVertexColor(RigidVertexColors[RigidInstances[InstanceID].ColorOffset + Input.VertexIndex]);
#endif
#if A51_SURFACE_INTERFACE & 4
    Output.WorldPos = WorldPos.xyz;
#endif
#if A51_SURFACE_INTERFACE & 8
    Output.Normal = normalize(mul((float3x3)RigidInstances[InstanceID].World,Input.Normal));
#endif
#if A51_SURFACE_INTERFACE & 16
    Output.ViewVector = WorldPos.xyz - CameraPosition.xyz;
#endif
#if A51_SURFACE_INTERFACE & 32
    Output.ViewDepth = ViewPos.z;
#endif
#if A51_SURFACE_INTERFACE & 64
    Output.DrawFlags = RigidInstances[InstanceID].ShaderFlags;
#endif
#if A51_SURFACE_INTERFACE & 128
    Output.LightingIndex = RigidInstances[InstanceID].LightingIndex;
#endif
#if A51_SURFACE_INTERFACE & 256
    Output.FadeAlpha = RigidInstances[InstanceID].FadeAlpha;
#endif
    return Output;
}
#endif
