//==============================================================================
//
//  BindObjects.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "BindObjects.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/object.hpp"
#include "MiscUtils/Property.hpp"
#include "PainMgr/Pain.hpp"
#include "Characters/Character.hpp"
#include "Objects/Player/Player.hpp"
#include "Navigation/Nav_Map.hpp"
#include "../../../MiscUtils/SimpleUtils.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

static const s32 PROP_STRING_BUFSIZE = 256;

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
inline void PushGuid( ScriptContext& Context, guid G )
{
    Context.PushU64( (u64)G );
}

//==============================================================================

static
inline void PushVector3( ScriptContext& Context, const vector3& V )
{
    Context.PushFloat( V.GetX() );
    Context.PushFloat( V.GetY() );
    Context.PushFloat( V.GetZ() );
}

//==============================================================================

static
inline void PushBBox( ScriptContext& Context, const bbox& B )
{
    Context.PushFloat( B.Min.GetX() );
    Context.PushFloat( B.Min.GetY() );
    Context.PushFloat( B.Min.GetZ() );
    Context.PushFloat( B.Max.GetX() );
    Context.PushFloat( B.Max.GetY() );
    Context.PushFloat( B.Max.GetZ() );
}

//==============================================================================

static
object_desc* FindDescByName( const char* pTypeName )
{
    for( object_desc* p = object_desc::GetFirstType(); p; p = object_desc::GetNextType(p) )
    {
        if( x_stricmp( p->GetTypeName(), pTypeName ) == 0 )
            return p;
    }
    return NULL;
}

//==============================================================================

static
prop_type FindPropType( object* pObj, const char* pPropName, prop_enum& OutEnum )
{
    pObj->OnEnumProp( OutEnum );
    for( s32 i = 0; i < OutEnum.GetCount(); i++ )
    {
        if( x_stricmp( OutEnum[i].GetName(), pPropName ) == 0 )
            return (prop_type)( OutEnum[i].GetType() & PROP_TYPE_BASIC_MASK );
    }
    return PROP_TYPE_NULL;
}

//==============================================================================
//  BINDING FUNCTIONS
//==============================================================================

static
s32 BindObjGetPos( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil(); Context.PushNil(); Context.PushNil();
        return 3;
    }
    PushVector3( Context, pObj->GetPosition() );
    return 3;
}

//==============================================================================

static
s32 BindObjSetPos( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
    {
        vector3 NewPos( Context.ArgFloat(1), Context.ArgFloat(2), Context.ArgFloat(3) );
        if( pObj->GetType() == object::TYPE_PLAYER )
        {
            player* pPlayer = static_cast<player*>( pObj );
            pPlayer->Teleport(
                NewPos,
                static_cast<zone_mgr::zone_id>( pObj->GetZone1() ),
                static_cast<zone_mgr::zone_id>( pObj->GetZone2() ),
                PlayerTeleportVelocityPolicy::Clear,
                FALSE,
                FALSE );
        }
        else
        {
            pObj->OnMove( NewPos );
        }
    }
    return 0;
}

//==============================================================================

static
s32 BindObjMoveRel( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
    {
        vector3 Delta( Context.ArgFloat(1), Context.ArgFloat(2), Context.ArgFloat(3) );
        pObj->OnMoveRel( Delta );
    }
    return 0;
}

//==============================================================================

static
s32 BindObjGetRotation( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil(); Context.PushNil(); Context.PushNil();
        return 3;
    }
    radian3 Rot = pObj->GetL2W().GetRotation();
    Context.PushFloat( RAD_TO_DEG( Rot.Pitch ) );
    Context.PushFloat( RAD_TO_DEG( Rot.Yaw   ) );
    Context.PushFloat( RAD_TO_DEG( Rot.Roll  ) );
    return 3;
}

//==============================================================================

static
s32 BindObjSetRotation( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
    {
        radian3 NewRot(
            DEG_TO_RAD( Context.ArgFloat(1) ),
            DEG_TO_RAD( Context.ArgFloat(2) ),
            DEG_TO_RAD( Context.ArgFloat(3) ) );

        const matrix4& OldL2W = pObj->GetL2W();
        matrix4 NewL2W;
        NewL2W.Setup( OldL2W.GetScale(), NewRot, OldL2W.GetTranslation() );
        pObj->OnTransform( NewL2W );
    }
    return 0;
}

