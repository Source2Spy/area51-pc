// Dynamic deformation supplies the shared surface interface.
#include "common/geom_shader_features.hlsl"
#include "common/frame_constants.hlsl"
#include "common/geom_surface_interface.hlsl"
#include "common/shader_bindings.hlsl"
#if A51_SURFACE_INTERFACE & (64 | 128)
A51_CBUFFER_ATTR(1, 1) cbuffer cbDynamicGeometry A51_CBUFFER_BIND(1, 1)
{
    A51_ABI_FIELDS_DynamicDrawData
};
#endif
struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float4 Color : TEXCOORD3;
};
GEOM_PIXEL_INPUT VSMain( VS_INPUT Input )
{
    GEOM_PIXEL_INPUT Output;
    const float4 ViewPosition = mul(View, float4(Input.Position, 1));
    precise float4 ClipPosition = mul(Projection, ViewPosition);
    Output.Pos = ClipPosition;
#if A51_SURFACE_INTERFACE & 1
    Output.UV = Input.UV;
#endif
#if A51_SURFACE_INTERFACE & 2
    Output.Color = float4(Input.Color.bgr, 1);
#endif
#if A51_SURFACE_INTERFACE & 4
    Output.WorldPos = Input.Position;
#endif
#if A51_SURFACE_INTERFACE & 8
    Output.Normal = Input.Normal;
#endif
#if A51_SURFACE_INTERFACE & 16
    Output.ViewVector = Input.Position - CameraPosition.xyz;
#endif
#if A51_SURFACE_INTERFACE & 32
    Output.ViewDepth = ViewPosition.z;
#endif
#if A51_SURFACE_INTERFACE & 64
    Output.DrawFlags = ShaderFlags;
#endif
#if A51_SURFACE_INTERFACE & 128
    Output.LightingIndex = LightingIndex;
#endif
#if A51_SURFACE_INTERFACE & 256
    Output.FadeAlpha = 1;
#endif
    return Output;
}
