//=========================================================================
//
// TeamLight.cpp
//
//=========================================================================

//=========================================================================
// INCLUDES
//=========================================================================
#include "Render/PrimitiveDebug.hpp"
#include "TeamLight.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "x_profile.hpp"
#include "NetworkMgr/Replication/NetObjMgr.hpp"
#include "Player/Player.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

#ifndef X_EDITOR
#include "GameLib/RenderContext.hpp"
#endif

//=========================================================================

//=========================================================================
// OBJECT DESCRIPTION
//=========================================================================

static struct team_light_desc : public object_desc
{
    team_light_desc( void ) : object_desc( 
        object::TYPE_TEAM_LIGHT, 
        "Team Light",
        "Multiplayer",
        object::ATTR_RENDERABLE                 |
        object::ATTR_NEEDS_LOGIC_TIME,

        FLAGS_GENERIC_EDITOR_CREATE | 
        FLAGS_IS_DYNAMIC            
    ) {}         
                             

    //-------------------------------------------------------------------------

    virtual object* Create( void ) { 
        return new team_light; 
    }

    //-------------------------------------------------------------------------

#ifdef X_EDITOR
    virtual s32 OnEditorRender( object& Object ) const
    { 

        render::debug::Marker( Object.GetPosition(), XCOLOR_GREEN );

        if( Object.IsKindOf( team_light::GetRTTI() ) )
        {
            team_light  TeamLight = team_light::GetSafeType( Object );
            xcolor      Color = TeamLight.GetCircuit().GetColor();

            matrix4 L2W = TeamLight.GetL2W();
            
            vector3 Translation = L2W.GetTranslation();
            
            // Render this upside down.
            L2W.Translate( -Translation );
            L2W.RotateZ( R_180 );
            L2W.Translate(  Translation );

            DrawEditorIcon( EditorIcon::Light, 
                L2W, 
                !!(TeamLight.GetAttrBits() & object::ATTR_EDITOR_SELECTED), 
                Color );

            if(Object.GetAttrBits() & object::ATTR_EDITOR_SELECTED)
            {
                render::debug::Box( Object.GetBBox(), XCOLOR_WHITE );
            }

            if( mp_settings::s_Selected )
                TeamLight.GetCircuit().SpecialRender( TeamLight.GetPosition() );
        }
        else
        {
            ASSERT( FALSE );
        }

        return( -1 );
    }
#endif // X_EDITOR

} s_team_light_Desc;

//=========================================================================

const object_desc& team_light::GetTypeDesc( void ) const
{
    return s_team_light_Desc;
}

//=========================================================================

const object_desc& team_light::GetObjectType( void )
{
    return s_team_light_Desc;
}


//=========================================================================
// FUNCTIONS
//=========================================================================

team_light::team_light( void )
{ 
    m_NewState = FRIENDLY_ALL;
    m_OldState = FRIENDLY_ALL;
    m_TransitionValue = 1.0f;
    
    m_States[ FRIEND_TO_TEAM  ] = &m_Friend;
    m_States[ FRIEND_TO_ENEMY ] = &m_Foe;
    m_States[ FRIEND_TO_ALL   ] = &m_FriendAll;
    m_States[ FRIEND_TO_NONE  ] = &m_FoeAll;

    m_Friend.m_Color        = XCOLOR_GREEN;
    m_Foe.m_Color           = XCOLOR_RED;
    m_FriendAll.m_Color     = XCOLOR_BLUE;
    m_FoeAll.m_Color        = XCOLOR_YELLOW;

    m_Radius       = 400.0f;
    m_Intensity    = 1.0f;
    m_Falloff      = 1.0f;

    // Setting it double what it should be because otherwise 
    // it disappears before it should.
    m_RenderBBox.Min.Set( -m_Radius * 2.0f, -m_Radius * 2.0f, -m_Radius * 2.0f );
    m_RenderBBox.Max.Set(  m_Radius * 2.0f,  m_Radius * 2.0f,  m_Radius * 2.0f );

}

//=========================================================================

team_light::~team_light( void )
{ 
}

//=========================================================================

void team_light::OnInit( void )
{
}

//=========================================================================

bbox team_light::GetLocalBBox( void ) const
{
    return m_RenderBBox;
}

//=========================================================================

#ifndef X_RETAIL
void team_light::OnDebugRender( void )
{
}
#endif // X_RETAIL

//==============================================================================

inline xcolor Interpolate( xcolor Color1, xcolor Color2, f32 Percentage )
{
    xcolor TmpColor( 
        (u8)(Color2.R * Percentage + Color1.R * (1.0f - Percentage)),
        (u8)(Color2.G * Percentage + Color1.G * (1.0f - Percentage)),
        (u8)(Color2.B * Percentage + Color1.B * (1.0f - Percentage)),
        (u8)(Color2.A * Percentage + Color1.A * (1.0f - Percentage))
        );
    return TmpColor;
}

//=========================================================================

void team_light::OnRender( void )
{
}

//=========================================================================