//==============================================================================

static
s32 BindObjGetVelocity( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil(); Context.PushNil(); Context.PushNil();
        return 3;
    }
    PushVector3( Context, pObj->GetVelocity() );
    return 3;
}

//==============================================================================

static
s32 BindObjGetBBox( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil(); Context.PushNil(); Context.PushNil();
        Context.PushNil(); Context.PushNil(); Context.PushNil();
        return 6;
    }
    PushBBox( Context, pObj->GetBBox() );
    return 6;
}

//==============================================================================

static
s32 BindObjGetLocalBBox( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil(); Context.PushNil(); Context.PushNil();
        Context.PushNil(); Context.PushNil(); Context.PushNil();
        return 6;
    }
    PushBBox( Context, pObj->GetLocalBBox() );
    return 6;
}

//==============================================================================

static
s32 BindObjActivate( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
        pObj->OnActivate( Context.ArgBool( 1 ) );
    return 0;
}

//==============================================================================

static
s32 BindNavNearestPoint( ScriptContext& Context )
{
    vector3 P( Context.ArgFloat(0), Context.ArgFloat(1), Context.ArgFloat(2) );
    vector3 N = g_NavMap.GetNearestPointInNavMap( P );
    Context.PushFloat( N.GetX() );
    Context.PushFloat( N.GetY() );
    Context.PushFloat( N.GetZ() );
    return 3;
}

//==============================================================================

static
s32 BindObjLoadStart( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
        pObj->LoadStart();
    return 0;
}

//==============================================================================

static
s32 BindObjLoadEnd( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
        pObj->LoadEnd();
    return 0;
}

//==============================================================================

static
s32 BindObjIsAlive( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    Context.PushBool( pObj ? pObj->IsAlive() : FALSE );
    return 1;
}

//==============================================================================

static
s32 BindObjIsActive( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    Context.PushBool( pObj ? pObj->IsActive() : FALSE );
    return 1;
}

//==============================================================================

static
s32 BindObjGetHealth( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    Context.PushFloat( pObj ? pObj->GetHealth() : 0.0f );
    return 1;
}

//==============================================================================

static
s32 BindObjApplyPain( ScriptContext& Context )
{
    guid        VictimGuid = ArgGuid( Context, 0 );
    guid        OriginGuid = ArgGuid( Context, 1 );
    vector3     Pos( Context.ArgFloat(2), Context.ArgFloat(3), Context.ArgFloat(4) );
    const char* pPainDesc  = Context.ArgString( 5 );

    pain P;
    P.Setup( pPainDesc, OriginGuid, Pos );
    Context.PushBool( P.ApplyToObject( VictimGuid ) );
    return 1;
}

//==============================================================================

static
s32 BindObjGetAttributes( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    Context.PushInt( pObj ? (s32)pObj->GetAttrBits() : 0 );
    return 1;
}

//==============================================================================

static
s32 BindObjSetAttributes( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
        pObj->SetAttrBits( (u32)Context.ArgInt( 1 ) );
    return 0;
}

//==============================================================================

static
s32 BindObjHasAttribute( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    u32     Bit  = (u32)Context.ArgInt( 1 );
    Context.PushBool( pObj ? ( (pObj->GetAttrBits() & Bit) == Bit ) : FALSE );
    return 1;
}

//==============================================================================

static
s32 BindObjEnableAttr( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
        pObj->TurnAttrBitsOn( (u32)Context.ArgInt( 1 ) );
    return 0;
}

//==============================================================================

static
s32 BindObjDisableAttr( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
        pObj->TurnAttrBitsOff( (u32)Context.ArgInt( 1 ) );
    return 0;
}

//==============================================================================

//static
//s32 BindObjGetZones( ScriptContext& Context )
//{
//    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
//    Context.PushInt( pObj ? (s32)pObj->GetZone1() : 0 );
//    Context.PushInt( pObj ? (s32)pObj->GetZone2() : 0 );
//    return 2;
//}
//
////==============================================================================
//
//static
//s32 BindObjGetZone1( ScriptContext& Context )
//{
//    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
//    Context.PushInt( pObj ? (s32)pObj->GetZone1() : 0 );
//    return 1;
//}
//
////==============================================================================
//
//static
//s32 BindObjGetZone2( ScriptContext& Context )
//{
//    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
//    Context.PushInt( pObj ? (s32)pObj->GetZone2() : 0 );
//    return 1;
//}

