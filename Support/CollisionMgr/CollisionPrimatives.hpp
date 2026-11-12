//=========================================================================
//
//  CollisionPrimatives.hpp
//
//=========================================================================

#ifndef COLLISION_PRIMITIVES_HPP
#define COLLISION_PRIMITIVES_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"

//=========================================================================
//  CONSTANTS
//=========================================================================

static const f32 CAPSULE_CAST_EPSILON = 0.001f;
static const f32 CAPSULE_PENETRATION_TOLERANCE = 0.1f;

//=========================================================================
//  CAPSULE QUERIES
//=========================================================================

// A and B are the sphere centers at the ends of the capsule axis. Delta is
// the full movement vector; successful sweeps return its fraction in T.

xbool ComputeCapsuleTriCollision       ( const vector3* Triangle,
                                         const vector3& A,
                                         const vector3& B,
                                         const vector3& Delta,
                                         f32            Radius,
                                         f32&           T,
                                         vector3&       Point,
                                         vector3&       Normal,
                                         xbool&         HitEdge );

xbool ComputeCapsuleTriPenetration     ( const vector3* Triangle,
                                         const vector3& A,
                                         const vector3& B,
                                         f32            Radius,
                                         vector3&       Correction );

xbool ComputeCapsuleCapsuleCollision   ( const vector3& A,
                                         const vector3& B,
                                         const vector3& Delta,
                                         f32            Radius,
                                         const vector3& C,
                                         const vector3& D,
                                         f32            OtherRadius,
                                         f32&           T,
                                         vector3&       Point,
                                         vector3&       Normal );

f32   ComputeCapsuleCapsuleSeparation  ( const vector3& A,
                                         const vector3& B,
                                         f32            Radius,
                                         const vector3& C,
                                         const vector3& D,
                                         f32            OtherRadius,
                                         const vector3& CoincidentNormal,
                                         vector3&       Normal );

xbool ComputeCapsuleCapsulePenetration ( const vector3& A,
                                         const vector3& B,
                                         f32            Radius,
                                         const vector3& C,
                                         const vector3& D,
                                         f32            OtherRadius,
                                         const vector3& CoincidentNormal,
                                         vector3&       Correction );

xbool ComputeCapsuleAABBoxOverlap      ( const vector3& A,
                                         const vector3& B,
                                         f32            Radius,
                                         const bbox&    Bounds );

//=========================================================================
//  SPHERE AND RAY QUERIES
//=========================================================================

xbool ComputeSphereTriCollision        ( const vector3* Tri,
                                         const vector3& Start,
                                         const vector3& End,
                                         f32            Radius,
                                         f32&           FinalT,
                                         vector3&       FinalHitPoint );

xbool ComputeRayTriCollision           ( const vector3* Tri,
                                         const vector3& Start,
                                         const vector3& End,
                                         f32&           FinalT,
                                         vector3&       FinalHitPoint );

xbool ComputeRaySphereCollision        ( const vector3& SpherePos,
                                         const f32      SphereRadius,
                                         const vector3& Start,
                                         const vector3& End,
                                         f32&           FinalT,
                                         vector3&       FinalHitPoint );

xbool ComputeSphereSphereCollision     ( const vector3& TestSpherePos,
                                         const f32      TestSphereRadius,
                                         const f32      MovingSphereRadius,
                                         const vector3& Start,
                                         const vector3& End,
                                         f32&           FinalT,
                                         vector3&       FinalHitPoint );

xbool ComputeSphereAABBoxCollision     ( const bbox&    AABBox,
                                         const vector3& Start,
                                         const vector3& End,
                                         f32            Radius,
                                         f32&           FinalT,
                                         vector3&       FinalHitPoint,
                                         plane&         FinalHitPlane,
                                         plane&         FinalSlipPlane );

xbool ComputeRayAABBoxCollision        ( const bbox&    AABBox,
                                         const vector3& Start,
                                         const vector3& End,
                                         f32&           FinalT,
                                         vector3&       FinalHitPoint,
                                         plane&         FinalHitPlane,
                                         plane&         FinalSlipPlane );

#endif // COLLISION_PRIMITIVES_HPP
//=========================================================================
