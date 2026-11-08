#ifndef    __COKE_CAN_HPP__
#define __COKE_CAN_HPP__

//=========================================================================
// INCLUDES
//=========================================================================

#include "Obj_mgr/obj_mgr.hpp"
#include "x_array.hpp"
#include "Objects/Render/SkinInst.hpp"
#include "Characters/FloorProperties.hpp"

//=========================================================================
// FORWARD DECLARATIONS
//=========================================================================
struct coke_can_profile;


//=========================================================================
// CLASS
//=========================================================================

class coke_can : public object
{
//=========================================================================
// Structures
//=========================================================================
private:
    struct particle
    {
        // Members
        vector3 m_BindPos;     // Position of particle when binded to geometry
        vector3 m_Pos;         // Current world position of particle
        vector3 m_Velocity;    // World velocity in centimeters per second
        vector3 m_LastCollPos; // Last collision free position

        // Initialization
        particle() :
            m_Pos(0,0,0),
            m_Velocity(0,0,0),
            m_LastCollPos(0,0,0)
        {
        }

    };

    //-------------------------------------------------------------------------

    struct velocity_constraint
    {
        particle* pParticleA;
        particle* pParticleB;
        plane     Plane;
        f32       Distance;
        f32       InvMassA;
        f32       InvMassB;
    };

//=========================================================================
// Class info
//=========================================================================
public:

    CREATE_RTTI( coke_can, object, object )
    virtual const object_desc&  GetTypeDesc     ( void ) const;    
    static  const object_desc&  GetObjectType   ( void );
//=========================================================================
// Public functions
//=========================================================================
public:
                        coke_can               ( void );
    virtual            ~coke_can               ( void );

    virtual bbox        GetLocalBBox            ( void ) const;
            bbox        GetGeomBBox             ( void ) const;
    virtual s32         GetMaterial                ( void ) const { return MAT_TYPE_SOLID_METAL; }
    virtual void        OnRender                ( void );
    virtual void        OnRenderShadowCast      ( u64 ProjMask );
    virtual const char* GetLogicalName          ( void )   { return "COKE_CAN"; }

#ifndef X_RETAIL
    virtual void        OnColRender             ( xbool bRenderHigh );
#endif // X_RETAIL

    virtual void        OnAdvanceSimulation          ( f32 DeltaTime );
    virtual void        OnPain                  ( const pain& Pain );    
    virtual void        OnColCheck              ( void );
    virtual void        OnMove                  ( const vector3& NewPos );        
    virtual void        OnTransform             ( const matrix4& L2W    ); 

#ifdef X_EDITOR
    virtual s32         OnValidateProperties    ( xstring& ErrorMsg );
#endif // X_EDITOR

//=========================================================================
// Private functions
//=========================================================================
protected:

            // Misc
            const coke_can_profile& GetProfile( void ) const;

            // Physics functions
            void    InitPhysics                 ( void );
            f32     GetSpeedSquaredSum          ( void ) const;
            xbool   IsMoving                    ( void ) const;
            void    UpdateL2W                   ( void );
            void    Integrate                   ( f32 DeltaTime );
            void    ApplyEqualDistConstraint    ( particle& ParticleA, particle& ParticleB, f32 EqualDist );
            f32     ApplyMinDistConstraint      ( particle& ParticleA, particle& ParticleB, f32 MinDist,
                                                  f32 InvMassA, f32 InvMassB,
                                                  f32 Elasticity, f32 Friction );
            void    ApplyDistConstraints        ( void );
            xbool   ApplyCapsuleConstraint      ( const vector3& Bottom, const vector3& Top, f32 Radius,
                                                  f32 ContactOffset, const vector3& Velocity );
            void    ApplyCollConstraints        ( void );
            void    CollectCanNeighbors         ( f32 DeltaTime, xarray<guid>& Neighbors );
            xbool   HasCanContact               ( const coke_can& CokeCan ) const;
            void    WakeCanContacts             ( const xarray<guid>& Neighbors );
            void    ApplyCanConstraints         ( coke_can& CokeCan, xbool WakeNeighbors );
            void    ApplyConstraints            ( const xarray<guid>& Neighbors, xbool WakeNeighbors );
            xbool   AddWorldConstraint          ( particle& Particle, const plane& Plane );
            void    SolveVelocityConstraints    ( void );
            void    ApplyDamping                ( f32 DeltaTime );

//=========================================================================
// Public functions
//=========================================================================
public:
            xbool   ApplyActorConstraints       ( const vector3& Position, f32 Radius, f32 Height,
                                                  f32 ContactOffset, const vector3& Velocity );




//=========================================================================
// Editor functions
//=========================================================================
protected:
    
    virtual void    OnEnumProp      ( prop_enum&    List );
    virtual xbool   OnProperty      ( prop_query&   I    );

//=========================================================================
// Profiles
//=========================================================================
public:
    enum {
            PROFILE_CAN         = 0,
            PROFILE_BARREL      = 1,
            
            PROFILE_COUNT
         };

//=========================================================================
// Data
//=========================================================================
protected:  

    // Flags
    u32                 m_isInitialized : 1;         // TRUE if initialized
    u32                 m_bOnGround    : 1;         // TRUE if lying on the ground

    // Physics    
    f32                 m_ActiveSeconds;            // Keeps physics active after an impact
    particle            m_Particles[2];             // Particles
    f32                 m_ParticleRadius;           // Radius of particles
    f32                 m_ParticleDist;             // Constraint distance
    radian              m_Roll;                     // Roll of can
    radian              m_RollRate;                 // Roll rate in radians per second
    f32                 m_ImpactAudioCooldownSeconds;
    s32                 m_iMajorAxis;               // Longest axis of can
    vector3             m_MinInitVel;               // Min initial velocity
    vector3             m_MaxInitVel;               // Max initial velocity
    s32                 m_RollAudioID;              // Can rolling audio id
    s32                 m_iProfile;                 // Which physics profile to use

    xarray<velocity_constraint> m_VelocityConstraints;

    // Rendering
    skin_inst           m_SkinInst;                 // Skinned inst
    floor_properties    m_FloorProperties;          // Floor tracking class
};

//=========================================================================


//=========================================================================
// END
//=========================================================================
#endif
