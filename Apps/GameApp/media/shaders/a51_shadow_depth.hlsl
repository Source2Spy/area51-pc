// Coverage is an offline shadow recipe; opaque depth has no sampled resources.
#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"
#if A51_SHADOW_MASKED
A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);
A51_CBUFFER_ATTR(1, 0) cbuffer cbShadowAlpha A51_CBUFFER_BIND(1, 0)
{
    A51_ABI_FIELDS_ShadowAlphaData
};
#endif
void PSMain( A51_SHADOW_PIXEL_INPUT Input )
{
#if A51_SHADOW_MASKED
    if( txDiffuse.Sample(samDiffuse, Input.UV + UVOffset).a < AlphaRef ) discard;
#endif
}
