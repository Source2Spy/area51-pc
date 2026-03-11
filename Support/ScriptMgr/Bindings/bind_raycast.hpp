//==============================================================================
//
//  bind_raycast.hpp
//
//==============================================================================

//==============================================================================
//
//  Exposed functions:
//
//  ── Camera raycasts ──────────────────────────────────────────────────────────
//  
//    guid = raycast_from_cam( dist )
//        Casts a ray from the current camera position along the view axis.
//        Returns the guid of the first object hit, or 0 on miss.
//  
//    x,y,z, nx,ny,nz, guid = raycast_from_cam_pos( dist )
//        Same ray; returns the world-space hit point, surface normal, and guid.
//        Returns nil on miss.
//  
//  ── World-space raycasts ─────────────────────────────────────────────────────
//  
//    x,y,z, nx,ny,nz, guid = raycast_world( x1,y1,z1, x2,y2,z2 )
//        Casts a ray between two arbitrary world-space points.
//        Returns the hit point, surface normal, and guid, or nil on miss.
//  
//    visible = raycast_los( x1,y1,z1, x2,y2,z2 )
//        Line-of-sight test (glass is ignored, stops at first hit).
//        Returns true if the segment is unobstructed.
//  
//  ── Object-relative raycasts ─────────────────────────────────────────────────
//  
//    x,y,z, nx,ny,nz, guid = raycast_from_object( obj_guid, dist )
//        Casts a ray from the object's world position along its local forward
//        (Z) axis.  The object itself is excluded from the collision test.
//        Returns the hit point, surface normal, and guid, or nil on miss.
//
//==============================================================================

#ifndef BIND_RAYCAST_HPP
#define BIND_RAYCAST_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../script_bindings.hpp"

//==============================================================================
//  BIND_RAYCAST
//==============================================================================

class bind_raycast : public script_bindings
{
public:
    const char* GetModuleName( void ) const override { return "raycast"; }
    void        Register     ( script_backend* pBackend ) override;
};

//==============================================================================
#endif // BIND_RAYCAST_HPP
//==============================================================================