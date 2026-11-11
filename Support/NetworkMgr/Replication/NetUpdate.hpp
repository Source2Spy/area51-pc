//==============================================================================
//
//  NetUpdate.hpp
//
//==============================================================================

#ifndef NET_UPDATE_HPP
#define NET_UPDATE_HPP

#include "NetObj.hpp"

#ifndef X_EDITOR

#include "Objects/Actor/Actor.hpp"
#include "Debris/debris_meson_lash.hpp"
#include <memory>

//==============================================================================

struct net_update
{
    virtual ~net_update( void ) {}
    virtual void        Read            ( const bitstream& BS ) = 0;
    virtual void        Apply           ( netobj& Object ) const = 0;
    virtual xbool       IsActivation    ( void ) const = 0;

    static std::unique_ptr<net_update> Decode( netobj::type Type, s32 Slot, const bitstream& BS );
};

struct net_actor_update : net_update
{
    update              Data;
    virtual void        Read            ( const bitstream& BS );
    virtual void        Apply           ( netobj& Object ) const;
    virtual xbool       IsActivation    ( void ) const { return (Data.DirtyBits & netobj::ACTIVATE_BIT) != 0; }
};

struct net_projectile_update : net_update
{
    explicit net_projectile_update( netobj::type ObjectType ) : Type( ObjectType ) {}

    netobj::type        Type;
    xbool               Activate;
    s32                 OriginNetSlot;
    xbool               Contagious;
    f32                 WanderPhase;
    f32                 WanderOffset;

    xbool               AtRest;

    xbool               HasPosition;
    vector3             Position;
    radian3             Orientation;
    s32                 PositionZone1;
    s32                 PositionZone2;

    xbool               HasStart;
    vector3             StartPosition;
    vector3             StartVelocity;
    s32                 StartZone1;
    s32                 StartZone2;
    s32                 ImpactCount;
    s32                 TargetNetSlot;
    xbool               ClearBlend;

    xbool               HasTime;
    f32                 Time;

    xbool               HasAttach;
    xbool               Attached;
    s32                 AttachedNetSlot;
    s32                 AttachedBoneIndex;
    vector3             AttachedLocalPosition;

    xbool               Exploded;
    vector3             ExplosionPosition;

    virtual void        Read            ( const bitstream& BS );
    virtual void        Apply           ( netobj& Object ) const;
    virtual xbool       IsActivation    ( void ) const { return Activate; }
};

struct net_pickup_update : net_projectile_update
{
    net_pickup_update( void ) : net_projectile_update( netobj::TYPE_PICKUP ) {}
    xbool               HasItem;
    s32                 Item;
    s32                 Amount;
    xbool               Spins;

    xbool               HasState;
    s32                 State;

    xbool               HasRequest;
    s32                 RequestPlayer;

    xbool               HasAssign;
    s32                 AssignPlayer;

    xbool               HasProjectile;

    virtual void        Read            ( const bitstream& BS );
    virtual void        Apply           ( netobj& Object ) const;
    virtual xbool       IsActivation    ( void ) const { return HasItem && HasProjectile && Activate; }
};

struct net_flag_update : net_update
{
    xbool               Activate;
    u32                 TeamBits;

    xbool               TimeoutImminent;
    xbool               Capture;
    xbool               Teleport;

    xbool               HasPosition;
    xbool               Attached;
    s32                 Player;
    vector3             Position;
    radian              Yaw;
    s32                 Zone1;
    s32                 Zone2;
    xbool               Falling;

    xbool               HasTimeout;
    xbool               Timeout;
    f32                 TimeoutAge;
    f32                 CurrentAge;

    virtual void        Read            ( const bitstream& BS );
    virtual void        Apply           ( netobj& Object ) const;
    virtual xbool       IsActivation    ( void ) const { return Activate; }
};

struct net_flag_base_update : net_update
{
    xbool               Activate;
    vector3             Position;
    radian3             Orientation;
    s32                 Zone1;
    s32                 Zone2;
    s32                 Circuit;

    virtual void        Read            ( const bitstream& BS );
    virtual void        Apply           ( netobj& Object ) const;
    virtual xbool       IsActivation    ( void ) const { return Activate; }
};

struct net_cap_point_update : net_update
{
    xbool               Activate;
    vector3             Position;
    f32                 Elevation;
    f32                 Radius;
    radian3             Orientation;
    s32                 Zone1;
    s32                 Zone2;
    s32                 Circuit;

    xbool               HasMode;
    s32                 Mode;
    f32                 Value;

    virtual void        Read            ( const bitstream& BS );
    virtual void        Apply           ( netobj& Object ) const;
    virtual xbool       IsActivation    ( void ) const { return Activate; }
};

struct net_meson_update : net_update
{
    struct target
    {
        s32 Slot;
        s32 Type;
        s32 Bone;
        xbool Valid;
        s32 LifeSeq;
    };

    xbool               Activate;
    s32                 Owner;
    vector3             Origin;
    vector3             Normal;
    u16                 Zones;

    xbool               HasTargets;
    s32                 TargetCount;
    target              Targets[MAX_LASH_TARGETS];

    xbool               HasNextTarget;
    s32                 NextTarget;

    xbool               Collapse;

    xbool               HasStatus;
    u16                 KilledMask;

    virtual void        Read            ( const bitstream& BS );
    virtual void        Apply           ( netobj& Object ) const;
    virtual xbool       IsActivation    ( void ) const { return Activate; }
};

#endif // X_EDITOR
#endif // NET_UPDATE_HPP
