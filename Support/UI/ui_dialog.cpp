//=========================================================================
//
//  ui_dialog.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "ui_dialog.hpp"
#include "ui_manager.hpp"
#include "ui_control.hpp"
#include "ui_edit.hpp"
#include "ui_font.hpp"
#include "StringMgr/StringMgr.hpp"
#include "AudioMgr/AudioMgr.hpp"

//=========================================================================
//  Defines
//=========================================================================

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

// define the global dialog colors
xcolor   ui_dialog::m_TextColorNormal;
xcolor   ui_dialog::m_TextColorShadow;

//=========================================================================
//  Factory function
//=========================================================================

ui_win* ui_dialog_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    ui_dialog* pDialog = new ui_dialog;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  ui_dialog
//=========================================================================

ui_dialog::ui_dialog( void )
{
    m_IsNavTextVisible = TRUE;
}

//=========================================================================

ui_dialog::~ui_dialog( void )
{
}

//=========================================================================

xbool ui_dialog::Create( s32                        UserID,
                         ui_manager*                pManager,
                         ui_manager::dialog_tem*    pDialogTem,
                         const irect&               Position,
                         ui_win*                    pParent,
                         s32                        Flags,
                         void*                      pUserData)
{
    xbool   Success = FALSE;
    s32     i;
    s32     x, y;

    ASSERT( pManager );

    // Get pointer to user
    ui_manager::user* pUser = pManager->GetUser( UserID );
    ASSERT( pUser );

    // Do window creation
    Success = ui_win::Create( UserID, pManager, Position, pParent, Flags );

    // Setup Dialog specific stuff
    m_iElement          = m_pManager->FindElement( "frame" );
    ASSERT( m_iElement != -1 );
    m_pDialogTem        = pDialogTem;
    m_NavW              = pDialogTem ? pDialogTem->NavW : 0;
    m_NavH              = pDialogTem ? pDialogTem->NavH : 0;
    m_NavX              = 0;
    m_NavY              = 0;

    m_BackgroundColor   = xcolor( 40, 40, 0, 192 );

    m_InputEnabled      = TRUE;
    m_pUserData         = pUserData;
    m_State             = DIALOG_STATE_INIT;
    m_CurrentControl    = -1;
    m_NavText.Clear();
    m_IsNavTextVisible  = TRUE;


    // Setup Navgraph size
    m_NavGraph.SetCapacity( m_NavW * m_NavH );
    for( i=0; i<(m_NavW*m_NavH); i++ )
        m_NavGraph.Append() = 0;

    if( pDialogTem )
    {
        if( pDialogTem->StringID )
        {
            const xwchar* pLabel = g_StringTableMgr( "ui", pDialogTem->StringID );
            ASSERTS( pLabel, "Missing localized dialog label" );
            m_Label = pLabel;
        }
        else
        {
            m_Label.Clear();
        }
    }
 
    // Save Template Pointer
    m_pDialogTem = pDialogTem;

    // Create controls and add to navigation graph if template exists
    if( pDialogTem )
    {
        for( i=0; i<pDialogTem->nControls; i++ )
        {
            ui_manager::control_tem*    pControlTem = &pDialogTem->pControls[i];

            // Templates and controls share the same logical coordinate space.
            irect Pos;
            Pos.Set( pControlTem->x, pControlTem->y, pControlTem->x + pControlTem->w, pControlTem->y + pControlTem->h );

            ui_control* pControl = (ui_control*)pManager->CreateWin( UserID, pControlTem->pClass, Pos, this, pControlTem->Flags );
            ASSERT( pControl );

            // Assign control ID
            pControl->SetControlID( pControlTem->ControlID );

            // Configure the control
            static const xwchar s_EmptyLabel[] = { 0 };
            const xwchar* pLabel = s_EmptyLabel;
            if( pControlTem->StringID )
            {
                pLabel = g_StringTableMgr( "ui", pControlTem->StringID );
                ASSERTS( pLabel, "Missing localized control label" );
            }
            pControl->SetLabel( pLabel );
            if( x_strcmp( pControlTem->pClass, "edit" ) == 0 )
            {
                ui_edit*    pEdit = (ui_edit*)pControl;
                pEdit->SetVirtualKeyboardTitle( pLabel );
            }

            // Add the control to the navigation graph
            ASSERT( pControlTem->nx >= 0 );
            ASSERT( pControlTem->ny >= 0 );
            ASSERT( (pControlTem->nx + pControlTem->nw) <= m_NavW );
            ASSERT( (pControlTem->ny + pControlTem->nh) <= m_NavH );

            // Save navgrid position
            pControl->SetNavPos( irect( pControlTem->nx, pControlTem->ny, pControlTem->nx+pControlTem->nw, pControlTem->ny+pControlTem->nh ) );

            // Add control into navgrid
            for( y=pControlTem->ny; y<(pControlTem->ny+pControlTem->nh); y++ )
            {
                for( x=pControlTem->nx; x<(pControlTem->nx+pControlTem->nw); x++ )
                {
                    m_NavGraph[x+y*m_NavW] = pControl;
                }
            }
        }
    }

    // Return success code
    return Success;
}

