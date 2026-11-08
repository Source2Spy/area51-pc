//=========================================================================
// INCLUDES
//=========================================================================
#include "Render/PrimitiveDebug.hpp"
#include "Camera.hpp"
#include "Render/Render.hpp"
#include "Render/Editor/EditorIcons.hpp"
#include "../MiscUtils/SimpleUtils.hpp"
#include "Path.hpp"
#include "Characters/Character.hpp"
#include "Objects/Player/Player.hpp"
#include "Objects/HudObject.hpp"
#include "UI/ui_renderer.hpp"
#include "TriggerEx/TriggerEx_Object.hpp"
#include "TriggerEx/Actions/action_set_property.hpp"

//=========================================================================
// GLOBALS
//=========================================================================

extern xbool g_first_person;

#ifdef X_EDITOR
static const char* s_PlayerCinemaCameraProperty = "Player\\Cinema\\CinemaCameraGuid";
#endif


//=========================================================================
// OBJECT DESCRIPTION
//=========================================================================

static struct camera_desc : public object_desc
{
    camera_desc( void ) : object_desc( 
            object::TYPE_CAMERA, 
            "Camera", 
            "SCRIPT",
#ifdef X_EDITOR
            object::ATTR_RENDERABLE | object::ATTR_NEEDS_LOGIC_TIME |
            object::ATTR_SPACIAL_ENTRY,

#else // X_EDITOR
            object::ATTR_NEEDS_LOGIC_TIME,
#endif // X_EDITOR
            FLAGS_GENERIC_EDITOR_CREATE | FLAGS_TARGETS_OBJS |
            FLAGS_IS_DYNAMIC ) {}

    //---------------------------------------------------------------------
    virtual object* Create          ( void ) { return new camera; }

    //---------------------------------------------------------------------

#ifdef X_EDITOR

    virtual s32     OnEditorRender  ( object& Object ) const 
    { 
        object_desc::OnEditorRender( Object );

        return static_cast<s32>( EditorIcon::Camera );
    }

#endif // X_EDITOR

} s_camera_Desc;

//=========================================================================

const object_desc& camera::GetTypeDesc( void ) const
{
    return s_camera_Desc;
}

//=========================================================================

const object_desc& camera::GetObjectType( void )
{
    return s_camera_Desc;
}
//=========================================================================
// FUNCTIONS
//=========================================================================

//=========================================================================

camera::camera( void ) : tracker()
{
    m_FieldOfView = R_60 ;
    m_FarClip     = 10000.0f ;
    m_PipEarVolume   = 1.0f ;
    m_PipEarNearClip = 1.0f ;
    m_PipEarFarClip  = 1.0f ;
    m_TargetPos.Zero();
}

//=============================================================================

void camera::OnEnumProp( prop_enum& List )
{
    // Call base class
    tracker::OnEnumProp( List );

    // Add properties
    List.PropEnumHeader("Camera", "Properties for the camera object", 0 );

    s32 iHeader = List.PushPath( "Camera\\" );    

    //guid specific fields
    List.PropEnumGuid  ("TargetGuid",  "Guid of object to look at (if any)", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SHOW | PROP_TYPE_DONT_SAVE);
    m_ObjectAffecter.OnEnumProp( List, "Target" ); //must come after the "TargetGuid" property

    List.PropEnumAngle ("FieldOfView", "Field of view for camera.", 0 ) ;
    List.PropEnumFloat ("FarClip",     "Far clipping plane distance from camera.", 0 ) ;
    
    List.PropEnumFloat ("PipEarVolume",   "Pip ear volume - if this camera is used by a pip.", 0 ) ;
    List.PropEnumFloat ("PipEarNearClip", "Pip ear near clip scaler.", 0 ) ;
    List.PropEnumFloat ("PipEarFarClip",  "Pip ear far clip scaler.", 0 ) ;

    List.PopPath( iHeader );
}

//=============================================================================

