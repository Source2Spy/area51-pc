
#ifndef CLOTH_OBJECT_HPP
#define CLOTH_OBJECT_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "Obj_mgr/obj_mgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "Objects/Cloth.hpp"
#include "Objects/Render/ClothInst.hpp"
#include "Characters/FloorProperties.hpp"

//=========================================================================
// DEFINES
//=========================================================================

#define CLOTH_DATA_VERSION      1000

//=========================================================================
// CLASS
//=========================================================================

class cloth_object : public object
{
public:

    CREATE_RTTI( cloth_object, object, object )

                            cloth_object        ( void );
                           ~cloth_object        ( void );

    virtual void            OnInit              ( void ) ;
    virtual void            OnKill              ( void ) ;
    virtual         bbox    GetLocalBBox        ( void ) const;
    virtual         s32     GetMaterial         ( void ) const { return MAT_TYPE_NULL ; }
    virtual void            OnEnumProp          ( prop_enum&    List );
    virtual xbool           OnProperty          ( prop_query&   I    );
    virtual void            OnPain              ( const pain& Pain ) ;
    virtual void            OnRenderShadowCast  ( u64 ProjMask );

    rigid_inst&             GetRigidInst        ( void ) { return ( m_ClothRender.GetRigidInst() ) ; }
    
    virtual const object_desc&  GetTypeDesc     ( void ) const;
    static  const object_desc&  GetObjectType   ( void );

    virtual void            OnProjectileImpact  ( const object&     Projectile,
                                                  const vector3&    Velocity,
                                                        u32         CollPrimKey, 
                                                  const vector3&    CollPoint,
                                                        xbool       PunchDamageHole = TRUE,        // TRUE to punch out a hole
                                                        f32         ManualImpactForce = -1.0f );   // <= 0 : normal behaviour for bullets


    virtual render_inst*    GetRenderInstPtr    ( void ) { return &(GetRigidInst()); }
                                                                                                        // >  0 : scale force to this length
            void            SetActive           ( xbool bActive );

protected:

    virtual void            OnImport            ( text_in& TIn );
    virtual void            OnAdvanceSimulation        ( f32 DeltaTime );
    virtual void            OnRender            ( void );

    virtual void            OnColCheck          ( void );
    virtual void            OnPolyCacheGather   ( void );
    virtual xbool           GetColDetails       ( s32 Key, detail_tri& Tri );

#ifndef X_RETAIL
    virtual void            OnColRender         ( xbool bRenderHigh );
#endif // X_RETAIL

    virtual void            OnMove              ( const vector3& NewPos ) ;
    virtual void            OnTransform         ( const matrix4& L2W    ) ;
    
protected:

    floor_properties    m_FloorProperties;
    cloth               m_ClothSim ;     // Cloth simulation
    cloth_inst          m_ClothRender ;  // Cloth rendering (rigid inst + dynamic cloth mesh)
    f32                 m_ActiveTimer ;  // If >0, cloth is updated

#ifdef X_EDITOR    
    xbool   m_bDrawDebug;   // Draw debug info
#endif
    
};

//=========================================================================
// END
//=========================================================================
#endif
