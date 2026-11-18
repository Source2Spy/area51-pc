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

// Ladder tweakables
static f32 LADDER_CLIMB_SPEED       = 300.0f ;  // Vertical speed
static f32 LADDER_STRAFE_SPEED      = 100.0f ;  // Horizontal speed
static f32 LADDER_FLIP_UP_ANGLE     = 25.0f ;   // Angle at which to swap up/down when facing ladder
static f32 LADDER_AT_TOP_OFFSET     = 10.0f ;   // Dismount distance from top
static f32 LADDER_TOP_CLIMB_FACTOR  = 0.25f ;   // Vertical speed fraction while clearing the top
static f32 LADDER_AT_BOT_OFFSET     = 10.0f ;   // Dismount distance from bottom
static f32 LADDER_JUMP_OFF_VEL      = 150.0f ;  // Push away vel when jumping
//static f32 LADDER_MOVING_AWAY_VEL   = 300.0f ;   // Vel threshold for detecting moving away from ladder

//===========================================================================

guid player::IsInLadderField( void )
{
    // Lookup character physics
    const character_physics& Physics = m_Physics ;

    // Clear the last ladder after landing outside its field
    if (m_LadderReattachBlockedGuid && Physics.IsGrounded())
    {
        const object* pObject = g_ObjMgr.GetObjectByGuid(m_LadderReattachBlockedGuid) ;
        if (  !pObject
           || (pObject->GetType() != object::TYPE_LADDER_FIELD)
           || !(pObject->GetAttrBits() & object::ATTR_COLLIDABLE)
           || !((const ladder_field*)pObject)->DoesCylinderIntersect(Physics.GetPosition(),
                                                                     Physics.GetColHeight(),
                                                                     Physics.GetColRadius()) )
        {
            m_LadderReattachBlockedGuid = NULL_GUID ;
        }
    }

    // Keep the current ladder at the top where the capsule rounds away from the field
    if (m_bOnLadder && (m_LastLadderGuid != m_LadderReattachBlockedGuid))
    {
        const object* pObject = g_ObjMgr.GetObjectByGuid(m_LastLadderGuid) ;
        if (  pObject
           && (pObject->GetType() == object::TYPE_LADDER_FIELD)
           && (pObject->GetAttrBits() & object::ATTR_COLLIDABLE) )
        {
            const ladder_field* pLadder = (const ladder_field*)pObject ;
            const f32 Skin = Physics.GetMovementSettings().ContactOffset ;
            if (pLadder->CanKeepGripAtTop(Physics.GetPosition(), Physics.GetColRadius(), Skin, LADDER_AT_TOP_OFFSET))
            {
                return m_LastLadderGuid ;
            }
        }
    }

    // Search for being inside a ladder
    g_ObjMgr.SelectBBox( object::ATTR_COLLIDABLE, GetBBox(), object::TYPE_LADDER_FIELD) ;
    slot_id SlotID = g_ObjMgr.StartLoop();
    while(SlotID != SLOT_NULL)
    {
        // Lookup object and quit loop
        ladder_field* pLadder = (ladder_field*)g_ObjMgr.GetObjectBySlot(SlotID) ;
        ASSERT(pLadder) ;

        // Overlapping this ladder?
        if (  (pLadder->GetGuid() != m_LadderReattachBlockedGuid)       // did we just get off this ladder?
           && pLadder->DoesCylinderIntersect(Physics.GetPosition(),   // are we in the field?
                                            Physics.GetColHeight(),
                                            Physics.GetColRadius()) )
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

xbool player::UpdateLadderMovement( f32 DeltaTime )
{
    // Lookup physics to use
    character_physics& Physics = m_Physics ;

    // On a ladder?
    guid LadderGuid = IsInLadderField() ;
    if (!LadderGuid && m_LadderReattachBlockedGuid && Physics.IsGrounded())
        LadderGuid = m_LadderReattachBlockedGuid ;

    const xbool bWasOnLadder = m_bOnLadder && (LadderGuid == m_LastLadderGuid) ;

    // Default to not being on a ladder
    m_bOnLadder = FALSE ;
    m_LadderOutDir.Zero() ;

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
    const f32 PlaneOffset = bWasOnLadder ? Physics.GetMovementSettings().ContactOffset : 0.0f ;

    // Behind the ladder? (eg. when entering the ladder from the top of a ledge)
    if (Dist < -PlaneOffset)
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

    const f32 RunOutward = LadderOut.Dot(PlayerRun) ;

    // Let the player climb again after landing without having to leave the field
    if (LadderGuid == m_LadderReattachBlockedGuid)
    {
        if (  (UpDownVelocity.GetY() <= 0)
           || (RunOutward >= 0)
           || (Feet >= (Top - LADDER_AT_TOP_OFFSET)) )
        {
            return FALSE ;
        }

        m_LadderReattachBlockedGuid = NULL_GUID ;
    }

    // If moving down and near the bottom of the ladder, let go even if the capsule is still in the field
    if ((UpDownVelocity.GetY() < 0) && (Feet <= (Bottom + LADDER_AT_BOT_OFFSET)))
    {
        if (bWasOnLadder)
            m_LadderReattachBlockedGuid = LadderGuid ;
        return FALSE ;
    }

    const xbool bClimbingNearTop   = (UpDownVelocity.GetY() > 0) && (Feet > (Top - LADDER_AT_TOP_OFFSET)) ;
    const xbool bClimbingOntoLedge = (RunOutward < 0) && bClimbingNearTop ;

    // Running into the ladder?
    if (RunOutward < 0)
    {
        // If near top and moving up, push the player off ready dismount
        if (bClimbingOntoLedge)
        {
            const CharacterMovementSettings Settings = Physics.GetMovementSettings() ;
            vector3 RunDir = PlayerRun ;
            RunDir.Normalize() ;

            // Check past the requested move so the ground tolerance does not hide a blocked ledge
            const vector3 RunVelocity = PlayerRun * LADDER_CLIMB_SPEED ;
            const vector3 TestPos     = Physics.GetPosition() + RunVelocity * DeltaTime +
                                        RunDir * (Settings.GroundTolerance + Settings.ContactOffset) ;
            if (Physics.CanMoveTo(TestPos))
            {
                f32 Blend = x_clamp((Feet - Top + LADDER_AT_TOP_OFFSET) / LADDER_AT_TOP_OFFSET, 0.0f, 1.0f) ;
                Blend = Blend * Blend * (3.0f - 2.0f * Blend) ;

                // Blend from climbing up to moving onto the ledge
                UpDownVelocity *= 1.0f - Blend * (1.0f - LADDER_TOP_CLIMB_FACTOR) ;
                SideVelocity   += RunVelocity * Blend ;
            }
        }
    }
    else
    {
        // If pulling away from the ladder, pulling back on the stick, and looking down, then let go!
        if ((RunOutward > 0) && (m_fMoveValue < 0) && (bLookingDownLadder))
            return FALSE ;

        // If the player is NOT facing the ladder and moving up, then stop from going off the top
        if (bClimbingNearTop)
            UpDownVelocity.Zero();
    }

    // Build up ground movement speed before getting off at the top
    if (bClimbingOntoLedge)
        EvaluateMovementSpeeds(DeltaTime) ;
    else
        m_Movement.Clear() ;

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

    // If past the ladder plane or clear of the top, dismount and turn gravity back on
    if (  bClimbingOntoLedge
       && (  (LadderOutPlane.Distance(Physics.GetPosition()) < -PlaneOffset)
          || (Physics.GetPosition().GetY() > (Top + LADDER_AT_TOP_OFFSET)) ) )
    {
        m_LadderReattachBlockedGuid = LadderGuid ;

        // Use the surface below the player instead of dropping from the top of the field
        if (!Physics.TryAttachToGround(LADDER_AT_TOP_OFFSET))
            Physics.SetUseGravity(TRUE) ;

        OnMove( Physics.GetPosition() );
        return TRUE ;
    }

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
        m_LadderReattachBlockedGuid = m_LastLadderGuid;
        vector3 Vel = Physics.GetMotionVelocity();
        Vel += m_LadderOutDir * LADDER_JUMP_OFF_VEL;
        Physics.SetUseGravity( TRUE );
        // Apply push away vel in the air only
        Physics.SetMotionVelocity( Vel, FALSE );
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