//=========================================================================

void ui_dialog::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Get window rectangle
        irect  r;
        r.Set( m_Position.l+ox, m_Position.t+oy, m_Position.r+ox, m_Position.b+oy );
        irect  rb = r;
        rb.Deflate( 1, 1 );

        // Clipping remains in logical coordinates until renderer submission.
        irect clipRect = r;

        const xbool WipeClipped     = m_pManager->IsWipeActiveFor( this );
        const xbool ScalingClipped  = m_pManager->IsScreenScaling();
        const xbool ChildrenClipped = WipeClipped || ScalingClipped;
        if( WipeClipped )
        {
            clipRect.b = MIN( clipRect.b, m_pManager->GetWipeRevealY() );
            if( clipRect.b < clipRect.t )
            {
                clipRect.b = clipRect.t;
            }
        }

        if( ChildrenClipped )
        {
            m_pManager->PushClipWindow( clipRect );
        }

        // render the background highlight
        m_pManager->RenderScreenHighlight();

        // Render children
        for( s32 i=0; i<m_Children.GetCount(); i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }

        if( ChildrenClipped )
        {
            m_pManager->PopClipWindow();
        }

        m_pManager->RenderScreenWipe( this );

        // Render frame
        if( m_Flags & WF_BORDER )
        {
            if( m_Flags & WF_DISABLED )
            {
                m_pManager->RenderElement( m_iElement, r, 1 );
            }
            else
            {
                m_pManager->RenderElement( m_iElement, r, 0 );
            }


            // Render Title
            if (!m_pManager->IsScreenScaling())
            {
                rb.Deflate( 0, 5 );
                s32 FontID = m_pManager->FindFont("large");
                m_pManager->RenderText( FontID, rb, ui_font::h_center, m_TextColorShadow, m_Label );
                rb.Translate( -1, -1 );
                m_pManager->RenderText( FontID, rb, ui_font::h_center, m_TextColorNormal, m_Label );
            }

            // render screen glow effect
            m_pManager->RenderScreenGlow();
        }

    }
}

//=========================================================================

//=========================================================================
//  Message Handler Functions
//=========================================================================
//=========================================================================

