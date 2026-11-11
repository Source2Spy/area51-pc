//==============================================================================
//
//  NetUpdate.cpp
//
//==============================================================================

#include "NetUpdate.hpp"

#ifndef X_EDITOR

#include "Objects/Player/Player.hpp"
#include "Objects/Flag.hpp"
#include "Objects/FlagBase.hpp"
#include "Objects/CapPoint.hpp"
#include "NetworkMgr/NetworkMgr.hpp"

//==============================================================================

std::unique_ptr<net_update> net_update::Decode( netobj::type Type, s32 Slot, const bitstream& BS )
{
    BS.RequireFiniteFloats();

    std::unique_ptr<net_update> Result;

    switch( Type )
    {
        case netobj::TYPE_PLAYER:
        {
            net_actor_update* pUpdate = new net_actor_update;
            pUpdate->Data.Slot        = Slot;
            Result.reset( pUpdate );
        }

        break;

        case netobj::TYPE_PICKUP:
            Result.reset( new net_pickup_update );
            break;

        case netobj::TYPE_FLAG:
            Result.reset( new net_flag_update );
            break;

        case netobj::TYPE_FLAG_BASE:
            Result.reset( new net_flag_base_update );
            break;

        case netobj::TYPE_CAP_POINT:
            Result.reset( new net_cap_point_update );
            break;

        case netobj::TYPE_MESON_EXPL:
            Result.reset( new net_meson_update );
            break;

        case netobj::TYPE_GRENADE:
        case netobj::TYPE_JBEAN_GRENADE:
        case netobj::TYPE_MESON_1ST:
        case netobj::TYPE_MESON_2ND:
        case netobj::TYPE_BBG_1ST:
        case netobj::TYPE_PARASITE:
        case netobj::TYPE_CONTAGION:
        case netobj::TYPE_TENDRIL:
            Result.reset( new net_projectile_update( Type ) );
            break;

        default:
            BS.SetReadError();
            return Result;
    }

    Result->Read( BS );

    if( BS.ReadError() )
        Result.reset();

    return Result;
}

//==============================================================================

void net_actor_update::Read( const bitstream& BS )
{
    Data.Read( BS );
}

//==============================================================================

void net_actor_update::Apply( netobj& Object ) const
{
    static_cast<actor&>( Object ).net_AcceptUpdate( Data );
}

//==============================================================================

void net_projectile_update::Read( const bitstream& BS )
{
    Activate = BS.ReadFlag();

    if( Activate )
    {
        BS.ReadRangedS32( OriginNetSlot, -1, 31 );

        if( Type == netobj::TYPE_PARASITE )
        {
            BS.ReadFlag( Contagious );
            BS.ReadRangedF32( WanderPhase, 8, 0.0f, 2 * PI );
            BS.ReadRangedF32( WanderOffset, 8, 0.7f, 1.4f );
        }
    }

    AtRest      = BS.ReadFlag();
    HasPosition = FALSE;
    HasStart    = FALSE;
    HasTime     = FALSE;
    HasAttach   = FALSE;

    if( AtRest )
    {
        HasPosition = BS.ReadFlag();

        if( HasPosition )
        {
            BS.ReadVector( Position );
            BS.ReadRadian3( Orientation );
            BS.ReadRangedS32( PositionZone1, 0, 255 );
            BS.ReadRangedS32( PositionZone2, 0, 255 );
        }
    }
    else
    {
        HasStart = BS.ReadFlag();

        if( HasStart )
        {
            BS.ReadVector( StartPosition );
            BS.ReadVector( StartVelocity );
            BS.ReadRangedS32( StartZone1, 0, 255 );
            BS.ReadRangedS32( StartZone2, 0, 255 );

            if( Type == netobj::TYPE_JBEAN_GRENADE )
                BS.ReadRangedS32( ImpactCount, 0, 7 );

            if( Type == netobj::TYPE_PARASITE )
                BS.ReadRangedS32( TargetNetSlot, -1, 31 );

            ClearBlend = BS.ReadFlag();
        }

        HasTime = BS.ReadFlag();

        if( HasTime )
        {
            BS.ReadF32( Time );

            if( Time < 0.0f )
                BS.SetReadError();
        }

        HasAttach = BS.ReadFlag();

        if( HasAttach )
        {
            Attached = BS.ReadFlag();

            if( Attached )
            {
                BS.ReadS32( AttachedNetSlot );
                BS.ReadS32( AttachedBoneIndex );
                AttachedLocalPosition.Zero();

                if( AttachedBoneIndex != -1 )
                    BS.ReadVector( AttachedLocalPosition );

                if( ( AttachedNetSlot < 0 ) || ( AttachedNetSlot >= NET_MAX_OBJECTS ) ||
                    ( AttachedBoneIndex < -1 ) || ( AttachedBoneIndex > 255 ) )
                    BS.SetReadError();

                if( !BS.ReadError() && ( AttachedBoneIndex != -1 ) )
                {
                    netobj* pTarget = NetObjMgr.GetObjFromSlot( AttachedNetSlot );

                    if( pTarget && pTarget->IsKindOf( actor::GetRTTI() ) &&
                        !pTarget->IsKindOf( player::GetRTTI() ) )
                    {
                        loco* pLoco = ( (actor*)pTarget )->GetLocoPointer();

                        if( !pLoco || ( AttachedBoneIndex >= pLoco->GetNBones() ) )
                            BS.SetReadError();
                    }
                }
            }
        }
    }

    Exploded = BS.ReadFlag();

    if( Exploded )
        BS.ReadVector( ExplosionPosition );

    BS.ReadMarker();
}