xbool camera::OnProperty( prop_query& I )
{
    // Call base class
    if( tracker::OnProperty( I ) )
        return TRUE ;

    if( I.IsSimilarPath( "Camera" ) )
    {
        s32 iHeader = I.PushPath( "Camera\\" );        

        if( m_ObjectAffecter.OnProperty( I, "Target" ) )
        {
            //new guid specific fields
        }
        else if (I.IsVar("TargetGuid"))
        {
            //old Target guid
            if( I.IsRead() )
            {
                I.SetVarGUID( m_ObjectAffecter.GetGuid() );
            }
            else
            {
                //write the new target
                m_ObjectAffecter.SetStaticGuid(I.GetVarGUID());
            }
        }
        else if (I.VarAngle("FieldOfView", m_FieldOfView, 0, R_180))
        {
            // FieldOfView?
        }
        else if (I.VarFloat("FarClip", m_FarClip,0, 50000))
        {
            // Far clip?
        }
        else if (I.VarFloat("PipEarVolume", m_PipEarVolume, 0.0f, 1.0f))
        {
            // PipEarVolume?
        }
        else if (I.VarFloat("PipEarNearClip", m_PipEarNearClip, 0.0f, 10.0f))
        {
            // PipEarNearClip?
        }
        else if (I.VarFloat("PipEarFarClip", m_PipEarFarClip, 0.0f, 10.0f))
        {
            // PipEarFarClip?
        }
        else
        {
            I.PopPath( iHeader );
            return FALSE;
        }

        I.PopPath( iHeader );
        return TRUE;
    }

    return FALSE ;
}

//=========================================================================

void camera::OnMove( const vector3& NewPos )
{
    // So that the camera rotation is updated (if there is a target)
    // we actually need to call OnTransform!
    matrix4 L2W = GetL2W() ;
    L2W.SetTranslation(NewPos) ;
    OnTransform(L2W) ;
}

//=========================================================================

void camera::OnCameraCut( const matrix4& L2W )
{
    g_ZoneMgr.RelocateZoneTracking( *this, m_ZoneTracker, L2W.GetTranslation() );
    OnTransform( L2W );
}

//=========================================================================

void camera::OnTransform( const matrix4& L2W )
{
    // Lookup position
    vector3 NewPos = L2W.GetTranslation();

    // Lookup target guid
    guid TargetGuid = m_ObjectAffecter.GetGuid();

    // Get target pos from tracker?
    object_ptr<tracker> pTracker( TargetGuid );
    if( pTracker )
    {
        m_TargetPos = pTracker->GetCurrentKey().m_Position;
    }        
    else
    {
        // Get target pos from character?
        object_ptr<character> pCharacter( TargetGuid );
        if( pCharacter )
        {
            m_TargetPos = pCharacter->GetPositionWithOffset( character::OFFSET_EYES );
        }            
        else
        {
            // Get target pos from object?
            object_ptr<object> pObject( TargetGuid );
            if ( pObject )
            {
                m_TargetPos = pObject->GetPosition();
            }                
            else
            // If no target, then face forwards
            if ( TargetGuid == 0 )
            {
                // Update target
                m_TargetPos = NewPos + L2W.RotateVector( vector3( 0,0,5 ) );
            }                
            else
            {
                // Leave the target untouched
            }
        }
    }

    // Update view properties
    m_View.SetXFOV( m_FieldOfView );
    m_View.SetZLimits( 10.0f, m_FarClip );

    // Compute new L2W using target?
    matrix4 LookL2W;
    if( TargetGuid )
    {
        // Setup rotation
        vector3 DeltaTarget = m_TargetPos - NewPos;
        if( DeltaTarget.LengthSquared() > F32_MIN )
        {
            LookL2W.Identity();
            LookL2W.RotateX( DeltaTarget.GetPitch() );
            LookL2W.RotateY( DeltaTarget.GetYaw() );
            LookL2W.PreRotateZ( m_CurrentKey.m_Roll );
        }
        else
        {
            LookL2W = L2W;
        }

        // Setup translation
        LookL2W.SetTranslation( NewPos );
    }
    else
    {
        // Just use the transform as is so that cinema roll comes through for the player
        LookL2W = L2W;
    }
    
    // Update view
    m_View.SetV2W( LookL2W );

    // Call base class with final LookL2W!
    tracker::OnTransform( LookL2W );
}

//=========================================================================

//=========================================================================

void camera::OnInit( void )
{
    // Call base class
    tracker::OnInit() ;
}

//=========================================================================

#ifndef X_RETAIL
void camera::OnDebugRender  ( void )
{
#ifdef X_EDITOR
    X_PROFILE_SCOPE_CATEGORY( "Context", "camera::OnDebugRender" );

    // Make sure view is updated!
    UpdateTrackedTransform(FALSE) ;

    // Draw frustrum where view is
    render::debug::Frustum(m_View, XCOLOR_RED, 100.0f) ;
    render::debug::Line(m_View.GetPosition(), m_TargetPos, XCOLOR_RED) ;
#endif // X_EDITOR
}
#endif // X_RETAIL

