//==============================================================================
//
//  skin_instance_buffers.hlsl
//
//  Structured buffers for skinned instanced rendering.
//
//==============================================================================

#ifndef SKIN_INSTANCE_BUFFERS_HLSL
#define SKIN_INSTANCE_BUFFERS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #ifndef A51_SKIN_INSTANCES_BINDING
        #define A51_SKIN_INSTANCES_BINDING 9
    #endif
    #ifndef A51_SKIN_BONE_MATRICES_BINDING
        #define A51_SKIN_BONE_MATRICES_BINDING 13
    #endif
    #ifndef A51_SKIN_BONE_REMAPS_BINDING
        #define A51_SKIN_BONE_REMAPS_BINDING 14
    #endif
#else
    #define A51_SKIN_INSTANCES_BINDING      0
    #define A51_SKIN_BONE_MATRICES_BINDING  1
    #define A51_SKIN_BONE_REMAPS_BINDING    2
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_STORAGE_BUFFER_ATTR(24, A51_SKIN_INSTANCES_BINDING)
StructuredBuffer<SkinInstanceData> SkinInstances
    A51_STORAGE_BUFFER_BIND(24, A51_SKIN_INSTANCES_BINDING);
A51_STORAGE_BUFFER_ATTR(25, A51_SKIN_BONE_MATRICES_BINDING)
StructuredBuffer<SkinBoneMatrix> SkinBoneMatrices
    A51_STORAGE_BUFFER_BIND(25, A51_SKIN_BONE_MATRICES_BINDING);
A51_STORAGE_BUFFER_ATTR(26, A51_SKIN_BONE_REMAPS_BINDING)
StructuredBuffer<uint> SkinBoneRemaps
    A51_STORAGE_BUFFER_BIND(26, A51_SKIN_BONE_REMAPS_BINDING);

//==============================================================================
//  FUNCTIONS
//==============================================================================

float4x4 SkinGetBoneL2W( uint InstanceID, uint BoneRemapOffset, uint CacheIndex )
{
    const uint ClampedCache = min( CacheIndex, (uint)( MAX_SKIN_BONES - 1 ) );
    const uint BoneIndex = SkinBoneRemaps[BoneRemapOffset + ClampedCache];
    return SkinBoneMatrices[SkinInstances[InstanceID].BoneOffset + BoneIndex].L2W;
}

//==============================================================================
#endif // SKIN_INSTANCE_BUFFERS_HLSL
//==============================================================================