//==============================================================================

static
s32 BindObjSetZones( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
    {
        pObj->SetZone1( (u16)Context.ArgInt( 1 ) );
        pObj->SetZone2( (u16)Context.ArgInt( 2 ) );

        if( pObj->IsKindOf( actor::GetRTTI() ) )
        {
            static_cast<actor*>( pObj )->InitZoneTracking();
        }
    }
    return 0;
}

//==============================================================================

static
s32 BindObjGetType( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil();
        return 1;
    }
    Context.PushString( pObj->GetTypeDesc().GetTypeName() );
    return 1;
}

//==============================================================================

static
s32 BindObjGetSlot( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    Context.PushInt( pObj ? pObj->GetSlot() : -1 );
    return 1;
}

//==============================================================================

static
s32 BindObjGetParentGuid( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushU64( 0 );
        return 1;
    }
    PushGuid( Context, pObj->GetParentGuid() );
    return 1;
}

//==============================================================================

static
s32 BindObjGetLogicalName( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil();
        return 1;
    }
    const char* pName = pObj->GetLogicalName();
    if( pName && pName[0] )
        Context.PushString( pName );
    else
        Context.PushNil();
    return 1;
}

//==============================================================================

static
s32 BindObjGetLookAtExtent( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    Context.PushFloat( pObj ? pObj->GetLookAtExtent() : 0.0f );
    return 1;
}

//==============================================================================

static
s32 BindObjCreate( ScriptContext& Context )
{
    guid G = g_ObjMgr.CreateObject( Context.ArgString(0) );
    PushGuid( Context, G );
    return 1;
}

//==============================================================================

static
s32 BindObjDestroy( ScriptContext& Context )
{
    g_ObjMgr.DestroyObject( ArgGuid( Context, 0 ) );
    return 0;
}

//==============================================================================

static
s32 BindObjGetFirst( ScriptContext& Context )
{
    object_desc* pDesc = FindDescByName( Context.ArgString( 0 ) );
    if( !pDesc )
    {
        Context.PushU64( 0 );
        return 1;
    }

    slot_id Slot = g_ObjMgr.GetFirst( pDesc->GetType() );
    if( Slot == SLOT_NULL )
    {
        Context.PushU64( 0 );
        return 1;
    }

    object* pObj = g_ObjMgr.GetObjectBySlot( Slot );
    PushGuid( Context, pObj ? pObj->GetGuid() : guid(0) );
    return 1;
}

//==============================================================================

static
s32 BindObjGetNext( ScriptContext& Context )
{
    guid    G    = ArgGuid( Context, 0 );
    object* pObj = g_ObjMgr.GetObjectByGuid( G );
    if( !pObj )
    {
        Context.PushU64( 0 );
        return 1;
    }

    slot_id Slot = g_ObjMgr.GetFirst( pObj->GetType() );
    while( Slot != SLOT_NULL )
    {
        object* pCur = g_ObjMgr.GetObjectBySlot( Slot );
        if( pCur && pCur->GetGuid() == G )
        {
            slot_id Next = g_ObjMgr.GetNext( Slot );
            if( Next == SLOT_NULL )
            {
                Context.PushU64( 0 );
            }
            else
            {
                object* pNext = g_ObjMgr.GetObjectBySlot( Next );
                PushGuid( Context, pNext ? pNext->GetGuid() : guid(0) );
            }
            return 1;
        }
        Slot = g_ObjMgr.GetNext( Slot );
    }

    Context.PushU64( 0 );
    return 1;
}

//==============================================================================

static
s32 BindObjGetCount( ScriptContext& Context )
{
    object_desc* pDesc = FindDescByName( Context.ArgString( 0 ) );
    if( !pDesc )
    {
        Context.PushInt( 0 );
        return 1;
    }
    Context.PushInt( g_ObjMgr.GetNumInstances( pDesc->GetType() ) );
    return 1;
}

//==============================================================================

static
s32 BindObjDumpTypes( ScriptContext& Context )
{
    (void)Context;
    x_DebugMsg( "[SCRIPT] obj_dump_types:\n" );
    for( object_desc* p = object_desc::GetFirstType(); p; p = object_desc::GetNextType(p) )
    {
        slot_id Slot = g_ObjMgr.GetFirst( p->GetType() );
        if( Slot != SLOT_NULL )
        {
            s32 Count = g_ObjMgr.GetNumInstances( p->GetType() );
            x_DebugMsg( "[SCRIPT]   %-32s  count=%d\n", p->GetTypeName(), Count );
        }
    }
    return 0;
}