void ui_dialog::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    (void)pWin;
    (void)Presses;
    (void)Repeats;

    if (!m_InputEnabled)
        return;

    ui_manager::user*   pUser   = m_pManager->GetUser( m_UserID );
    s32                 x       = m_NavX;
    s32                 y       = m_NavY;
    s32                 dx      = 0;
    s32                 dy      = 0;

    // Which way are we moving
    switch( Code )
    {
    case ui_navigation::Up:
        dy = -1;
        break;
    case ui_navigation::Down:
        dy = 1;
        break;
    case ui_navigation::Left:
        dx = -1;
        break;
    case ui_navigation::Right:
        dx = 1;
        break;

    default:
        break;
    }

    if( (dx == 0) && (dy == 0) )
    {
        return;
    }

    xbool bWrapped = FALSE;

    // Scan until the control to move to is found
    ui_win* pCurrentWin = pUser->pFocusedWindow;
    while( ((x < m_NavW) && (x >= 0)) && ((y < m_NavH) && (y >= 0)) )
    {
        ui_win* pCandidate = m_NavGraph[x+y*m_NavW];
        if( pCandidate && (pCandidate != pCurrentWin) )
        {
            xbool Found = FALSE;

            if( !pCandidate->CanFocus() )
            {
                if( dy != 0 )
                {
                    // look to the left and right
                    s32 xleft = x;
                    s32 xright = x;
                    xbool doneLeft = 0;
                    xbool doneRight = 0;
                    
                    while( 1 )
                    {
                        // look left
                        if( xleft > 0 )
                        {
                            xleft--;
                            pCandidate = m_NavGraph[xleft+y*m_NavW];
                            if( pCandidate && (pCandidate != pCurrentWin) && pCandidate->CanFocus() )
                            {
                                x = xleft;
                                Found = TRUE;
                                break;
                            }
                        }
                        else
                        {
                            doneLeft = TRUE;
                        }

                        // look right
                        if( xright < (m_NavW-1) )
                        {
                            xright++;
                            pCandidate = m_NavGraph[xright+y*m_NavW];
                            if( pCandidate && (pCandidate != pCurrentWin) && pCandidate->CanFocus() )
                            {
                                x = xright;
                                Found = TRUE;
                                break;
                            }
                        }
                        else
                        {
                            doneRight = TRUE;
                        }

                        if( doneLeft && doneRight )
                            break;
                    }
                }
            }
            else
            {
                Found = TRUE;
            }

            if( Found )
            {
                m_pManager->SetFocusWindow( m_UserID, pCandidate );
                m_NavX = x;
                m_NavY = y;
                m_CurrentControl = m_Children.Find( pCandidate );
                break;
            }
        }

        // Advance to next position
        x += dx;
        y += dy;

        // Check for wrapping
        if ( (WrapX) && (!bWrapped) )
        {
            if (x < 0)
            {
                x = m_NavW-1;
                bWrapped = TRUE;
            }

            if (x == m_NavW)
            {
                x = 0;
                bWrapped = TRUE;
            }
        }

        if ( (WrapY) && (!bWrapped) )
        {
            if (y < 0)
            {
                y = m_NavH-1;
                bWrapped = TRUE;
            }

            if (y == m_NavH)
            {
                y = 0;
                bWrapped = TRUE;
            }
        }
    }
}

//=========================================================================

void ui_dialog::OnFocusWithin( ui_win* pWin )
{
    if( m_NavW > 0 )
    {
        for( s32 i = 0; i < m_NavGraph.GetCount(); i++ )
        {
            if( m_NavGraph[i] == pWin )
            {
                m_NavX = i % m_NavW;
                m_NavY = i / m_NavW;
                break;
            }
        }
    }

    s32 const iControl = m_Children.Find( pWin );
    if( iControl != -1 )
    {
        m_CurrentControl = iControl;
    }
}

//=========================================================================

void ui_dialog::SetBackgroundColor( xcolor Color )
{
    m_BackgroundColor = Color;
}

//=========================================================================

xcolor ui_dialog::GetBackgroundColor( void ) const
{
    return m_BackgroundColor;
}

//=========================================================================

ui_control* ui_dialog::GotoControl( s32 iControl )
{
    ui_control* pControl = NULL;

    ASSERT( (iControl >= 0) && (iControl < m_Children.GetCount()) );

    if( (iControl < 0) || (iControl >= m_Children.GetCount()) )
    {
        return NULL;
    }

    ui_control* pChild = (ui_control*)m_Children[iControl];

    ASSERT( pChild );
    if( !pChild ) return NULL;

    if( pChild->CanFocus() )
    {
        // Set focus on this control
        m_pManager->SetFocusWindow( m_UserID, pChild );

        // Set Navigation cursor to center of the control
        const irect& r = pChild->GetNavPos( );
        m_NavX = r.l + r.GetWidth() / 2;
        m_NavY = r.t + r.GetHeight() / 2;

        // store index of current control
        m_CurrentControl = iControl;

        pControl = pChild;
    }
    return pControl;
}

//=========================================================================

