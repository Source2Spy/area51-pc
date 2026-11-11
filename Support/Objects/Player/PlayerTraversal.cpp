//=========================================================================
//
//  PlayerTraversal.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "Configuration/GameConfig.hpp"
#include "Objects/JumpPad.hpp"
#include "Objects/Ladders/Ladder_Field.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

guid player::IsInLadderField( void )
{
    // Lookup character physics
    const character_physics& Physics = m_Physics ;

    // Search for being inside a ladder
    g_ObjMgr.SelectBBox( object::ATTR_COLLIDABLE, GetBBox(), object::TYPE_LADDER_FIELD) ;
    slot_id SlotID = g_ObjMgr.StartLoop();
    while(SlotID != SLOT_NULL)
    {
        // Lookup object and quit loop
        ladder_field* pLadder = (ladder_field*)g_ObjMgr.GetObjectBySlot(SlotID) ;
        ASSERT(pLadder) ;

        // Overlapping this ladder?
        if (  (pLadder->GetGuid() != m_JumpedOffLadderGuid)         // did we just jump off this ladder?
            && pLadder->DoesCylinderIntersect(Physics.GetPosition(),// are we in the field? 
            Physics.GetColHeight(), 
            Physics.GetColRadius()))
        {
            g_ObjMgr.EndLoop();
            return pLadder->GetGuid() ;
        }

        // Check next object
        SlotID = g_ObjMgr.GetNextResult(SlotID) ;
    }
    g_ObjMgr.EndLoop();

    // No ladder found
    return 0 ;
}

//===========================================================================

// Ladder tweakables
static f32 LADDER_CLIMB_SPEED       = 300.0f ;  // Vertical speed
static f32 LADDER_STRAFE_SPEED      = 100.0f ;  // Horizontal speed
static f32 LADDER_FLIP_UP_ANGLE     = 25.0f ;   // Angle at which to swap up/down when facing ladder
static f32 LADDER_AT_TOP_OFFSET     = 10.0f ;   // Dismount distance from top
static f32 LADDER_AT_BOT_OFFSET     = 10.0f ;   // Dismount distance from bottom
static f32 LADDER_JUMP_OFF_VEL      = 150.0f ;  // Push away vel when jumping
//static f32 LADDER_MOVING_AWAY_VEL   = 300.0f ;   // Vel threshold for detecting moving away from ladder

xbool player::UpdateLadderMovement( f32 DeltaTime )
{
    // Lookup physics to use
    character_physics& Physics = m_Physics ;

    // Default to not being on a ladder
    m_bOnLadder = FALSE ;
    m_LadderOutDir.Zero() ;
    Physics.SetUseGravity(TRUE) ;

    // On a ladder?
    guid LadderGuid = IsInLadderField() ;
    if (!LadderGuid)
        return FALSE ;

    // Get ladder object
    const ladder_field* pLadder = (ladder_field*)g_ObjMgr.GetObjectByGuid(LadderGuid) ;
    ASSERT(pLadder) ;

    // Lookup ladder object info
    const matrix4& LadderL2W = pLadder->GetL2W() ;

    // Setup local direction vectors
    vector3 Out (0,0,1) ;
    vector3 Up  (0,1,0) ;
    vector3 Side(1,0,0) ;

    // Compute ladder world direction vectors
    vector3 LadderOut  = LadderL2W.RotateVector(Out) ;
    vector3 LadderUp   = LadderL2W.RotateVector(Up) ;
    vector3 LadderSide = LadderL2W.RotateVector(Side) ;

    // Compute ladder climb plane
    plane LadderOutPlane ;
    LadderOutPlane.Setup(pLadder->GetPosition(), LadderOut) ;
    f32 Dist = LadderOutPlane.Distance(GetPosition()) ;

    // Behind the ladder? (eg. when entering the ladder from the top of a ledge)
    if (Dist < 0)
        return FALSE ;

    // Compute player facing forward direction
    vector3 PlayerForward(Out) ;
    PlayerForward.RotateX(m_Pitch) ;
    PlayerForward.RotateY(m_Yaw) ;

    // Compute player running direction after getting off ladder
    vector3 PlayerRun(Out * m_fMoveValue) ;
    PlayerRun.RotateY(m_Yaw) ;

    // Flip up direction if player is looking down the ladder
    xbool bLookingDownLadder = (LadderUp.Dot(PlayerForward) < x_cos(DEG_TO_RAD(90.0f + LADDER_FLIP_UP_ANGLE))) ;
    if (bLookingDownLadder)
        LadderUp = -LadderUp ;

    // Flip side direction if player is looking out from ladder
    if (LadderOut.Dot(PlayerForward) < 0)
        LadderSide  = -LadderSide ;

    // Compute up/down/side movement from input
    vector3 UpDownVelocity = LadderUp   * m_fMoveValue   * LADDER_CLIMB_SPEED;
    vector3 SideVelocity   = LadderSide * m_fStrafeValue * LADDER_STRAFE_SPEED;

    // Get ladder and feet info
    f32 Top    = pLadder->GetTop() ;
    f32 Bottom = pLadder->GetBottom() ;
    f32 Feet   = GetPosition().GetY() ;

    // If moving down and past bottom of ladder, push the player off ready for dismount
    if ((UpDownVelocity.GetY() < 0) && (Feet < (Bottom + LADDER_AT_BOT_OFFSET)))
        SideVelocity += PlayerRun * LADDER_CLIMB_SPEED;

    // Running into the ladder?
    if (LadderOut.Dot(PlayerRun) < 0)
    {
        // If near top and moving up, push the player off ready dismount
        if ( (UpDownVelocity.GetY() > 0) && (Feet > (Top - LADDER_AT_TOP_OFFSET)) )
            SideVelocity += PlayerRun * LADDER_CLIMB_SPEED;
    }
    else
    {
        // If pulling away from the ladder, pulling back on the stick, and looking down, then let go!
        if ((PlayerRun.Dot(LadderOut) > 0) && (m_fMoveValue < 0) && (bLookingDownLadder))
            return FALSE ;

        // If the player is NOT facing the ladder and moving up, then stop from going off the top
        if ( (UpDownVelocity.GetY() > 0) && (Feet > (Top - LADDER_AT_TOP_OFFSET)) )
            UpDownVelocity.Zero();
    }

    // Compute final vel
    vector3 const FinalVelocity = UpDownVelocity + SideVelocity;
    m_ForwardVelocity = UpDownVelocity;
    m_StrafeVelocity  = SideVelocity;
    m_DeltaPos        = FinalVelocity * DeltaTime;

    // Clear velocity so player doesn't shoot up/down if no input
    Physics.ClearMotionVelocity() ;

    // Turn off gravity
    Physics.SetUseGravity(FALSE) ;

    // Update
    Physics.Advance( Physics.GetPosition() + m_DeltaPos, DeltaTime );
    OnMove( Physics.GetPosition() );

    // Record player is on a ladder
    m_bOnLadder         = TRUE ;
    m_Physics.DetachFromGround();
    m_LastLadderGuid    = LadderGuid;
    m_LadderOutDir      = LadderOut ;

    return TRUE ;
}