//==============================================================================

static
s32 BindObjPropGet( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil();
        return 1;
    }

    const char* pPropName = Context.ArgString( 1 );
    prop_enum   Enum;
    prop_type   Type      = FindPropType( pObj, pPropName, Enum );
    prop_query  Q;

    switch( Type )
    {
    case PROP_TYPE_FLOAT:
    {
        f32 V = 0.0f;
        if( pObj->OnProperty( Q.RQueryFloat( pPropName, V ) ) )
            Context.PushFloat( V );
        else
            Context.PushNil();
        return 1;
    }
    case PROP_TYPE_INT:
    {
        s32 V = 0;
        if( pObj->OnProperty( Q.RQueryInt( pPropName, V ) ) )
            Context.PushInt( V );
        else
            Context.PushNil();
        return 1;
    }
    case PROP_TYPE_BOOL:
    {
        xbool V = FALSE;
        if( pObj->OnProperty( Q.RQueryBool( pPropName, V ) ) )
            Context.PushBool( V );
        else
            Context.PushNil();
        return 1;
    }
    // ANGLE: engine stores radians; script layer uses degrees per Property.hpp contract
    case PROP_TYPE_ANGLE:
    {
        radian V = 0.0f;
        if( pObj->OnProperty( Q.RQueryAngle( pPropName, V ) ) )
            Context.PushFloat( RAD_TO_DEG( V ) );
        else
            Context.PushNil();
        return 1;
    }
    case PROP_TYPE_VECTOR2:
    {
        vector2 V( 0.0f, 0.0f );
        if( pObj->OnProperty( Q.RQueryVector2( pPropName, V ) ) )
        {
            Context.PushFloat( V.X );
            Context.PushFloat( V.Y );
            return 2;
        }
        Context.PushNil();
        return 1;
    }
    case PROP_TYPE_VECTOR3:
    {
        vector3 V( 0.0f, 0.0f, 0.0f );
        if( pObj->OnProperty( Q.RQueryVector3( pPropName, V ) ) )
        {
            PushVector3( Context, V );
            return 3;
        }
        Context.PushNil();
        return 1;
    }
    // ROTATION: engine stores radian3; script layer uses degrees per Property.hpp contract
    case PROP_TYPE_ROTATION:
    {
        radian3 V( 0.0f, 0.0f, 0.0f );
        if( pObj->OnProperty( Q.RQueryRotation( pPropName, V ) ) )
        {
            Context.PushFloat( RAD_TO_DEG( V.Pitch ) );
            Context.PushFloat( RAD_TO_DEG( V.Yaw   ) );
            Context.PushFloat( RAD_TO_DEG( V.Roll  ) );
            return 3;
        }
        Context.PushNil();
        return 1;
    }
    case PROP_TYPE_BBOX:
    {
        bbox V;
        if( pObj->OnProperty( Q.RQueryBBox( pPropName, V ) ) )
        {
            PushBBox( Context, V );
            return 6;
        }
        Context.PushNil();
        return 1;
    }
    case PROP_TYPE_COLOR:
    {
        xcolor V( 255, 255, 255, 255 );
        if( pObj->OnProperty( Q.RQueryColor( pPropName, V ) ) )
        {
            Context.PushInt( (s32)V.R );
            Context.PushInt( (s32)V.G );
            Context.PushInt( (s32)V.B );
            Context.PushInt( (s32)V.A );
            return 4;
        }
        Context.PushNil();
        return 1;
    }
    case PROP_TYPE_GUID:
    {
        guid V = 0;
        if( pObj->OnProperty( Q.RQueryGUID( pPropName, V ) ) )
            PushGuid( Context, V );
        else
            Context.PushNil();
        return 1;
    }
    case PROP_TYPE_STRING:
    case PROP_TYPE_FILENAME:
    {
        char Buf[PROP_STRING_BUFSIZE] = { 0 };
        if( pObj->OnProperty( Q.RQueryString( pPropName, Buf ) ) )
            Context.PushString( Buf );
        else
            Context.PushNil();
        return 1;
    }
    case PROP_TYPE_ENUM:
    {
        char Buf[PROP_STRING_BUFSIZE] = { 0 };
        if( pObj->OnProperty( Q.RQueryEnum( pPropName, Buf ) ) )
            Context.PushString( Buf );
        else
            Context.PushNil();
        return 1;
    }
    case PROP_TYPE_EXTERNAL:
    {
        char Buf[PROP_STRING_BUFSIZE] = { 0 };
        if( pObj->OnProperty( Q.RQueryExternal( pPropName, Buf ) ) )
            Context.PushString( Buf );
        else
            Context.PushNil();
        return 1;
    }
    default:
        Context.PushNil();
        return 1;
    }
}

