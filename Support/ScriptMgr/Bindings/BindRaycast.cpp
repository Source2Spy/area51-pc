//==============================================================================
//
//  BindRaycast.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "BindRaycast.hpp"
#include "Entropy.hpp"
#include "CollisionMgr/CollisionMgr.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "../../../MiscUtils/SimpleUtils.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
inline guid ArgGuid( ScriptContext& Context, s32 Index )
{
    return guid( Context.ArgU64( Index ) );
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
s32 PushHit( ScriptContext& Context, const collision_mgr::collision& Hit )
{
    const vector3& Pt = Hit.Point;
    const vector3& N  = Hit.Plane.Normal;

    Context.PushFloat( Pt.GetX() );
    Context.PushFloat( Pt.GetY() );
    Context.PushFloat( Pt.GetZ() );
    Context.PushFloat( N.GetX()  );
    Context.PushFloat( N.GetY()  );
    Context.PushFloat( N.GetZ()  );
    Context.PushU64  ( (u64)Hit.ObjectHitGuid );
    return 7;
}

//==============================================================================
//  BINDING FUNCTIONS
//==============================================================================

static
s32 BindRaycastFromCam( ScriptContext& Context )
{
    f32         dist  = Context.ArgFloat( 0 );
    const view* pView = eng_GetView();

    if( !pView || dist <= 0.0f )
    {
        Context.PushU64( 0 );
        return 1;
    }

    vector3 Start = pView->GetPosition();
    vector3 End   = Start + pView->GetViewZ() * dist;

    if( !CastRay( SMP_UTIL_GetActivePlayerGuid(), Start, End ) )
    {
        Context.PushU64( 0 );
        return 1;
    }

    Context.PushU64( (u64)g_CollisionMgr.m_Collisions[0].ObjectHitGuid );
    return 1;
}

//==============================================================================

static
s32 BindRaycastFromCamPos( ScriptContext& Context )
{
    f32         dist  = Context.ArgFloat( 0 );
    const view* pView = eng_GetView();

    if( !pView || dist <= 0.0f )
    {
        Context.PushNil();
        return 1;
    }

    vector3 Start = pView->GetPosition();
    vector3 End   = Start + pView->GetViewZ() * dist;

    if( !CastRay( SMP_UTIL_GetActivePlayerGuid(), Start, End ) )
    {
        Context.PushNil();
        return 1;
    }

    return PushHit( Context, g_CollisionMgr.m_Collisions[0] );
}

//==============================================================================

static
s32 BindRaycastWorld( ScriptContext& Context )
{
    vector3 Start( Context.ArgFloat(0), Context.ArgFloat(1), Context.ArgFloat(2) );
    vector3 End  ( Context.ArgFloat(3), Context.ArgFloat(4), Context.ArgFloat(5) );

    if( !CastRay( NULL_GUID, Start, End ) )
    {
        Context.PushNil();
        return 1;
    }

    return PushHit( Context, g_CollisionMgr.m_Collisions[0] );
}

//==============================================================================

static
s32 BindRaycastLOS( ScriptContext& Context )
{
    vector3 Start( Context.ArgFloat(0), Context.ArgFloat(1), Context.ArgFloat(2) );
    vector3 End  ( Context.ArgFloat(3), Context.ArgFloat(4), Context.ArgFloat(5) );

    xbool bBlocked = CastLOS( NULL_GUID, Start, End );
    Context.PushBool( !bBlocked );
    return 1;
}

//==============================================================================

static
s32 BindRaycastFromObject( ScriptContext& Context )
{
    guid    ObjGuid = ArgGuid( Context, 0 );
    f32     dist    = Context.ArgFloat( 1 );
    object* pObj    = g_ObjMgr.GetObjectByGuid( ObjGuid );

    if( !pObj || dist <= 0.0f )
    {
        Context.PushNil();
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
        Context.PushNil();
        return 1;
    }

    return PushHit( Context, g_CollisionMgr.m_Collisions[0] );
}

//==============================================================================
//  REGISTER
//==============================================================================

void BindRaycast::Register( ScriptBackend* pBackend )
{
    pBackend->RegisterFunction( "raycast_from_cam",     BindRaycastFromCam    );
    pBackend->RegisterFunction( "raycast_from_cam_pos", BindRaycastFromCamPos );
    pBackend->RegisterFunction( "raycast_world",        BindRaycastWorld      );
    pBackend->RegisterFunction( "raycast_los",          BindRaycastLOS        );
    pBackend->RegisterFunction( "raycast_from_object",  BindRaycastFromObject );
}