//=========================================================================

void camera::OnRender  ( void )
{
#ifdef X_EDITOR
    X_PROFILE_SCOPE_CATEGORY( "Context", "camera::OnRender" );

    // Make sure view is updated!
    UpdateTrackedTransform(FALSE) ;

    // Draw frustrum where view is
    render::debug::Frustum(m_View, XCOLOR_RED, 100.0f) ;
    render::debug::Line(m_View.GetPosition(), m_TargetPos, XCOLOR_RED) ;
#endif // X_EDITOR
}

//=========================================================================

void camera::UpdateTrackedTransform( xbool bSendKeyEvents, xbool bDiscontinuous )
{
    // Call base class which will update L2W
    tracker::UpdateTrackedTransform(bSendKeyEvents, bDiscontinuous) ;

    // If the camera is not on a path, then we need to call an OnTransform
    // so that the look direction is updated!
    if (m_PathGuid == 0)
        OnTransform(GetL2W()) ;

    // Over-ride field of view?
    path* pPath = GetPath() ;
    if (pPath)
    {
        if (pPath->GetFlags() & path::FLAG_KEY_FIELD_OF_VIEW)
        {
            m_FieldOfView = m_CurrentKey.m_FieldOfView ;
            m_View.SetXFOV( m_FieldOfView );
        }
    }
}

//=========================================================================

void camera::RenderView( const irect& Viewport )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "camera::RenderView" );

    // Keep a copy of the current view.
    view View = *eng_GetView();

    // Make sure the view is up to date with position and target.
    UpdateTrackedTransform( FALSE );

    // Set and activate the camera view.
    m_View.SetViewport( Viewport.l, Viewport.t, Viewport.r, Viewport.b );
    eng_SetView( m_View );
    eng_SetViewport( m_View );

#ifdef X_EDITOR
    // The editor draws the camera view after the normal render.
    if( eng_Begin( "camera::RenderViewEditorClear" ) )
    {
        g_UIRenderer.DrawRect( Viewport, XCOLOR_BLACK );
        eng_End();
    }
#endif // X_EDITOR

    // Render the world.
    g_ObjMgr.Render3dObjects( TRUE, m_View, m_ZoneTracker );

#ifdef X_EDITOR
    // Fill the camera viewport depth buffer in the editor.
    if( eng_Begin( "Fill camera viewport Z buffer" ) )
    {
        render::SetDepthRect( Viewport, 0.0f );
        eng_End();
    }
#endif // X_EDITOR

    // Restore the previous view.
    eng_SetView( View );
    eng_SetViewport( View );
}

//=========================================================================

void camera::RenderView( const irect& Viewport, render::RenderScopeDesc const& Scope )
{
    render::ScopedRenderView RenderViewScope( Scope );
    view const SavedView = *eng_GetView();
    x_try;
    RenderView( Viewport );
    if ( !render::EndRenderScope() )
        return;
    x_catch_begin;
    render::ResetAfterException();
    eng_SetView( SavedView );
    eng_SetViewport( SavedView );
    return;
    x_catch_end;
}

//=========================================================================

xbool camera::IsEditorSelected( void )
{
    // Is this object selected?
    if (tracker::IsEditorSelected())
        return TRUE ;

    guid ObjGuid = m_ObjectAffecter.GetGuid();

    // Is target object selected?
    object_ptr<object> pObject(ObjGuid) ;
    if (pObject)
    {
        // Is object selected?
        if (pObject->GetAttrBits() & (object::ATTR_EDITOR_SELECTED | object::ATTR_EDITOR_PLACEMENT_OBJECT))
            return TRUE ;
    }

    // Is targe a tracker?
    object_ptr<tracker> pTracker(ObjGuid) ;
    if (pTracker)
    {
        // Is tracker object selected?
        if (pTracker->IsEditorSelected())
            return TRUE ;
    }

    // Not selected
    return FALSE ;
}

//=========================================================================

#ifdef X_EDITOR

