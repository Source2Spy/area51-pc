//==============================================================================
//
//  bind_objects.hpp
//
//==============================================================================

//==============================================================================
//
//  Object system binding module.
//
//  Exposes the full public API of Objects/object.hpp to the script layer.
//  guids are represented as u64 integers (0 == NULL_GUID).
//
//  Angle conventions
//  ─────────────────
//  All angles exposed to scripts are in DEGREES.  This matches the Property.hpp
//  contract for PROP_TYPE_ANGLE and PROP_TYPE_ROTATION ("should be treated as
//  degrees").  Conversion to/from engine radians is handled transparently.
//
//  ──── Transform ────
//    x, y, z           = obj_getpos      ( guid )
//    obj_setpos          ( guid, x, y, z )              ── teleport (calls OnMove)
//    obj_moverel         ( guid, dx, dy, dz )            ── relative offset (calls OnMoveRel)
//    pitch, yaw, roll  = obj_getrotation ( guid )        ── Euler from L2W, in degrees
//    obj_setrotation     ( guid, pitch, yaw, roll )      ── set rotation keeping pos/scale
//
//  ──── Velocity ────
//    vx, vy, vz = obj_getvelocity( guid )     ── current velocity; (0,0,0) for statics
//
//  ──── Bounding boxes ────
//    minx,miny,minz,maxx,maxy,maxz = obj_getbbox     ( guid )  ── world-space AABB
//    minx,miny,minz,maxx,maxy,maxz = obj_getlocalbbox( guid )  ── local-space AABB
//
//  ──── Lifecycle / state ────
//    obj_activate  ( guid, bActive )          ── calls OnActivate
//    bAlive  = obj_isalive  ( guid )          ── GetHealth() > 0
//    bActive = obj_isactive ( guid )          ── ATTR_NEEDS_LOGIC_TIME is set
//    health  = obj_gethealth( guid )          ── raw health value
//    bHit    = obj_applypain( victim_guid, origin_guid,
//                             pos_x, pos_y, pos_z,
//                             pain_type_name )
//              ── inflict named pain on an object; returns TRUE if applied
//              ── pain_type_name is a PainMgr descriptor string
//                 (e.g. "GENERIC_1", "LETHAL", "EXPLOSIVE")
//
//  ──── Attribute bits ────
//    bits = obj_getattributes( guid )         ── read full 32-bit attribute word
//    obj_setattributes( guid, bits )          ── overwrite full attribute word
//    bSet = obj_hasattribute( guid, bit )     ── all bits in `bit` are set?
//    obj_enableattr ( guid, bit )             ── set bits, preserve others
//    obj_disableattr( guid, bit )             ── clear bits, preserve others
//
//    Named bit constants are in object::object_attr (Objects/object.hpp):
//      ATTR_NEEDS_LOGIC_TIME, ATTR_RENDERABLE, ATTR_COLLIDABLE,
//      ATTR_DAMAGEABLE, ATTR_LIVING, ATTR_PLAYER, etc.
//
//  ──── Zone membership ────
//    zone1, zone2 = obj_getzones ( guid )          ── both zones in one call
//    zone1        = obj_getzone1 ( guid )          ── primary zone index (0 = none)
//    zone2        = obj_getzone2 ( guid )          ── secondary zone index (0 = none)
//    obj_setzones   ( guid, zone1, zone2 )         ── assign both zone slots
//
//  ──── Identity / info ────
//    typename  = obj_gettype       ( guid )   ── type name string
//    slot      = obj_getslot       ( guid )   ── obj_mgr slot index (-1 if invalid)
//    pguid     = obj_getparentguid ( guid )   ── logical parent guid, 0 if none
//    name      = obj_getlogicalname( guid )   ── trigger-system logical name, or nil
//    extent    = obj_getlookatextent( guid )  ── camera look-at height fraction (0..1)
//
//  ──── Creation / destruction / iteration ────
//    guid  = obj_create  ( typename )         ── allocate a new instance
//    obj_destroy         ( guid )             ── queue for deletion
//    guid  = obj_getfirst( typename )         ── first live instance, or 0
//    guid  = obj_getnext ( guid )             ── next of same type, or 0
//    count = obj_getcount( typename )         ── number of live instances
//    obj_dump_types()                         ── log all types with live instances
//
//  ──── Universal property bridge ────
//    obj_prop_get ( guid, "PropName" )        ── read any engine property
//    obj_prop_set ( guid, "PropName", ... )   ── write any engine property
//    obj_prop_list( guid )                    ── debug: dump all properties
//
//    Return value count for obj_prop_get:
//      float, int, bool, angle, enum, string,
//        filename, external, guid              ──  1 value
//      vector2                                ──  2 values (x, y)
//      vector3, rotation                      ──  3 values (x/pitch, y/yaw, z/roll)
//      bbox                                   ──  6 values (minX/Y/Z, maxX/Y/Z)
//      color                                  ──  4 values (R, G, B, A  0-255)
//      not found / unsupported type           ──  nil
//
//    angle and rotation properties are in DEGREES for both get and set.
//
//  ──── Name lookup (USE_OBJECT_NAMES builds only) ────
//    name = obj_getname   ( guid )            ── assigned object name string
//    obj_setname          ( guid, name )      ── assign object name
//    guid = obj_findbyname( name )            ── linear scan; 0 if not found
//
//==============================================================================

#ifndef BIND_OBJECTS_HPP
#define BIND_OBJECTS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../script_bindings.hpp"

//==============================================================================
//  bind_objects
//==============================================================================

class bind_objects : public script_bindings
{
public:
    const char*     GetModuleName   ( void ) const override { return "objects"; }
    void            Register        ( script_backend* pBackend ) override;
};

//==============================================================================
#endif // BIND_OBJECTS_HPP
//==============================================================================