//==============================================================================
//
//  bind_objects.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "bind_objects.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/object.hpp"
#include "MiscUtils/Property.hpp"
#include "PainMgr/Pain.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

static const s32 PROP_STRING_BUFSIZE = 256;

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
inline void PushGuid( script_context& ctx, guid G )
{
    ctx.PushU64( (u64)G );
}

//==============================================================================

static
inline void PushVector3( script_context& ctx, const vector3& V )
{
    ctx.PushFloat( V.GetX() );
    ctx.PushFloat( V.GetY() );
    ctx.PushFloat( V.GetZ() );
}

//==============================================================================

static
inline void PushBBox( script_context& ctx, const bbox& B )
{
    ctx.PushFloat( B.Min.GetX() );
    ctx.PushFloat( B.Min.GetY() );
    ctx.PushFloat( B.Min.GetZ() );
    ctx.PushFloat( B.Max.GetX() );
    ctx.PushFloat( B.Max.GetY() );
    ctx.PushFloat( B.Max.GetZ() );
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
//  IMPLEMENTATION
//==============================================================================

static
s32 bind_obj_getpos( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil(); ctx.PushNil(); ctx.PushNil();
        return 3;
    }
    PushVector3( ctx, pObj->GetPosition() );
    return 3;
}

//==============================================================================

static
s32 bind_obj_setpos( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
    {
        vector3 NewPos( ctx.ArgFloat(1), ctx.ArgFloat(2), ctx.ArgFloat(3) );
        pObj->OnMove( NewPos );
    }
    return 0;
}

//==============================================================================

static
s32 bind_obj_moverel( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
    {
        vector3 Delta( ctx.ArgFloat(1), ctx.ArgFloat(2), ctx.ArgFloat(3) );
        pObj->OnMoveRel( Delta );
    }
    return 0;
}

//==============================================================================

static
s32 bind_obj_getrotation( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil(); ctx.PushNil(); ctx.PushNil();
        return 3;
    }
    radian3 Rot = pObj->GetL2W().GetRotation();
    ctx.PushFloat( RAD_TO_DEG( Rot.Pitch ) );
    ctx.PushFloat( RAD_TO_DEG( Rot.Yaw   ) );
    ctx.PushFloat( RAD_TO_DEG( Rot.Roll  ) );
    return 3;
}

//==============================================================================

static
s32 bind_obj_setrotation( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
    {
        radian3 NewRot(
            DEG_TO_RAD( ctx.ArgFloat(1) ),
            DEG_TO_RAD( ctx.ArgFloat(2) ),
            DEG_TO_RAD( ctx.ArgFloat(3) ) );

        const matrix4& OldL2W = pObj->GetL2W();
        matrix4 NewL2W;
        NewL2W.Setup( OldL2W.GetScale(), NewRot, OldL2W.GetTranslation() );
        pObj->OnTransform( NewL2W );
    }
    return 0;
}

//==============================================================================

static
s32 bind_obj_getvelocity( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil(); ctx.PushNil(); ctx.PushNil();
        return 3;
    }
    PushVector3( ctx, pObj->GetVelocity() );
    return 3;
}

//==============================================================================

static
s32 bind_obj_getbbox( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil(); ctx.PushNil(); ctx.PushNil();
        ctx.PushNil(); ctx.PushNil(); ctx.PushNil();
        return 6;
    }
    PushBBox( ctx, pObj->GetBBox() );
    return 6;
}

//==============================================================================

static
s32 bind_obj_getlocalbbox( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil(); ctx.PushNil(); ctx.PushNil();
        ctx.PushNil(); ctx.PushNil(); ctx.PushNil();
        return 6;
    }
    PushBBox( ctx, pObj->GetLocalBBox() );
    return 6;
}

//==============================================================================

static
s32 bind_obj_activate( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
        pObj->OnActivate( ctx.ArgBool( 1 ) );
    return 0;
}

//==============================================================================

static
s32 bind_obj_isalive( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    ctx.PushBool( pObj ? pObj->IsAlive() : FALSE );
    return 1;
}

//==============================================================================

static
s32 bind_obj_isactive( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    ctx.PushBool( pObj ? pObj->IsActive() : FALSE );
    return 1;
}

//==============================================================================

static
s32 bind_obj_gethealth( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    ctx.PushFloat( pObj ? pObj->GetHealth() : 0.0f );
    return 1;
}

//==============================================================================