static
xbool IsCinemaCameraAction( actions_ex_base* pAction, guid CameraGuid )
{
    if( !pAction )
        return FALSE;

    // Must be set property action
    if( pAction->GetType() == actions_ex_base::TYPE_ACTION_AFFECT_PROPERTY )
    {
        action_set_property* pSetProperty = (action_set_property*)pAction;
        
        // Setting a player cinema camera guid to use this camera?
        if(         ( pSetProperty->GetCode() == action_set_property::PMOD_CODE_SET )
                &&  ( ( pSetProperty->GetPropertyType() & PROP_TYPE_BASIC_MASK ) == PROP_TYPE_GUID )
                &&  ( pSetProperty->GetPropertyGuid() == CameraGuid )
                &&  ( x_strcmp( pSetProperty->GetPropertyName(), s_PlayerCinemaCameraProperty ) == 0 ) )
        {                
            return TRUE; 
        }
    }
        
    // Not found
    return FALSE;
}

//=========================================================================

static
xbool IsCinemaCamera( guid CameraGuid )
{
    slot_id     SlotID     = SLOT_NULL;
    s32         i          = 0;

    // Loop through all players and search for this camera being used in a cinema
    SlotID = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
    while( SlotID != SLOT_NULL )
    {
        // Get player
        player* pPlayer = (player*)g_ObjMgr.GetObjectBySlot( SlotID );
        ASSERT( pPlayer );

        // Used in cinema?
        if( pPlayer->GetCinemaCameraGuid() == CameraGuid )
            return TRUE;

        // Check next player
        SlotID = g_ObjMgr.GetNext( SlotID );
    }

    // Loop through all triggers to see this camera is referenced in a cinema
    SlotID = g_ObjMgr.GetFirst( object::TYPE_TRIGGER_EX );
    while( SlotID != SLOT_NULL )
    {
        // Get trigger
        trigger_ex_object* pTrigger = (trigger_ex_object*)g_ObjMgr.GetObjectBySlot( SlotID );
        ASSERT( pTrigger );

        // Loop through actions
        for( i = 0; i < pTrigger->GetActionCount(); i++ )
        {
            // Is this a set property action?
            actions_ex_base* pAction = pTrigger->GetAction( i );
            ASSERT( pAction );
            if( IsCinemaCameraAction( pAction, CameraGuid ) )
                return TRUE;
        }

        // Loop through else actions
        for( i = 0; i < pTrigger->GetElseActionCount(); i++ )
        {
            // Is this a set property action?
            actions_ex_base* pAction = pTrigger->GetElseAction( i );
            ASSERT( pAction );
            if( IsCinemaCameraAction( pAction, CameraGuid ) )
                return TRUE;
        }
        
        // Check next trigger
        SlotID = g_ObjMgr.GetNext( SlotID );
    }
    
    // Not found
    return FALSE;
}

//=========================================================================

void camera::RenderEditorView( void )
{
    s32 W, H;
    f32 PixelScale;

    // Lookup the current view info
    const view* pCurrView = eng_GetView();
    ASSERT( pCurrView );
    rect CurrVP;
    pCurrView->GetViewport( CurrVP );
    
    // Setup size
    xbool bIsCinemaCamera = IsCinemaCamera( GetGuid() );
    if( bIsCinemaCamera )
    {
        // Cinema display.
        W          = 256;
        H          = 224;
        PixelScale = DEFAULT_PIXEL_SCALE;
    }
    else
    {
        // PIP display.
        W          = 256;
        H          = 128;
        PixelScale = DEFAULT_PIXEL_SCALE;
    }

    // Setup viewport    
    irect Viewport;
    Viewport.l = ( (s32)CurrVP.GetWidth() - W ) / 2;
    Viewport.r = Viewport.l + W;
    Viewport.t = g_first_person ? (s32)( CurrVP.GetHeight() * 0.21f ) : 8;
    Viewport.b = Viewport.t + H;

    // Skip if bigger than the window
    if( W > (s32)CurrVP.GetWidth() )
        return;
    if( H > (s32)CurrVP.GetHeight() )
        return;
        
    // Render
    m_View.SetPixelScale( PixelScale );
    RenderView( Viewport );
    
    // Render letter box?
    if( ( bIsCinemaCamera ) && ( eng_Begin() ) )
    {
        g_UIRenderer.PushScreenSpace( Viewport );
        hud_object::RenderLetterBox( Viewport, 1.0f );
        g_UIRenderer.PopScreenSpace();
        eng_End();
    }

    // Render red border
    if( eng_Begin() )    
    {
        g_UIRenderer.DrawRect( Viewport, XCOLOR_RED, TRUE );
        eng_End();
    }
}

#endif

//=========================================================================

void camera::OnAdvanceSimulation( f32 DeltaTime )
{
    // Call base class for camera position
    tracker::OnAdvanceSimulation(DeltaTime) ;
}

//=========================================================================