//===========================================================================

xbool player::Jump( void )
{

    // Lookup physics
    character_physics& Physics = m_Physics ;

    // Jump off a ladder?
    if( m_bOnLadder )
    {
        m_bOnLadder = FALSE ;
        m_JumpedOffLadderGuid = m_LastLadderGuid;
        vector3 Vel = Physics.GetMotionVelocity();
        Vel += m_LadderOutDir * LADDER_JUMP_OFF_VEL;
        Physics.SetUseGravity( TRUE );
        Physics.SetMotionVelocity( Vel );
        m_bJumpStarted = TRUE;
        return TRUE;
    }
    else
    {
        f32 JumpVelocity = m_JumpVelocity;
        if( g_MPTweaks.Active )
            JumpVelocity *= g_MPTweaks.JumpSpeed;

        // Jump vertically
        if( Physics.Jump( JumpVelocity ) )
        {
            m_bJumpStarted = TRUE;
            return TRUE;
        }
    }

    return FALSE;
}

//===========================================================================

void player::HitJumpPad( const vector3& Velocity, 
                               f32      DeltaTime, 
                               f32      AirControl, 
                               xbool    BoostOnly,
                               xbool    ReboostOnly,
                               xbool    Instantaneous,
                               guid     JumpPadGuid )
{
    if( Instantaneous )
    {
        m_Physics.FlingWithVelocity( Velocity,
                                     AirControl,
                                     BoostOnly,
                                     ReboostOnly,
                                     JumpPadGuid );
    }
    else
    {
        m_Physics.FlingWithAcceleration( Velocity,
                                         DeltaTime,
                                         AirControl,
                                         BoostOnly,
                                         ReboostOnly );
    }

    if( Instantaneous && !ReboostOnly )
    {
        #ifndef X_EDITOR
        if( !(m_WayPointFlags & WAYPOINT_TELEPORT_FX) )
        {
            slot_id Slot = g_ObjMgr.GetFirst( TYPE_JUMP_PAD );
            while( Slot != SLOT_NULL )
            {
                object* pObject = g_ObjMgr.GetObjectBySlot( Slot );
                ASSERT( pObject );
                ASSERT( pObject->GetType() == TYPE_JUMP_PAD );
                vector3 Gap = pObject->GetPosition() - GetPosition();
                if( Gap.LengthSquared() < 250.0f )
                {
                    jump_pad* pJumpPad = (jump_pad*)pObject;
                    pJumpPad->PlayJump();
                    break;
                }
                Slot = g_ObjMgr.GetNext( Slot );
            }

            m_NetDirtyBits    |= WAYPOINT_BIT;
            m_WayPointFlags   |= WAYPOINT_JUMP_PAD_FX;
            m_WayPointTimeOut  = 0;
        }
        #endif
    }
}

//===========================================================================

void player::UpdateFellFromAltitude( void )
{
    // Use the current position if we have moved upwards or are not falling
    f32 Altitude = GetPosition().GetY();
    if( (Altitude > m_FellFromAltitude) || !m_bFalling )
    {
        m_FellFromAltitude = Altitude;
    }
}