static
s32 bind_obj_applypain( script_context& ctx )
{
    guid        VictimGuid = ArgGuid( ctx, 0 );
    guid        OriginGuid = ArgGuid( ctx, 1 );
    vector3     Pos( ctx.ArgFloat(2), ctx.ArgFloat(3), ctx.ArgFloat(4) );
    const char* pPainDesc  = ctx.ArgString( 5 );

    pain P;
    P.Setup( pPainDesc, OriginGuid, Pos );
    ctx.PushBool( P.ApplyToObject( VictimGuid ) );
    return 1;
}

//==============================================================================

static
s32 bind_obj_getattributes( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    ctx.PushInt( pObj ? (s32)pObj->GetAttrBits() : 0 );
    return 1;
}

//==============================================================================

static
s32 bind_obj_setattributes( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
        pObj->SetAttrBits( (u32)ctx.ArgInt( 1 ) );
    return 0;
}

//==============================================================================

static
s32 bind_obj_hasattribute( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    u32     Bit  = (u32)ctx.ArgInt( 1 );
    ctx.PushBool( pObj ? ( (pObj->GetAttrBits() & Bit) == Bit ) : FALSE );
    return 1;
}

//==============================================================================

static
s32 bind_obj_enableattr( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
        pObj->TurnAttrBitsOn( (u32)ctx.ArgInt( 1 ) );
    return 0;
}

//==============================================================================

static
s32 bind_obj_disableattr( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
        pObj->TurnAttrBitsOff( (u32)ctx.ArgInt( 1 ) );
    return 0;
}

//==============================================================================

//static
//s32 bind_obj_getzones( script_context& ctx )
//{
//    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
//    ctx.PushInt( pObj ? (s32)pObj->GetZone1() : 0 );
//    ctx.PushInt( pObj ? (s32)pObj->GetZone2() : 0 );
//    return 2;
//}
//
////==============================================================================
//
//static
//s32 bind_obj_getzone1( script_context& ctx )
//{
//    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
//    ctx.PushInt( pObj ? (s32)pObj->GetZone1() : 0 );
//    return 1;
//}
//
////==============================================================================
//
//static
//s32 bind_obj_getzone2( script_context& ctx )
//{
//    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
//    ctx.PushInt( pObj ? (s32)pObj->GetZone2() : 0 );
//    return 1;
//}

//==============================================================================

static
s32 bind_obj_setzones( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
    {
        pObj->SetZone1( (u16)ctx.ArgInt( 1 ) );
        pObj->SetZone2( (u16)ctx.ArgInt( 2 ) );
    }
    return 0;
}

//==============================================================================

static
s32 bind_obj_gettype( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil();
        return 1;
    }
    ctx.PushString( pObj->GetTypeDesc().GetTypeName() );
    return 1;
}

//==============================================================================

static
s32 bind_obj_getslot( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    ctx.PushInt( pObj ? pObj->GetSlot() : -1 );
    return 1;
}

//==============================================================================

static
s32 bind_obj_getparentguid( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushU64( 0 );
        return 1;
    }
    PushGuid( ctx, pObj->GetParentGuid() );
    return 1;
}

//==============================================================================

static
s32 bind_obj_getlogicalname( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil();
        return 1;
    }
    const char* pName = pObj->GetLogicalName();
    if( pName && pName[0] )
        ctx.PushString( pName );
    else
        ctx.PushNil();
    return 1;
}

//==============================================================================

static
s32 bind_obj_getlookatextent( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    ctx.PushFloat( pObj ? pObj->GetLookAtExtent() : 0.0f );
    return 1;
}

//==============================================================================

static
s32 bind_obj_create( script_context& ctx )
{
    guid G = g_ObjMgr.CreateObject( ctx.ArgString(0) );
    PushGuid( ctx, G );
    return 1;
}

//==============================================================================

static
s32 bind_obj_destroy( script_context& ctx )
{
    g_ObjMgr.DestroyObject( ArgGuid( ctx, 0 ) );
    return 0;
}

//==============================================================================

static
s32 bind_obj_getfirst( script_context& ctx )
{
    object_desc* pDesc = FindDescByName( ctx.ArgString( 0 ) );
    if( !pDesc )
    {
        ctx.PushU64( 0 );
        return 1;
    }

    slot_id Slot = g_ObjMgr.GetFirst( pDesc->GetType() );
    if( Slot == SLOT_NULL )
    {
        ctx.PushU64( 0 );
        return 1;
    }

    object* pObj = g_ObjMgr.GetObjectBySlot( Slot );
    PushGuid( ctx, pObj ? pObj->GetGuid() : guid(0) );
    return 1;
}

//==============================================================================

