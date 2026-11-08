//==============================================================================
//
//  rigid_instance_buffers.hlsl
//
//  Structured buffers for rigid instanced rendering.
//
//==============================================================================

#ifndef RIGID_INSTANCE_BUFFERS_HLSL
#define RIGID_INSTANCE_BUFFERS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #ifndef A51_RIGID_INSTANCES_BINDING
        #define A51_RIGID_INSTANCES_BINDING 9
    #endif
    #ifndef A51_RIGID_VERTEX_COLORS_BINDING
        #define A51_RIGID_VERTEX_COLORS_BINDING 13
    #endif
#else
    #define A51_RIGID_INSTANCES_BINDING     0
    #define A51_RIGID_VERTEX_COLORS_BINDING 1
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_STORAGE_BUFFER_ATTR(22, A51_RIGID_INSTANCES_BINDING)
StructuredBuffer<RigidInstanceData> RigidInstances
    A51_STORAGE_BUFFER_BIND(22, A51_RIGID_INSTANCES_BINDING);
#if A51_SURFACE_INTERFACE & 2
    A51_STORAGE_BUFFER_ATTR(23, A51_RIGID_VERTEX_COLORS_BINDING)
    StructuredBuffer<uint> RigidVertexColors
        A51_STORAGE_BUFFER_BIND(23, A51_RIGID_VERTEX_COLORS_BINDING);

#endif
//==============================================================================
//  FUNCTIONS
//==============================================================================

float4 DecodeRigidVertexColor( uint PackedColor )
{
    return float4( (float)(( PackedColor >> 16 ) & 0xFFu),
                   (float)(( PackedColor >>  8 ) & 0xFFu),
                   (float)(  PackedColor         & 0xFFu),
                   (float)(( PackedColor >> 24 ) & 0xFFu) ) * (1.0f / 255.0f);
}

//==============================================================================
#endif // RIGID_INSTANCE_BUFFERS_HLSL
//==============================================================================