//==============================================================================

static
s32 BindObjPropSet( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
        return 0;

    const char* pPropName = Context.ArgString( 1 );
    prop_enum   Enum;
    prop_type   Type      = FindPropType( pObj, pPropName, Enum );
    prop_query  Q;

    switch( Type )
    {
    case PROP_TYPE_FLOAT:
    {
        f32 V = Context.ArgFloat( 2 );
        pObj->OnProperty( Q.WQueryFloat( pPropName, V ) );
        break;
    }
    case PROP_TYPE_INT:
    {
        s32 V = Context.ArgInt( 2 );
        pObj->OnProperty( Q.WQueryInt( pPropName, V ) );
        break;
    }
    case PROP_TYPE_BOOL:
    {
        xbool V = Context.ArgBool( 2 );
        pObj->OnProperty( Q.WQueryBool( pPropName, V ) );
        break;
    }
    // ANGLE: script passes degrees; engine expects radians
    case PROP_TYPE_ANGLE:
    {
        radian V = DEG_TO_RAD( Context.ArgFloat( 2 ) );
        pObj->OnProperty( Q.WQueryAngle( pPropName, V ) );
        break;
    }
    case PROP_TYPE_VECTOR2:
    {
        vector2 V( Context.ArgFloat(2), Context.ArgFloat(3) );
        pObj->OnProperty( Q.WQueryVector2( pPropName, V ) );
        break;
    }
    case PROP_TYPE_VECTOR3:
    {
        vector3 V( Context.ArgFloat(2), Context.ArgFloat(3), Context.ArgFloat(4) );
        pObj->OnProperty( Q.WQueryVector3( pPropName, V ) );
        break;
    }
    // ROTATION: script passes degrees (pitch, yaw, roll); engine expects radian3
    case PROP_TYPE_ROTATION:
    {
        radian3 V(
            DEG_TO_RAD( Context.ArgFloat(2) ),
            DEG_TO_RAD( Context.ArgFloat(3) ),
            DEG_TO_RAD( Context.ArgFloat(4) ) );
        pObj->OnProperty( Q.WQueryRotation( pPropName, V ) );
        break;
    }
    case PROP_TYPE_BBOX:
    {
        bbox V;
        V.Min.Set( Context.ArgFloat(2), Context.ArgFloat(3), Context.ArgFloat(4) );
        V.Max.Set( Context.ArgFloat(5), Context.ArgFloat(6), Context.ArgFloat(7) );
        pObj->OnProperty( Q.WQueryBBox( pPropName, V ) );
        break;
    }
    case PROP_TYPE_COLOR:
    {
        xcolor V( (u8)Context.ArgInt(2), (u8)Context.ArgInt(3),
                  (u8)Context.ArgInt(4), (u8)Context.ArgInt(5) );
        pObj->OnProperty( Q.WQueryColor( pPropName, V ) );
        break;
    }
    case PROP_TYPE_GUID:
    {
        guid V( Context.ArgU64( 2 ) );
        pObj->OnProperty( Q.WQueryGUID( pPropName, V ) );
        break;
    }
    case PROP_TYPE_STRING:
    case PROP_TYPE_FILENAME:
    {
        const char* pStr = Context.ArgString( 2 );
        pObj->OnProperty( Q.WQueryString( pPropName, pStr ) );
        break;
    }
    case PROP_TYPE_ENUM:
    {
        const char* pStr = Context.ArgString( 2 );
        pObj->OnProperty( Q.WQueryEnum( pPropName, pStr ) );
        break;
    }
    case PROP_TYPE_EXTERNAL:
    {
        const char* pStr = Context.ArgString( 2 );
        pObj->OnProperty( Q.WQueryExternal( pPropName, pStr ) );
        break;
    }
    default:
        x_DebugMsg( "***\n*** SCRIPT WARNING [obj_prop_set]\n"
                    "*** property '%s' not found or type not supported\n***\n",
                    pPropName );
        break;
    }
    return 0;
}

