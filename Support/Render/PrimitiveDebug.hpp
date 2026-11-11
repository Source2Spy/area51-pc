//=========================================================================
//
//  PrimitiveDebug.hpp
//
//  Stateless debug geometry helpers built on the primitive and UI renderers.
//
//=========================================================================

#ifndef PRIMITIVE_DEBUG_HPP
#define PRIMITIVE_DEBUG_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PrimitiveBatch.hpp"

#include "e_View.hpp"

//=========================================================================

namespace render::debug
{

xbool Line     ( vector3 const& position0, vector3 const& position1, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Box      ( bbox const& bounds, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY,
                 primitive_raster_mode raster = PRIMITIVE_RASTER_SOLID_NO_CULL );

xbool Box      ( bbox const& bounds, matrix4 const& LocalToWorld, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY,
                 primitive_raster_mode raster = PRIMITIVE_RASTER_SOLID_NO_CULL );

xbool SolidBox ( bbox const& bounds, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY,
                 primitive_raster_mode raster = PRIMITIVE_RASTER_SOLID_NO_CULL );

xbool Sphere   ( vector3 const& position, f32 radius, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Arc      ( vector3 const& center, f32 radius, radian direction, radian FieldOfView,
                 xcolor color = XCOLOR_WHITE, f32 SegmentDensity = 0.005f,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Circle   ( vector3 const& center, f32 radius, xcolor color = XCOLOR_WHITE,
                 vector3 const& up = vector3( 0.0f, 1.0f, 0.0f ), f32 SegmentAngle = 0.005f,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Cylinder ( vector3 const& center, f32 radius, f32 height, s32 segments, xcolor color,
                 xbool capped = TRUE, vector3 const& up = vector3( 0.0f, 1.0f, 0.0f ),
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Volume   ( vector3 const& position0, vector3 const& position1, f32 width, f32 height,
                 xcolor color = XCOLOR_WHITE, primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Volume   ( bbox const& bounds, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Volume   ( bbox const& bounds, matrix4 const& LocalToWorld, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Ngon     ( vector3 const* pPoints, s32 nPoints, xcolor color = XCOLOR_WHITE, xbool wire = TRUE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Arrow    ( vector3 const& start, vector3 const& end, xcolor color = XCOLOR_WHITE,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Axis     ( matrix4 const& LocalToWorld, f32 size = 1.0f,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Axis     ( f32 size = 1.0f, primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Grid     ( vector3 const& corner, vector3 const& edge0, vector3 const& edge1, xcolor color = XCOLOR_WHITE,
                 s32 subdivisions = 8, primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Grid     ( vector3 const& corner, vector3 const& edge0, vector3 const& edge1, xcolor color,
                 s32 Edge0Subdivisions, s32 Edge1Subdivisions,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Frustum  ( view const& FrustumView, xcolor color = XCOLOR_RED, f32 distance = 100.0f,
                 primitive_depth_mode depth = PRIMITIVE_DEPTH_READ_ONLY );

xbool Marker   ( vector3 const& position, xcolor color = XCOLOR_WHITE );

xbool Point    ( vector3 const& position, xcolor color = XCOLOR_WHITE, s32 size = 2 );

xbool Label    ( vector3 const& position, xcolor color, char const* pFormat, ... );

} // namespace render::debug

//=========================================================================
#endif // PRIMITIVE_DEBUG_HPP
//=========================================================================