xbool ui_dialog::GotoControl( ui_control* pControl )
{
    xbool   Success = FALSE;
    s32     iControl = -1;

    ui_control* pChild = NULL;

    // Locate the control
    for( s32 i=0; i<m_Children.GetCount(); i++ )
    {
        if( m_Children[i] == pControl )
        {
            pChild = (ui_control*)m_Children[i];
            iControl = i;
        }
    }
    ASSERT( pChild );
    if( !pChild )
    {
        return FALSE;
    }

    if( pChild->CanFocus() )
    {
        // Set focus on this control
        m_pManager->SetFocusWindow( m_UserID, pChild );

        // Set Navigation cursor to center of the control
        const irect& r = pChild->GetNavPos( );
        m_NavX = r.l + r.GetWidth() / 2;
        m_NavY = r.t + r.GetHeight() / 2;

        // store index of current control
        m_CurrentControl = iControl;

        Success = TRUE;
    }

    return Success;
}

//=========================================================================

s32 ui_dialog::GetNumControls( void ) const
{
    return m_Children.GetCount();
}

//=========================================================================

void ui_dialog::SetNavText( const xwstring& Text )
{
    m_NavText = Text;
}

//=========================================================================

void ui_dialog::SetNavText( const xwchar* pText )
{
    ASSERT( pText );
    if( pText )
    {
        m_NavText = pText;
    }
    else
    {
        m_NavText.Clear();
    }
}

//=========================================================================

void ui_dialog::SetNavTextVisible( xbool IsVisible )
{
    m_IsNavTextVisible = IsVisible;
}

//=========================================================================

const xwstring& ui_dialog::GetNavText( void ) const
{
    return m_NavText;
}

//=========================================================================

xbool ui_dialog::IsNavTextVisible( void ) const
{
    return m_IsNavTextVisible;
}

//=========================================================================

void ui_dialog::InitScreenScaling( const irect& Position )
{
    // store requested frame size
    m_RequestedPos = Position;

    // set starting position
    m_pManager->GetScreenSize(m_CurrPos);
    m_StartPos = m_CurrPos;
    
    // set up scaling
    m_scaleCount = 0.3f; // time to scale in seconds
    m_scaleAngle = 180.0f / m_scaleCount;
    m_scaleX = (f32)(Position.l - m_CurrPos.l) / 2.0f;
    m_totalX = 0.0f;

    // set starting position
    SetPosition(m_CurrPos);
    m_pManager->SetScreenScaling(TRUE);

    // play scaling sound
    if( m_pManager->IsScreenOn() )
    {
        if( m_scaleX > 0 )
        {
            g_AudioMgr.Play( "ResizeLarge" ); 
        }
        else
        {
            g_AudioMgr.Play( "ResizeSmall" );
        }
    }
    else
    {
        if( m_scaleX > 0 )
        {
            g_AudioMgr.Play( "Bars_Out" ); 
        }
        else
        {
            g_AudioMgr.Play( "Bars_In" );
        }
    }
}

//=========================================================================

xbool ui_dialog::UpdateScreenScaling( f32 DeltaTime, xbool DoWipe )
{
    // scale window if necessary
    if (m_scaleCount)
    {
        // apply delta time
        m_scaleCount -= DeltaTime;

        if (m_scaleCount <= 0)
        {
            // last one - make sure window is correct size
            m_scaleCount = 0;
            m_CurrPos = m_RequestedPos;

            // resize the window
            SetPosition(m_CurrPos);        
            m_pManager->SetScreenSize(m_CurrPos);
            m_pManager->SetScreenScaling(FALSE);

            // start a screen wipe
            if ( DoWipe )
            {
                m_pManager->InitScreenWipe( this );
            }
        }
        else
        {
            m_totalX = m_scaleX + (m_scaleX * x_cos( DEG_TO_RAD( m_scaleAngle * m_scaleCount ) ) );
            m_CurrPos.l = m_StartPos.l + (s32)m_totalX;
            m_CurrPos.r = m_StartPos.r - (s32)m_totalX;

            // resize the window
            SetPosition(m_CurrPos);        
            m_pManager->SetScreenSize(m_CurrPos);

            // still more to do!
            return TRUE;
        }
    }

    // we're done!
    return FALSE;
}

//=========================================================================
//=========================================================================


//=========================================================================

ui_manager::dialog_tem* ui_dialog::GetTemplate( void )
{
    return m_pDialogTem;
}

//=========================================================================