//==============================================================================

static
s32 BindObjPropList( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        x_DebugMsg( "[SCRIPT] obj_prop_list: invalid guid\n" );
        return 0;
    }

    prop_enum Enum;
    pObj->OnEnumProp( Enum );

    x_DebugMsg( "[SCRIPT] obj_prop_list for '%s':\n", pObj->GetTypeDesc().GetTypeName() );

    for( s32 i = 0; i < Enum.GetCount(); i++ )
    {
        prop_type   Type      = (prop_type)( Enum[i].GetType() & PROP_TYPE_BASIC_MASK );
        const char* pTypeName = "?";
        switch( Type )
        {
        case PROP_TYPE_NULL:     pTypeName = "header";         break;
        case PROP_TYPE_FLOAT:    pTypeName = "float";          break;
        case PROP_TYPE_INT:      pTypeName = "int";            break;
        case PROP_TYPE_BOOL:     pTypeName = "bool";           break;
        case PROP_TYPE_VECTOR2:  pTypeName = "vector2";        break;
        case PROP_TYPE_VECTOR3:  pTypeName = "vector3";        break;
        case PROP_TYPE_ROTATION: pTypeName = "rotation(deg)";  break;
        case PROP_TYPE_ANGLE:    pTypeName = "angle(deg)";     break;
        case PROP_TYPE_BBOX:     pTypeName = "bbox";           break;
        case PROP_TYPE_GUID:     pTypeName = "guid";           break;
        case PROP_TYPE_COLOR:    pTypeName = "color";          break;
        case PROP_TYPE_STRING:   pTypeName = "string";         break;
        case PROP_TYPE_ENUM:     pTypeName = "enum";           break;
        case PROP_TYPE_FILENAME: pTypeName = "filename";       break;
        case PROP_TYPE_EXTERNAL: pTypeName = "external";       break;
        case PROP_TYPE_BUTTON:   pTypeName = "button";         break;
        }
        x_DebugMsg( "[SCRIPT]   %-40s  %s\n", Enum[i].GetName(), pTypeName );
    }
    return 0;
}

//==============================================================================

#ifdef USE_OBJECT_NAMES

static
s32 BindObjGetName( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushNil();
        return 1;
    }
    Context.PushString( pObj->GetName() );
    return 1;
}

//==============================================================================

static
s32 BindObjSetName( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj )
        pObj->SetName( Context.ArgString( 1 ) );
    return 0;
}

//==============================================================================

static
s32 BindObjFindByName( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByName( Context.ArgString( 0 ) );
    PushGuid( Context, pObj ? pObj->GetGuid() : guid(0) );
    return 1;
}

#endif // USE_OBJECT_NAMES

//==============================================================================

static
s32 BindObjGetPlayer( ScriptContext& Context )
{
    PushGuid( Context, SMP_UTIL_GetActivePlayerGuid() );
    return 1;
}

//==============================================================================

static
s32 BindObjGetZone( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( !pObj )
    {
        Context.PushInt( 0 );
        Context.PushInt( 0 );
        return 2;
    }
    Context.PushInt( (s32)pObj->GetZone1() );
    Context.PushInt( (s32)pObj->GetZone2() );
    return 2;
}

//==============================================================================

static
s32 BindObjSetTarget( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj && pObj->IsKindOf( character::GetRTTI() ) )
    {
        character& Char = character::GetSafeType( *pObj );
        Char.SetTargetGuid( ArgGuid( Context, 1 ) );
        Char.SetAwarenessLevel( character::AWARENESS_TARGET_SPOTTED );
    }
    return 0;
}

//==============================================================================

static
s32 BindObjClearTarget( ScriptContext& Context )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( Context, 0 ) );
    if( pObj && pObj->IsKindOf( character::GetRTTI() ) )
    {
        character& Char = character::GetSafeType( *pObj );
        Char.SetOverrideTarget( 0 );
        Char.SetTargetGuid( 0 );
    }
    return 0;
}

//==============================================================================
//  REGISTER
//==============================================================================

