//=============================================================================
//  DEBRIS_RIGID.CPP 
//=============================================================================

//=============================================================================
// INCLUDES
//=============================================================================

#include "Debris/debris_rigid.hpp"

//=============================================================================
// CONSTANTS
//=============================================================================

static const f32 kLIFE_SCALE = 2.0f;
static const f32 kFADE_TIME  = 3.0f;

//=============================================================================
// OBJECT DESC.
//=============================================================================
static struct debris_rigid_desc : public object_desc
{
    debris_rigid_desc( void ) : object_desc( 
        object::TYPE_DEBRIS_RIGID, 
        "Debris_Rigid",
        "EFFECTS",
        object::ATTR_NEEDS_LOGIC_TIME   |
        object::ATTR_RENDERABLE         | 
        object::ATTR_NO_RUNTIME_SAVE,
        FLAGS_IS_DYNAMIC )
    {}
    
    virtual object* Create          ( void )
    {
        return new debris_rigid;
    }
    
} s_debris_rigid_desc ;

//=============================================================================================

const object_desc&  debris_rigid::GetTypeDesc ( void ) const
{
    return s_debris_rigid_desc;
}

//=============================================================================================

const object_desc&  debris_rigid::GetObjectType ( void )
{
    return s_debris_rigid_desc;
}

//=============================================================================

debris_rigid::debris_rigid(void) :
    debris()
{
    m_FadeTime  = 0.0f;
    m_VMeshMask = 0;
    m_VMeshMask = ~m_VMeshMask;
}

//=============================================================================

debris_rigid::~debris_rigid()
{
}

//=============================================================================

void debris_rigid::OnInit          ( void )
{
//    m_RigidInst.OnProperty( g_PropQuery.WQueryExternal( "RenderInst\\File", pFileName ) );
}

//=============================================================================

void debris_rigid::Create( const char*            rigidFileName,
                           const vector3&         startingPosition,
                           const vector3&         startingVelocity,
                           f32                    lifeSpan,
                           xbool                  bBounces )
{
    m_RigidInst.SetUpRigidGeom( rigidFileName );  //.SetUpRigidGeom( rigidFileName  );

    static radian InitSpinRate = R_60;
    m_Spin = radian3(x_frand(-InitSpinRate,InitSpinRate), x_frand(-InitSpinRate,InitSpinRate),x_frand(-InitSpinRate,InitSpinRate));

    lifeSpan *= kLIFE_SCALE;
    m_FadeTime = MIN( kFADE_TIME, MAX( 0.0f, lifeSpan ) );

    debris::Create( NULL, startingPosition, startingVelocity, lifeSpan, bBounces );

    m_FloorProperties.Init( 1.0f, 0.128f, GetGuid() );
    m_FloorProperties.ForceUpdate( startingPosition );
}

//=============================================================================

void debris_rigid::Create( rigid_inst&            Inst,
                           const vector3&         startingPosition,
                           const vector3&         startingVelocity,
                           f32                    lifeSpan,
                           xbool                  bBounces )
{
    xarray<prop_container>  ContainerList;
    Inst.OnCopy( ContainerList );
    m_RigidInst.OnPaste( ContainerList );


    m_Spin = radian3(x_frand(-R_20,R_20), x_frand(-R_20,R_20),x_frand(-R_20,R_20));

    lifeSpan *= kLIFE_SCALE;
    m_FadeTime = MIN( kFADE_TIME, MAX( 0.0f, lifeSpan ) );

    debris::Create( NULL, startingPosition, startingVelocity, lifeSpan, bBounces );

    m_FloorProperties.Init( 1.0f, 0.128f, GetGuid() );
    m_FloorProperties.ForceUpdate( startingPosition );
}

//=============================================================================

bbox debris_rigid::GetLocalBBox ( void ) const
{
    rigid_geom* pRigidGeom = m_RigidInst.GetRigidGeom();

    if( pRigidGeom )
    {
        return( pRigidGeom->m_Collision.BBox );
    }
    
    return( bbox( vector3( 20.0f, 20.0f, 20.0f),
                  vector3(-20.0f,-20.0f,-20.0f) ) );
}

//=============================================================================

void debris_rigid::OnAdvanceSimulation ( f32 DeltaTime )
{
    debris::OnAdvanceSimulation( DeltaTime );
    m_FloorProperties.Update( GetPosition(), DeltaTime );
}

//=============================================================================

void debris_rigid::OnRender ( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "debris_rigid::OnRender");
    rigid_geom* pRigidGeom = m_RigidInst.GetRigidGeom();
    
    if( pRigidGeom )
    {
        u32 Flags = (GetFlagBits() & object::FLAG_CHECK_PLANES) ? render::CLIPPED : 0;

        const matrix4& L2W = GetL2W();

        u8 Alpha = 255;
        if( m_FadeTime > 0.0f )
        {
            Alpha = (u8)(255.0f * x_clamp( m_LifeSpan / m_FadeTime, 0.0f, 1.0f ));
        }
        if( Alpha < 255 )
        {
            Flags |= render::FADING_ALPHA;
        }

        ASSERT( pRigidGeom->m_nBones <= 1 );
        m_RigidInst.SetVMeshMask( m_VMeshMask );
        m_RigidInst.Render( &L2W, Flags, m_RigidInst.GetLODMask(L2W), Alpha, m_FloorProperties.GetColor() );
    }
}

//=============================================================================

void debris_rigid::UpdatePhysics   ( f32 DeltaTime )
{
    if(m_Inactive )
        return;
    X_PROFILE_SCOPE_CATEGORY( "Context", "debris_rigid::UpdatePhysics");
    //update the spin
    f32 fTimeSpeed = DeltaTime * 4.0f;

    if(m_Velocity.LengthSquared() > 35.0f*35.0f )
    {
        m_TotalSpin += radian3(m_Spin.Pitch * fTimeSpeed, m_Spin.Yaw * fTimeSpeed, m_Spin.Roll * fTimeSpeed);
    }
    
    debris::UpdatePhysics(DeltaTime);
}

//=============================================================================




















