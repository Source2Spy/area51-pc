//==============================================================================
//
//  bind_raycast.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "bind_raycast.hpp"
#include "Entropy.hpp"                        
#include "CollisionMgr/CollisionMgr.hpp"      
#include "Obj_mgr/obj_mgr.hpp"                
#include "../../../MiscUtils/SimpleUtils.hpp" 

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
inline guid ArgGuid( script_context& ctx, s32 Index )
{
    return guid( ctx.ArgU64( Index ) );
}

//==============================================================================

static 
xbool CastRay( guid MovingObj, const vector3& Start, const vector3& End )
{
    g_CollisionMgr.RaySetup( MovingObj, Start, End );
    return g_CollisionMgr.CheckCollisions();
}

//==============================================================================

static 
xbool CastLOS( guid MovingObj, const vector3& Start, const vector3& End )
{
    g_CollisionMgr.LineOfSightSetup( MovingObj, Start, End );
    return g_CollisionMgr.CheckCollisions();
}

//==============================================================================

static 
s32 PushHit( script_context& ctx, const collision_mgr::collision& Hit )
{
    const vector3& Pt = Hit.Point;
    const vector3& N  = Hit.Plane.Normal;

    ctx.PushFloat( Pt.GetX() );
    ctx.PushFloat( Pt.GetY() );
    ctx.PushFloat( Pt.GetZ() );
    ctx.PushFloat( N.GetX()  );
    ctx.PushFloat( N.GetY()  );
    ctx.PushFloat( N.GetZ()  );
    ctx.PushU64  ( (u64)Hit.ObjectHitGuid );
    return 7;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

static 
s32 bind_raycast_from_cam( script_context& ctx )
{
    f32         dist  = ctx.ArgFloat( 0 );
    const view* pView = eng_GetView();

    if( !pView || dist <= 0.0f )
    {
        ctx.PushU64( 0 );
        return 1;
    }

    vector3 Start = pView->GetPosition();
    vector3 End   = Start + pView->GetViewZ() * dist;

    if( !CastRay( SMP_UTIL_GetActivePlayerGuid(), Start, End ) )
    {
        ctx.PushU64( 0 );
        return 1;
    }

    ctx.PushU64( (u64)g_CollisionMgr.m_Collisions[0].ObjectHitGuid );
    return 1;
}

//==============================================================================

static 
s32 bind_raycast_from_cam_pos( script_context& ctx )
{
    f32         dist  = ctx.ArgFloat( 0 );
    const view* pView = eng_GetView();

    if( !pView || dist <= 0.0f )
    {
        ctx.PushNil();
        return 1;
    }

    vector3 Start = pView->GetPosition();
    vector3 End   = Start + pView->GetViewZ() * dist;

    if( !CastRay( SMP_UTIL_GetActivePlayerGuid(), Start, End ) )
    {
        ctx.PushNil();
        return 1;
    }

    return PushHit( ctx, g_CollisionMgr.m_Collisions[0] );
}

//==============================================================================

static 
s32 bind_raycast_world( script_context& ctx )
{
    vector3 Start( ctx.ArgFloat(0), ctx.ArgFloat(1), ctx.ArgFloat(2) );
    vector3 End  ( ctx.ArgFloat(3), ctx.ArgFloat(4), ctx.ArgFloat(5) );

    if( !CastRay( NULL_GUID, Start, End ) )
    {
        ctx.PushNil();
        return 1;
    }

    return PushHit( ctx, g_CollisionMgr.m_Collisions[0] );
}

//==============================================================================

static 
s32 bind_raycast_los( script_context& ctx )
{
    vector3 Start( ctx.ArgFloat(0), ctx.ArgFloat(1), ctx.ArgFloat(2) );
    vector3 End  ( ctx.ArgFloat(3), ctx.ArgFloat(4), ctx.ArgFloat(5) );

    xbool bBlocked = CastLOS( NULL_GUID, Start, End );
    ctx.PushBool( !bBlocked );
    return 1;
}

//==============================================================================

static 
s32 bind_raycast_from_object( script_context& ctx )
{
    guid    ObjGuid = ArgGuid( ctx, 0 );
    f32     dist    = ctx.ArgFloat( 1 );
    object* pObj    = g_ObjMgr.GetObjectByGuid( ObjGuid );

    if( !pObj || dist <= 0.0f )
    {
        ctx.PushNil();
        return 1;
    }

    const matrix4& L2W = pObj->GetL2W();
    vector3        ColX, ColY, Forward;
    L2W.GetColumns( ColX, ColY, Forward );
    Forward.Normalize();

    vector3 Start = pObj->GetPosition();
    vector3 End   = Start + Forward * dist;

    if( !CastRay( ObjGuid, Start, End ) )
    {
        ctx.PushNil();
        return 1;
    }

    return PushHit( ctx, g_CollisionMgr.m_Collisions[0] );
}

//==============================================================================
//  REGISTER
//==============================================================================

void bind_raycast::Register( script_backend* pBackend )
{
    pBackend->RegisterFunction( "raycast_from_cam",     bind_raycast_from_cam     );
    pBackend->RegisterFunction( "raycast_from_cam_pos", bind_raycast_from_cam_pos );
    pBackend->RegisterFunction( "raycast_world",        bind_raycast_world        );
    pBackend->RegisterFunction( "raycast_los",          bind_raycast_los          );
    pBackend->RegisterFunction( "raycast_from_object",  bind_raycast_from_object  );
}