void BindObjects::Register( ScriptBackend* pBackend )
{
    // ──── Transform ────
    pBackend->RegisterFunction( "obj_getpos",          BindObjGetPos          );
    pBackend->RegisterFunction( "obj_setpos",          BindObjSetPos          );
    pBackend->RegisterFunction( "obj_moverel",         BindObjMoveRel         );
    pBackend->RegisterFunction( "obj_getrotation",     BindObjGetRotation     );
    pBackend->RegisterFunction( "obj_setrotation",     BindObjSetRotation     );

    // ──── Velocity ────
    pBackend->RegisterFunction( "obj_getvelocity",     BindObjGetVelocity     );

    // ──── Bounding boxes ────
    pBackend->RegisterFunction( "obj_getbbox",         BindObjGetBBox         );
    pBackend->RegisterFunction( "obj_getlocalbbox",    BindObjGetLocalBBox    );

    // ──── Lifecycle / state ────
    pBackend->RegisterFunction( "obj_activate",        BindObjActivate        );
    pBackend->RegisterFunction( "obj_load_start",      BindObjLoadStart       );
    pBackend->RegisterFunction( "obj_load_end",        BindObjLoadEnd         );
    pBackend->RegisterFunction( "nav_nearest_point",   BindNavNearestPoint    );
    pBackend->RegisterFunction( "obj_isalive",         BindObjIsAlive         );
    pBackend->RegisterFunction( "obj_isactive",        BindObjIsActive        );
    pBackend->RegisterFunction( "obj_gethealth",       BindObjGetHealth       );
    pBackend->RegisterFunction( "obj_applypain",       BindObjApplyPain       );

    // ──── Attribute bits ────
    pBackend->RegisterFunction( "obj_getattributes",   BindObjGetAttributes   );
    pBackend->RegisterFunction( "obj_setattributes",   BindObjSetAttributes   );
    pBackend->RegisterFunction( "obj_hasattribute",    BindObjHasAttribute    );
    pBackend->RegisterFunction( "obj_enableattr",      BindObjEnableAttr      );
    pBackend->RegisterFunction( "obj_disableattr",     BindObjDisableAttr     );

    // ──── Zone membership ────
    pBackend->RegisterFunction( "obj_getzone",         BindObjGetZone         );
    pBackend->RegisterFunction( "obj_setzones",        BindObjSetZones        );

    // ──── Active player / AI targeting ────
    pBackend->RegisterFunction( "obj_get_player",      BindObjGetPlayer       );
    pBackend->RegisterFunction( "obj_set_target",      BindObjSetTarget       );
    pBackend->RegisterFunction( "obj_clear_target",    BindObjClearTarget     );

    // ──── Identity / info ────
    pBackend->RegisterFunction( "obj_gettype",         BindObjGetType         );
    pBackend->RegisterFunction( "obj_getslot",         BindObjGetSlot         );
    pBackend->RegisterFunction( "obj_getparentguid",   BindObjGetParentGuid   );
    pBackend->RegisterFunction( "obj_getlogicalname",  BindObjGetLogicalName  );
    pBackend->RegisterFunction( "obj_getlookatextent", BindObjGetLookAtExtent );

    // ──── Creation / destruction / iteration ────
    pBackend->RegisterFunction( "obj_create",          BindObjCreate          );
    pBackend->RegisterFunction( "obj_destroy",         BindObjDestroy         );
    pBackend->RegisterFunction( "obj_getfirst",        BindObjGetFirst        );
    pBackend->RegisterFunction( "obj_getnext",         BindObjGetNext         );
    pBackend->RegisterFunction( "obj_getcount",        BindObjGetCount        );
    pBackend->RegisterFunction( "obj_dump_types",      BindObjDumpTypes       );

    // ──── Universal property bridge ────
    pBackend->RegisterFunction( "obj_prop_get",        BindObjPropGet         );
    pBackend->RegisterFunction( "obj_prop_set",        BindObjPropSet         );
    pBackend->RegisterFunction( "obj_prop_list",       BindObjPropList        );

    // ──── Name lookup ────
#ifdef USE_OBJECT_NAMES
    pBackend->RegisterFunction( "obj_getname",         BindObjGetName         );
    pBackend->RegisterFunction( "obj_setname",         BindObjSetName         );
    pBackend->RegisterFunction( "obj_findbyname",      BindObjFindByName      );
#endif
}