//==============================================================================

void net_projectile_update::Apply( netobj& Object ) const
{
    static_cast<net_proj&>( Object ).net_AcceptUpdate( *this );
}

//==============================================================================

void net_pickup_update::Read( const bitstream& BS )
{
    HasItem = BS.ReadFlag();

    if( HasItem )
    {
        BS.ReadRangedS32( Item, INVEN_NULL, INVEN_GLOVES );
        BS.ReadVariableLenS32( Amount );
        BS.ReadFlag( Spins );

        if( Amount < 0 )
            BS.SetReadError();
    }

    HasState = BS.ReadFlag();

    if( HasState )
        BS.ReadRangedS32( State, pickup::STATE_FIRST, pickup::STATE_LAST );

    HasRequest = BS.ReadFlag();

    if( HasRequest )
        BS.ReadRangedS32( RequestPlayer, 0, 31 );

    HasAssign = BS.ReadFlag();

    if( HasAssign )
        BS.ReadRangedS32( AssignPlayer, 0, 31 );

    if( ( g_NetworkMgr.IsServer() && ( HasItem || HasState || HasAssign ) ) ||
        ( g_NetworkMgr.IsClient() && HasRequest ) )
        BS.SetReadError();

    HasProjectile = BS.ReadFlag();

    if( HasProjectile )
        net_projectile_update::Read( BS );
}

//==============================================================================

void net_pickup_update::Apply( netobj& Object ) const
{
    static_cast<pickup&>( Object ).net_AcceptUpdate( *this );
}

//==============================================================================

void net_flag_update::Read( const bitstream& BS )
{
    Activate = BS.ReadFlag();

    if( Activate )
    {
        BS.ReadU32( TeamBits );

        if( ( TeamBits != 1 ) && ( TeamBits != 2 ) )
            BS.SetReadError();
    }

    TimeoutImminent = BS.ReadFlag();
    Capture         = BS.ReadFlag();
    Teleport        = BS.ReadFlag();
    HasPosition     = BS.ReadFlag();

    if( HasPosition )
    {
        Attached = BS.ReadFlag();

        if( Attached )
        {
            BS.ReadS32( Player );

            if( ( Player < 0 ) || ( Player >= NET_MAX_PLAYERS ) )
                BS.SetReadError();
        }
        else
        {
            BS.ReadVector( Position );
            BS.ReadF32( Yaw );
            BS.ReadS32( Zone1 );
            BS.ReadS32( Zone2 );
            BS.ReadFlag( Falling );

            if( ( Zone1 < 0 ) || ( Zone1 > 255 ) || ( Zone2 < 0 ) || ( Zone2 > 255 ) )
                BS.SetReadError();
        }
    }

    HasTimeout = BS.ReadFlag();

    if( HasTimeout )
    {
        Timeout = BS.ReadFlag();

        if( Timeout )
        {
            BS.ReadF32( TimeoutAge );
            BS.ReadF32( CurrentAge );

            if( ( TimeoutAge < 0.0f ) || ( CurrentAge < 0.0f ) )
                BS.SetReadError();
        }
    }
}

