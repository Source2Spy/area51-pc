//=============================================================================
//
//  FX Render Extraction
//
//  Converts the serialized FX material contract into renderer-owned material
//  descriptors. No render-pass or GPU state is owned by the FX runtime.
//
//=============================================================================

#ifndef FX_RENDER_HPP
#define FX_RENDER_HPP

//=============================================================================

#include "Render/PrimitiveBatch.hpp"

//=============================================================================

enum fx_mesh_topology
{
    FX_MESH_TRIANGLE_LIST = 0,
    FX_MESH_TRIANGLE_STRIP
};

//=============================================================================

render::primitive_draw_desc fx_BuildMaterial( s32 CombineMode, xbool ReadZ,
                                              render::primitive_sampler_mode Sampler );

render::primitive_draw_desc fx_CreateMaterial( const texture& Texture,
                                                s32            CombineMode,
                                                xbool          ReadZ,
                                                render::primitive_sampler_mode Sampler );

xbool fx_SubmitMesh( const render::primitive_draw_desc& Material,
                     const matrix4&                         LocalToWorld,
                     const vector3*                         pPositions,
                     const vector2*                         pUVs,
                     const xcolor*                          pColors,
                     s32                                    nVertices,
                     const s16*                             pIndices,
                     s32                                    nIndices,
                     fx_mesh_topology                       Topology );

//=============================================================================
#endif // FX_RENDER_HPP
//=============================================================================