void team_light::OnCollectLight( void )
{
#ifndef X_EDITOR
    X_PROFILE_SCOPE_CATEGORY( "Context", "team_light::OnCollectLight" );

    if( m_Circuit.GetCircuit() == 15 )
    {
        return;
    }

    player* pPlayer = (player*)NetObjMgr.GetObjFromSlot( g_RenderContext.NetPlayerSlot );

    u32 States[ 2 ]     = { m_OldState, m_NewState };
    u32 FadePoints[ 2 ] = { FRIEND_TO_ALL, FRIEND_TO_ALL };
    
    // We know what the alignment is, just check how that relates to the player viewing it.
    for( s32 i = 0; i < 2; i++ )
    {
        switch( States[ i ] )
        {
        case FRIENDLY_NONE:
            FadePoints[ i ] = FRIEND_TO_NONE;
            break;

        case FRIENDLY_ALPHA: // Fall through.
        case FRIENDLY_OMEGA:
            FadePoints[ i ] = (pPlayer->net_GetTeamBits() & States[ i ]) ? FRIEND_TO_TEAM : FRIEND_TO_ENEMY;
            break;
        
        case FRIENDLY_ALL:
            FadePoints[ i ] = FRIEND_TO_ALL;
            break;
        
        default:
            break;
        }
    }

    xcolor Color  = Interpolate( m_States[ FadePoints[ 0 ] ]->m_Color, 
                                 m_States[ FadePoints[ 1 ] ]->m_Color, 
                                 m_TransitionValue );
    f32 InnerRadius = MAX( 0.0f, m_Radius * ( 1.0f - m_Falloff ) );

    render::GetRenderPipeline().GetLightSystem().AddDynamicLight( GetPosition(),
                                Color,
                                m_Radius,
                                m_Intensity,
                                FALSE,
                                render::LightSystem::LIGHT_SHAPE_OMNI,
                                FALSE,
                                InnerRadius,
                                vector3( 0.0f, 0.0f, 1.0f ),
                                m_Falloff );
#endif
}

//=========================================================================

void team_light::OnAdvanceSimulation ( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "team_light::OnAdvanceSimulation" );

    u32 TeamBits = m_Circuit.GetTeamBits();

    // If the state has changed, swap the state values and reset the 
    // transition time to start the fading process.
    if( m_NewState != TeamBits )
    {
        m_OldState = m_NewState;
        m_NewState = TeamBits;
        m_TransitionValue = 0.0f;
    }
    
    f32 TransitionTime = 2.0f;

    m_TransitionValue = MINMAX( 0.0f, 
                                m_TransitionValue + (DeltaTime / TransitionTime), 
                                1.0f );
}

//==============================================================================

void team_light::OnEnumProp( prop_enum&  rPropList )
{
    m_Circuit.OnEnumProp( rPropList );
    object::OnEnumProp( rPropList );

    rPropList.PropEnumHeader( "TeamLight", "Light with team dependant rendering.", 0 );

    rPropList.PropEnumFloat( "TeamLight\\Radius",    "Radius for the dynamic light.",    0 );
    rPropList.PropEnumFloat( "TeamLight\\Intensity", "Intensity for the dynamic light.", 0 );
    rPropList.PropEnumFloat( "TeamLight\\Falloff",   "Falloff for the dynamic light.",   0 );

    const char* Prefixes[]  = { "FriendAll", "Friend", "Foe", "FoeAll" };
    s32 i;
    for( i = 0; i < 4; i++ )
    {
        rPropList.PropEnumHeader( xfs( "TeamLight\\%s", Prefixes[ i ] ), "View Attributes", 0 );

        s32 ID = rPropList.PushPath( xfs( "TeamLight\\%s\\", Prefixes[ i ] ) );
        rPropList.PropEnumColor( "Color",     "Color for the dynamic light.",     0 );

        rPropList.PopPath( ID );
    }
}

//=============================================================================

xbool team_light::OnProperty( prop_query& rPropQuery )
{    
    if( object::OnProperty( rPropQuery ) )
    {
        return TRUE;
    }

    if( m_Circuit.OnProperty( rPropQuery ) )
    {
        return TRUE;
    }

    s32 iPath = rPropQuery.PushPath( "TeamLight\\" );

    if( rPropQuery.VarFloat     ( "Radius",     m_Radius          ) ) 
    {
        m_RenderBBox.Min.Set( -m_Radius * 2.0f, -m_Radius * 2.0f, -m_Radius * 2.0f );
        m_RenderBBox.Max.Set(  m_Radius * 2.0f,  m_Radius * 2.0f,  m_Radius * 2.0f );
        return TRUE;
    }
    if( rPropQuery.VarFloat     ( "Intensity",  m_Intensity       ) ) return TRUE;
    if( rPropQuery.VarFloat     ( "Falloff",    m_Falloff, 0.0f, 1.0f ) ) return TRUE;


    const char* Prefixes[]  = { "FriendAll", "Friend", "Foe", "FoeAll" };

    s32 i;
    for( i = 0; i < 4; i++ )
    {
        s32 iPath = rPropQuery.PushPath(  xfs( "%s\\", Prefixes[ i ] ) );
        if( rPropQuery.VarColor     ( "Color",      m_States[ i ]->m_Color           ) ) return TRUE;

        rPropQuery.PopPath( iPath );
    }

    rPropQuery.PopPath( iPath );

    return( FALSE );
}