static
s32 bind_obj_getnext( script_context& ctx )
{
    guid    G    = ArgGuid( ctx, 0 );
    object* pObj = g_ObjMgr.GetObjectByGuid( G );
    if( !pObj )
    {
        ctx.PushU64( 0 );
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
                ctx.PushU64( 0 );
            }
            else
            {
                object* pNext = g_ObjMgr.GetObjectBySlot( Next );
                PushGuid( ctx, pNext ? pNext->GetGuid() : guid(0) );
            }
            return 1;
        }
        Slot = g_ObjMgr.GetNext( Slot );
    }

    ctx.PushU64( 0 );
    return 1;
}

//==============================================================================

static
s32 bind_obj_getcount( script_context& ctx )
{
    object_desc* pDesc = FindDescByName( ctx.ArgString( 0 ) );
    if( !pDesc )
    {
        ctx.PushInt( 0 );
        return 1;
    }
    ctx.PushInt( g_ObjMgr.GetNumInstances( pDesc->GetType() ) );
    return 1;
}

//==============================================================================

static
s32 bind_obj_dump_types( script_context& ctx )
{
    (void)ctx;
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
s32 bind_obj_prop_get( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil();
        return 1;
    }

    const char* pPropName = ctx.ArgString( 1 );
    prop_enum   Enum;
    prop_type   Type      = FindPropType( pObj, pPropName, Enum );
    prop_query  Q;

    switch( Type )
    {
    case PROP_TYPE_FLOAT:
    {
        f32 V = 0.0f;
        if( pObj->OnProperty( Q.RQueryFloat( pPropName, V ) ) )
            ctx.PushFloat( V );
        else
            ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_INT:
    {
        s32 V = 0;
        if( pObj->OnProperty( Q.RQueryInt( pPropName, V ) ) )
            ctx.PushInt( V );
        else
            ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_BOOL:
    {
        xbool V = FALSE;
        if( pObj->OnProperty( Q.RQueryBool( pPropName, V ) ) )
            ctx.PushBool( V );
        else
            ctx.PushNil();
        return 1;
    }
    // ANGLE: engine stores radians; script layer uses degrees per Property.hpp contract
    case PROP_TYPE_ANGLE:
    {
        radian V = 0.0f;
        if( pObj->OnProperty( Q.RQueryAngle( pPropName, V ) ) )
            ctx.PushFloat( RAD_TO_DEG( V ) );
        else
            ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_VECTOR2:
    {
        vector2 V( 0.0f, 0.0f );
        if( pObj->OnProperty( Q.RQueryVector2( pPropName, V ) ) )
        {
            ctx.PushFloat( V.X );
            ctx.PushFloat( V.Y );
            return 2;
        }
        ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_VECTOR3:
    {
        vector3 V( 0.0f, 0.0f, 0.0f );
        if( pObj->OnProperty( Q.RQueryVector3( pPropName, V ) ) )
        {
            PushVector3( ctx, V );
            return 3;
        }
        ctx.PushNil();
        return 1;
    }
    // ROTATION: engine stores radian3; script layer uses degrees per Property.hpp contract
    case PROP_TYPE_ROTATION:
    {
        radian3 V( 0.0f, 0.0f, 0.0f );
        if( pObj->OnProperty( Q.RQueryRotation( pPropName, V ) ) )
        {
            ctx.PushFloat( RAD_TO_DEG( V.Pitch ) );
            ctx.PushFloat( RAD_TO_DEG( V.Yaw   ) );
            ctx.PushFloat( RAD_TO_DEG( V.Roll  ) );
            return 3;
        }
        ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_BBOX:
    {
        bbox V;
        if( pObj->OnProperty( Q.RQueryBBox( pPropName, V ) ) )
        {
            PushBBox( ctx, V );
            return 6;
        }
        ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_COLOR:
    {
        xcolor V( 255, 255, 255, 255 );
        if( pObj->OnProperty( Q.RQueryColor( pPropName, V ) ) )
        {
            ctx.PushInt( (s32)V.R );
            ctx.PushInt( (s32)V.G );
            ctx.PushInt( (s32)V.B );
            ctx.PushInt( (s32)V.A );
            return 4;
        }
        ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_GUID:
    {
        guid V = 0;
        if( pObj->OnProperty( Q.RQueryGUID( pPropName, V ) ) )
            PushGuid( ctx, V );
        else
            ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_STRING:
    case PROP_TYPE_FILENAME:
    {
        char Buf[PROP_STRING_BUFSIZE] = { 0 };
        if( pObj->OnProperty( Q.RQueryString( pPropName, Buf ) ) )
            ctx.PushString( Buf );
        else
            ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_ENUM:
    {
        char Buf[PROP_STRING_BUFSIZE] = { 0 };
        if( pObj->OnProperty( Q.RQueryEnum( pPropName, Buf ) ) )
            ctx.PushString( Buf );
        else
            ctx.PushNil();
        return 1;
    }
    case PROP_TYPE_EXTERNAL:
    {
        char Buf[PROP_STRING_BUFSIZE] = { 0 };
        if( pObj->OnProperty( Q.RQueryExternal( pPropName, Buf ) ) )
            ctx.PushString( Buf );
        else
            ctx.PushNil();
        return 1;
    }
    default:
        ctx.PushNil();
        return 1;
    }
}

//==============================================================================

static
s32 bind_obj_prop_set( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
        return 0;

    const char* pPropName = ctx.ArgString( 1 );
    prop_enum   Enum;
    prop_type   Type      = FindPropType( pObj, pPropName, Enum );
    prop_query  Q;

    switch( Type )
    {
    case PROP_TYPE_FLOAT:
    {
        f32 V = ctx.ArgFloat( 2 );
        pObj->OnProperty( Q.WQueryFloat( pPropName, V ) );
        break;
    }
    case PROP_TYPE_INT:
    {
        s32 V = ctx.ArgInt( 2 );
        pObj->OnProperty( Q.WQueryInt( pPropName, V ) );
        break;
    }
    case PROP_TYPE_BOOL:
    {
        xbool V = ctx.ArgBool( 2 );
        pObj->OnProperty( Q.WQueryBool( pPropName, V ) );
        break;
    }
    // ANGLE: script passes degrees; engine expects radians
    case PROP_TYPE_ANGLE:
    {
        radian V = DEG_TO_RAD( ctx.ArgFloat( 2 ) );
        pObj->OnProperty( Q.WQueryAngle( pPropName, V ) );
        break;
    }
    case PROP_TYPE_VECTOR2:
    {
        vector2 V( ctx.ArgFloat(2), ctx.ArgFloat(3) );
        pObj->OnProperty( Q.WQueryVector2( pPropName, V ) );
        break;
    }
    case PROP_TYPE_VECTOR3:
    {
        vector3 V( ctx.ArgFloat(2), ctx.ArgFloat(3), ctx.ArgFloat(4) );
        pObj->OnProperty( Q.WQueryVector3( pPropName, V ) );
        break;
    }
    // ROTATION: script passes degrees (pitch, yaw, roll); engine expects radian3
    case PROP_TYPE_ROTATION:
    {
        radian3 V(
            DEG_TO_RAD( ctx.ArgFloat(2) ),
            DEG_TO_RAD( ctx.ArgFloat(3) ),
            DEG_TO_RAD( ctx.ArgFloat(4) ) );
        pObj->OnProperty( Q.WQueryRotation( pPropName, V ) );
        break;
    }
    case PROP_TYPE_BBOX:
    {
        bbox V;
        V.Min.Set( ctx.ArgFloat(2), ctx.ArgFloat(3), ctx.ArgFloat(4) );
        V.Max.Set( ctx.ArgFloat(5), ctx.ArgFloat(6), ctx.ArgFloat(7) );
        pObj->OnProperty( Q.WQueryBBox( pPropName, V ) );
        break;
    }
    case PROP_TYPE_COLOR:
    {
        xcolor V( (u8)ctx.ArgInt(2), (u8)ctx.ArgInt(3),
                  (u8)ctx.ArgInt(4), (u8)ctx.ArgInt(5) );
        pObj->OnProperty( Q.WQueryColor( pPropName, V ) );
        break;
    }
    case PROP_TYPE_GUID:
    {
        guid V( ctx.ArgU64( 2 ) );
        pObj->OnProperty( Q.WQueryGUID( pPropName, V ) );
        break;
    }
    case PROP_TYPE_STRING:
    case PROP_TYPE_FILENAME:
    {
        const char* pStr = ctx.ArgString( 2 );
        pObj->OnProperty( Q.WQueryString( pPropName, pStr ) );
        break;
    }
    case PROP_TYPE_ENUM:
    {
        const char* pStr = ctx.ArgString( 2 );
        pObj->OnProperty( Q.WQueryEnum( pPropName, pStr ) );
        break;
    }
    case PROP_TYPE_EXTERNAL:
    {
        const char* pStr = ctx.ArgString( 2 );
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
s32 bind_obj_prop_list( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
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
s32 bind_obj_getname( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( !pObj )
    {
        ctx.PushNil();
        return 1;
    }
    ctx.PushString( pObj->GetName() );
    return 1;
}

//==============================================================================

static
s32 bind_obj_setname( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByGuid( ArgGuid( ctx, 0 ) );
    if( pObj )
        pObj->SetName( ctx.ArgString( 1 ) );
    return 0;
}

//==============================================================================

static
s32 bind_obj_findbyname( script_context& ctx )
{
    object* pObj = g_ObjMgr.GetObjectByName( ctx.ArgString( 0 ) );
    PushGuid( ctx, pObj ? pObj->GetGuid() : guid(0) );
    return 1;
}

#endif // USE_OBJECT_NAMES

//==============================================================================
//  REGISTER
//==============================================================================

void bind_objects::Register( script_backend* pBackend )
{
    // ──── Transform ────
    pBackend->RegisterFunction( "obj_getpos",           bind_obj_getpos           );
    pBackend->RegisterFunction( "obj_setpos",           bind_obj_setpos           );
    pBackend->RegisterFunction( "obj_moverel",          bind_obj_moverel          );
    pBackend->RegisterFunction( "obj_getrotation",      bind_obj_getrotation      );
    pBackend->RegisterFunction( "obj_setrotation",      bind_obj_setrotation      );

    // ──── Velocity ────
    pBackend->RegisterFunction( "obj_getvelocity",      bind_obj_getvelocity      );

    // ──── Bounding boxes ────
    pBackend->RegisterFunction( "obj_getbbox",          bind_obj_getbbox          );
    pBackend->RegisterFunction( "obj_getlocalbbox",     bind_obj_getlocalbbox     );

    // ──── Lifecycle / state ────
    pBackend->RegisterFunction( "obj_activate",         bind_obj_activate         );
    pBackend->RegisterFunction( "obj_isalive",          bind_obj_isalive          );
    pBackend->RegisterFunction( "obj_isactive",         bind_obj_isactive         );
    pBackend->RegisterFunction( "obj_gethealth",        bind_obj_gethealth        );
    pBackend->RegisterFunction( "obj_applypain",        bind_obj_applypain        );

    // ──── Attribute bits ────
    pBackend->RegisterFunction( "obj_getattributes",    bind_obj_getattributes    );
    pBackend->RegisterFunction( "obj_setattributes",    bind_obj_setattributes    );
    pBackend->RegisterFunction( "obj_hasattribute",     bind_obj_hasattribute     );
    pBackend->RegisterFunction( "obj_enableattr",       bind_obj_enableattr       );
    pBackend->RegisterFunction( "obj_disableattr",      bind_obj_disableattr      );

    // ──── Zone membership ────
    //pBackend->RegisterFunction( "obj_getzones",         bind_obj_getzones         );
    //pBackend->RegisterFunction( "obj_getzone1",         bind_obj_getzone1         );
    //pBackend->RegisterFunction( "obj_getzone2",         bind_obj_getzone2         );
    pBackend->RegisterFunction( "obj_setzones",         bind_obj_setzones         );

    // ──── Identity / info ────
    pBackend->RegisterFunction( "obj_gettype",          bind_obj_gettype          );
    pBackend->RegisterFunction( "obj_getslot",          bind_obj_getslot          );
    pBackend->RegisterFunction( "obj_getparentguid",    bind_obj_getparentguid    );
    pBackend->RegisterFunction( "obj_getlogicalname",   bind_obj_getlogicalname   );
    pBackend->RegisterFunction( "obj_getlookatextent",  bind_obj_getlookatextent  );

    // ──── Creation / destruction / iteration ────
    pBackend->RegisterFunction( "obj_create",           bind_obj_create           );
    pBackend->RegisterFunction( "obj_destroy",          bind_obj_destroy          );
    pBackend->RegisterFunction( "obj_getfirst",         bind_obj_getfirst         );
    pBackend->RegisterFunction( "obj_getnext",          bind_obj_getnext          );
    pBackend->RegisterFunction( "obj_getcount",         bind_obj_getcount         );
    pBackend->RegisterFunction( "obj_dump_types",       bind_obj_dump_types       );

    // ──── Universal property bridge ────
    pBackend->RegisterFunction( "obj_prop_get",         bind_obj_prop_get         );
    pBackend->RegisterFunction( "obj_prop_set",         bind_obj_prop_set         );
    pBackend->RegisterFunction( "obj_prop_list",        bind_obj_prop_list        );

    // ──── Name lookup ────
#ifdef USE_OBJECT_NAMES
    pBackend->RegisterFunction( "obj_getname",          bind_obj_getname          );
    pBackend->RegisterFunction( "obj_setname",          bind_obj_setname          );
    pBackend->RegisterFunction( "obj_findbyname",       bind_obj_findbyname       );
#endif
}