//==============================================================================

void net_flag_update::Apply( netobj& Object ) const
{
    static_cast<flag&>( Object ).net_AcceptUpdate( *this );
}

//==============================================================================

void net_flag_base_update::Read( const bitstream& BS )
{
    Activate = BS.ReadFlag();

    if( Activate )
    {
        Orientation.Zero();
        BS.ReadVector( Position );
        BS.ReadF32( Orientation.Yaw );
        BS.ReadRangedS32( Zone1, 0, 255 );
        BS.ReadRangedS32( Zone2, 0, 255 );
        BS.ReadRangedS32( Circuit, 0, 3 );
    }
}

//==============================================================================

void net_flag_base_update::Apply( netobj& Object ) const
{
    static_cast<flag_base&>( Object ).net_AcceptUpdate( *this );
}

//==============================================================================

void net_cap_point_update::Read( const bitstream& BS )
{
    Activate = BS.ReadFlag();

    if( Activate )
    {
        BS.ReadVector( Position );
        BS.ReadF32( Elevation );
        BS.ReadF32( Radius );
        BS.ReadRadian3( Orientation );
        BS.ReadRangedS32( Zone1, 0, 255 );
        BS.ReadRangedS32( Zone2, 0, 255 );
        BS.ReadRangedS32( Circuit, 0, 15 );

        if( Radius <= 0.0f )
            BS.SetReadError();
    }

    HasMode = BS.ReadFlag();

    if( HasMode )
    {
        BS.ReadRangedS32( Mode, -1, 1 );
        BS.ReadRangedF32( Value, 8, -1.0f, 1.0f );
    }
}

//==============================================================================

void net_cap_point_update::Apply( netobj& Object ) const
{
    static_cast<cap_point&>( Object ).net_AcceptUpdate( *this );
}

//==============================================================================

void net_meson_update::Read( const bitstream& BS )
{
    Activate = BS.ReadFlag();

    if( Activate )
    {
        BS.ReadRangedS32( Owner, -1, 31 );
        BS.ReadVector( Origin );
        BS.ReadVector( Normal );
        BS.ReadU16( Zones );

        if( !IN_RANGE( 0.99f, Normal.LengthSquared(), 1.01f ) )
            BS.SetReadError();
    }

    HasTargets = BS.ReadFlag();

    if( HasTargets )
    {
        BS.ReadRangedS32( TargetCount, 0, MAX_LASH_TARGETS );

        for( s32 i = 0; ( i < TargetCount ) && !BS.ReadError(); i++ )
        {
            target& Target = Targets[i];
            BS.ReadS32( Target.Slot );
            BS.ReadRangedS32( Target.Type, 0, debris_meson_explosion::TARGET_TYPE_MAX );
            BS.ReadRangedS32( Target.Bone, S8_MIN, S8_MAX );
            BS.ReadFlag( Target.Valid );
            BS.ReadRangedS32( Target.LifeSeq, 0, 7 );

            if( ( Target.Slot < -1 ) || ( Target.Slot >= NET_MAX_OBJECTS ) ||
                ( Target.Type >= debris_meson_explosion::TARGET_TYPE_MAX ) )
                BS.SetReadError();
        }
    }

    HasNextTarget = BS.ReadFlag();

    if( HasNextTarget )
        BS.ReadRangedS32( NextTarget, 0, MAX_LASH_TARGETS );

    Collapse  = BS.ReadFlag();
    HasStatus = BS.ReadFlag();

    if( HasStatus )
        BS.ReadU16( KilledMask, 8 );

    BS.ReadMarker();
}

//==============================================================================

void net_meson_update::Apply( netobj& Object ) const
{
    static_cast<debris_meson_explosion&>( Object ).net_AcceptUpdate( *this );
}

#endif // X_EDITOR
