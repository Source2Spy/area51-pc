//=========================================================================
//
//  ui_dlg_vkeyboard.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"

#include "ui_dlg_vkeyboard.hpp"
#include "ui_manager.hpp"
#include "ui_control.hpp"
#include "ui_font.hpp"
#include "ui_frame.hpp"

//=========================================================================
//  Defines
//=========================================================================

#define KEYW    14
#define KEYH    18
#define NCOLS   17
#define NROWS   5

static const s32 VKEYBOARD_WIDTH           = 270;
static const s32 VKEYBOARD_GAMEPAD_HEIGHT  = 188;
static const s32 VKEYBOARD_KEYBOARD_HEIGHT = 62;

static u8 Keys[17*5] =
{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '%', '$', '^', '#', '/', ':', '\'',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', '@', '(', '+', ')',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '?', '<', '=', '>', 
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', '~', '[', '-', ']',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '.', '{', '|', '}',
};

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

//=========================================================================
//  Virtual Keyboard Dialog
//=========================================================================

enum controls
{
    IDC_CANCEL,
    IDC_ACCEPT,
    IDC_FRAME
};

ui_manager::control_tem vkeyboardControls[] =
{
//  { IDC_CANCEL,   "IDS_CANCEL", "button",     9, 162+22, 100,  24, 0, 8, 9, 1, ui_win::WF_VISIBLE },
//  { IDC_ACCEPT,   "IDS_OK",     "button",   162, 162+22, 100,  24, 9, 8, 8, 1, ui_win::WF_VISIBLE },

    { IDC_FRAME,    "IDS_NULL",   "frame",      8,  36+22, 254, 122, 0, 0, 0, 0, ui_win::WF_VISIBLE|ui_win::WF_STATIC },
};

ui_manager::dialog_tem vkeyboardDialog =
{
    "IDS_VIRTUAL_KEYBOARD_TITLE",
    17, 9,
    sizeof(vkeyboardControls)/sizeof(ui_manager::control_tem),
    &vkeyboardControls[0],
    0
};

//=========================================================================
//  vkey Window
//=========================================================================

class ui_vkey : public ui_control
{
public:
    virtual void    Render              ( s32 ox=0, s32 oy=0 );
    virtual void    OnAccept         ( ui_win* pWin );
    virtual void    OnPointerDown       ( ui_win* pWin, s32 x, s32 y );
};

//=========================================================================

void ui_vkey::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        xcolor  Color       = XCOLOR_BLACK;
        xcolor  TextColor1  = XCOLOR_WHITE;
        xcolor  TextColor2  = XCOLOR_BLACK;

        // Calculate rectangle
        irect    r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        xwstring Label;
        Label = m_Label;
        xwchar Char = Label[0];

        switch( Char )
        {
        case '\010':
            Label = g_StringTableMgr( "ui", "IDS_DELETE" );
            break;
        case '\020':
            Label = g_StringTableMgr( "ui", "IDS_SPACE" );
            break;
        case '\030':
            Label = g_StringTableMgr( "ui", "IDS_CANCEL" );
            break;
        case '\040':
            Label = g_StringTableMgr( "ui", "IDS_OK" );
            break;
        }

        // Render appropriate state
        if( m_Flags & WF_DISABLED )
        {
            Color      = xcolor(0,0,0,0);
            TextColor1 = XCOLOR_GREY;
            TextColor2 = xcolor(0,0,0,0);
        }
        else if( ShouldRenderHighlight() || IsActive() )
        {
            s32 alpha = 128 + (m_pManager->GetHighlightAlpha(8) * 8); // 128<->192
            Color      = xcolor(79,214,60,alpha); //xcolor( 121, 199, 213 );
            TextColor1 = xcolor(0,0,0,255);
            TextColor2 = xcolor(0,0,0,0);

        }
        else
        {
            Color      = xcolor(0,0,0,0);
            TextColor1 = xcolor(255,252,204,255); //XCOLOR_WHITE;
            TextColor2 = XCOLOR_BLACK;
        }

        r.Inflate( 1, 1 );
        m_pManager->RenderRect( r, Color, FALSE );

        r.Translate(  2, -2 );
        m_pManager->RenderText( 1, r, ui_font::h_center|ui_font::v_center, TextColor2, Label );
        r.Translate( -1, -1 );
        m_pManager->RenderText( 1, r, ui_font::h_center|ui_font::v_center, TextColor1, Label );
    }
}

//=========================================================================

void ui_vkey::OnAccept( ui_win* pWin )
{
    if( m_pManager->GetInputDevice( m_UserID ) == ui_input_device::Gamepad )
    {
        Notify( ui_notification_type::TextInput, m_Label );
        g_AudioMgr.Play( "Select_VKB" );
    }
    else
    {
        ui_control::OnAccept( pWin );
    }
}

//=========================================================================

void ui_vkey::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    (void)x;
    (void)y;

    if( m_pManager->GetInputDevice( m_UserID ) == ui_input_device::Gamepad )
    {
        Notify( ui_notification_type::TextInput, m_Label );
        g_AudioMgr.Play( "Select_VKB" );
    }
}

//=========================================================================

//=========================================================================
//  vkString Window
//=========================================================================

class ui_vkString : public ui_control
{
public:
    xbool           Create              ( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );
    virtual void    Render              ( s32 ox=0, s32 oy=0 );
    virtual void    OnNavigate       ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void    OnPage       ( ui_win* pWin, s32 Direction );
    virtual void    OnDelete         ( ui_win* pWin );

    void            Backspace           ( void );
    void            Character           ( const xwstring& String );
    s32             GetCursorPos        ( void );
    void            SetCursorPos        ( s32 Pos );

protected:
    s32             m_iElement;
    s32             m_Cursor;
};

//=========================================================================

xbool ui_vkString::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Initialize Data
    m_iElement = m_pManager->FindElement( "frame2" );
    ASSERT( m_iElement != -1 );
    
    m_Cursor = 0;

    return Success;
}

//=========================================================================

static const f32 s_CursorBlinkPeriod = 0.5f;
static       f32 s_CursorBlinkTime   = 0.0f;

void ui_vkString::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Render placeholder rectangle
        xcolor  Color       = XCOLOR_BLACK;
        xcolor  TextColor1  = xcolor(255,252,204,255); //XCOLOR_WHITE;
        xcolor  TextColor2  = XCOLOR_BLACK;

        // Calculate rectangle
        irect    r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        // Render appropriate state
        if( m_Flags & WF_DISABLED )
        {
            Color      = xcolor(0,0,0,0);
            TextColor1 = XCOLOR_GREY;
            TextColor2 = xcolor(0,0,0,0);
        }
        else if( ShouldRenderHighlight() || IsActive() )
        {
            Color      = xcolor (39,117,28,128);//xcolor( 121, 199, 213 );
            TextColor1 = XCOLOR_BLACK;
            TextColor2 = xcolor(255,252,204,255); //XCOLOR_WHITE;
        }
        else
        {
            Color      = xcolor (39,117,28,128); //xcolor(25,79,103,255);
            TextColor1 = xcolor(255,252,204,255); //XCOLOR_WHITE;
            TextColor2 = XCOLOR_BLACK;
        }

        // Render color rectangle and frame
        irect   rb = r;
        rb.Deflate( 1, 1 );
        m_pManager->RenderRect( rb, Color, FALSE );
        m_pManager->RenderElement( m_iElement, r, 0 );

        // Set a clip window to render the text
        rb.Deflate( 4, 1 );

        // render highlight if selected
        if( ShouldRenderHighlight() || IsActive() )
        {
            s32 alpha = 128 + (m_pManager->GetHighlightAlpha(8) * 8); // 64<->192
            m_pManager->RenderRect( rb, xcolor(79,214,60,alpha), FALSE );
        }

        m_pManager->PushClipWindow( rb );

        // Render Text
        irect rt = rb;
        rt.l += 1;
        rt.r -= 3;
        rt.Translate(  3, -2 );
        m_pManager->RenderText( 1, rt, ui_font::h_left|ui_font::v_center|ui_font::clip_r_justify, TextColor2, m_Label );
        rt.Translate( -1, -1 );
        m_pManager->RenderText( 1, rt, ui_font::h_left|ui_font::v_center|ui_font::clip_r_justify, TextColor1, m_Label );

        // Render Cursor
        irect Rect;
        m_pManager->TextSize( 1, Rect, m_Label, m_Cursor );
        rb.l = rt.l + Rect.r - 1;
        rb.r = rb.l + 1;
        rb.Deflate( 0, 2 );

        if( s_CursorBlinkTime < (s_CursorBlinkPeriod * 0.5f) )
            m_pManager->RenderRect( rb, TextColor1, TRUE );

        // Clear the clip window
        m_pManager->PopClipWindow();
    }
}

//=========================================================================

void ui_vkString::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    ui_dlg_vkeyboard* pKeyboard = (ui_dlg_vkeyboard*)m_pParent;
    ASSERT( pKeyboard );

    if( (m_pManager->GetInputDevice( m_UserID ) == ui_input_device::Gamepad) &&
        !pKeyboard->IsGamepadLayout() )
    {
        ui_control::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
        return;
    }

    // Which way are we moving
    switch( Code )
    {
    case ui_navigation::Left:
        if( m_Cursor > 0 )
        {
            m_Cursor--;
            s_CursorBlinkTime = 0.0f;
            g_AudioMgr.Play( "Cursor_VKB" );
        }
        else
        {
            g_AudioMgr.Play( "InvalidEntry" );
        }
        break;
    case ui_navigation::Right:
        if( m_Cursor < m_Label.GetLength() )
        {
            m_Cursor++;
            s_CursorBlinkTime = 0.0f;
            g_AudioMgr.Play( "Cursor_VKB" );
        }
        else
        {
            g_AudioMgr.Play( "InvalidEntry" );
        }
        break;
    case ui_navigation::Up:
        ui_control::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
        g_AudioMgr.Play( "InvalidEntry" );
        break;
    default:
        ui_control::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
        break;
    }
}

//=========================================================================

void ui_vkString::OnPage( ui_win* pWin, s32 Direction )
{
    // Which way are we moving
    switch( Direction )
    {
    case -1:
        if( m_Cursor > 0 )
        {
            m_Cursor--;
            s_CursorBlinkTime = 0.0f;
            g_AudioMgr.Play( "Cursor_VKB" );
        }
        else
        {
            g_AudioMgr.Play( "InvalidEntry" );
        }
        break;
    case 1:
        if( m_Cursor < m_Label.GetLength() )
        {
            m_Cursor++;
            s_CursorBlinkTime = 0.0f;
            g_AudioMgr.Play( "Cursor_VKB" );
        }
        else
        {
            g_AudioMgr.Play( "InvalidEntry" );
        }
        break;
    default:
        ui_control::OnPage( pWin, Direction );
    }
}

//=========================================================================

void ui_vkString::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_Cursor > 0 )
    {
        Backspace();
        Notify( ui_notification_type::TextRefresh );
        g_AudioMgr.Play( "Delete_VKB" );
    }
    else
    {
        g_AudioMgr.Play( "InvalidEntry" );
    }
}

//=========================================================================

void ui_vkString::Backspace( void )
{
    ASSERT( m_Cursor > 0 );

    xstring NewString;
    NewString = m_Label.Left( m_Cursor-1 );
    NewString += m_Label.Right( m_Label.GetLength()-m_Cursor );
    m_Label = NewString;
    m_Cursor--;
    s_CursorBlinkTime = 0.0f;
}

void ui_vkString::Character( const xwstring& String )
{
    xwstring NewString;
    NewString = m_Label.Left( m_Cursor );
    NewString += String;
    NewString += m_Label.Right( m_Label.GetLength()-m_Cursor );
    m_Label = NewString;
    m_Cursor++;
    s_CursorBlinkTime = 0.0f;
}

s32 ui_vkString::GetCursorPos( void )
{
    return m_Cursor;
}

void ui_vkString::SetCursorPos( s32 Pos )
{
    if( Pos < 0 )
        Pos = 0;
    if( Pos > m_Label.GetLength() )
        Pos = m_Label.GetLength();
    m_Cursor = Pos;
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* ui_dlg_vkeyboard_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    ui_dlg_vkeyboard* pDialog = new ui_dlg_vkeyboard;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  ui_dlg_vkeyboard
//=========================================================================

ui_dlg_vkeyboard::ui_dlg_vkeyboard( void )
{
    m_pResultDone = NULL;
    m_pResultOk   = NULL;
    m_bName       = TRUE;
    m_pPopUp      = NULL;
    m_pGamepadDefault = NULL;
    m_LayoutDevice    = ui_input_device::None;
    m_LayoutCenterX   = 0;
    m_LayoutCenterY   = 0;
    m_RepeatKeyIdx   = -1;
    m_KeyRepeatTimer = 0.0f;
}

//=========================================================================

ui_dlg_vkeyboard::~ui_dlg_vkeyboard( void )
{
}

//=========================================================================

xbool ui_dlg_vkeyboard::Create( s32                        UserID,
                                ui_manager*                pManager,
                                ui_manager::dialog_tem*    pDialogTem,
                                const irect&               Position,
                                ui_win*                    pParent,
                                s32                        Flags,
                                void*                      pUserData )
{
    xbool   Success = FALSE;
    s32     x,y;


    (void)pDialogTem;
    (void)pUserData;

    ASSERT( pManager );

    if( pParent )
    {
        m_LayoutCenterX = pParent->GetWidth () / 2;
        m_LayoutCenterY = pParent->GetHeight() / 2;
    }
    else
    {
        m_LayoutCenterX = Position.l + Position.GetWidth () / 2;
        m_LayoutCenterY = Position.t + Position.GetHeight() / 2;
    }

    // Set the initial window size. ApplyInputLayout selects the final height
    // once all controls exist.
    irect r( m_LayoutCenterX - VKEYBOARD_WIDTH / 2,
             m_LayoutCenterY - VKEYBOARD_GAMEPAD_HEIGHT / 2,
             m_LayoutCenterX + VKEYBOARD_WIDTH / 2,
             m_LayoutCenterY + VKEYBOARD_GAMEPAD_HEIGHT / 2 );

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, &vkeyboardDialog, r, pParent, Flags );
    // Setup Frame
    ((ui_frame*)m_Children[0])->SetBackgroundColor( xcolor (39,117,28,128) ); //xcolor(25,79,103,255) );
    ((ui_frame*)m_Children[0])->ChangeElement("frame2");

    // Add custom controls to Dialog
    {
        s32 i = 0;
        for( y=0 ; y<NROWS ; y++ )
        {
            for( x=0 ; x<NCOLS ; x++ )
            {
                // Create each control
                irect Pos;
                Pos.Set( 16+x*KEYW, 22+42+y*KEYH, 16+x*KEYW+KEYW, 22+42+y*KEYH+KEYH );

                ui_vkey* pVKey = new ui_vkey;
                ASSERT( pVKey );
                pVKey->Create( m_UserID, m_pManager, Pos, this, ui_win::WF_VISIBLE );
                if( m_pGamepadDefault == NULL )
                    m_pGamepadDefault = pVKey;

                // Configure the control
                pVKey->SetLabel( xwstring(xfs("%c",Keys[i])) );
                pVKey->SetNavPos( irect( x, y+1, x+1, y+2 ) );
                m_NavGraph[x+(y+1)*NCOLS] = pVKey;
                i++;
            }
        }
    }

    // These are the standard button widths that were in the code previously.
    // I guess these were based from english text.
    s32 ui_button_width[4] = { 58, 58, 58, 58 };

    // Override the button widths for foreign languages.
    // Make sure the total width remains at 232 (58*4)
    switch( x_GetLocale() )
    {
    case XL_LANG_FRENCH:
        ui_button_width[0] = 72;
        ui_button_width[1] = 56;
        ui_button_width[3] = 46;
        break;
    case XL_LANG_SPANISH:
        ui_button_width[2] = 66;
        ui_button_width[3] = 50;
        break;
    case XL_LANG_GERMAN:
        ui_button_width[0] = 58;
        ui_button_width[1] = 70;
        ui_button_width[2] = 74;
        ui_button_width[3] = 30;
        break;
    }
        
    // Add backspace
    {
        s32 x=0;
        s32 y=5;
        irect Pos;
        Pos.Set( 16, 22+4+42+y*KEYH, 16+ui_button_width[0], 22+4+42+y*KEYH+KEYH );

        ui_vkey* pVKey = new ui_vkey;
        ASSERT( pVKey );
        pVKey->Create( m_UserID, m_pManager, Pos, this, ui_win::WF_VISIBLE );
        pVKey->SetLabel( "\010" );
        pVKey->SetNavPos( irect( x, y+1, x+4, y+2 ) );
        for( x=0 ; x<4 ; x++ )
            m_NavGraph[x+(y+1)*NCOLS] = pVKey;
    }

    // Add space
    {
        s32 x=4;
        s32 y=5;
        irect Pos;
        Pos.Set( 16+ui_button_width[0]+2, 22+4+42+y*KEYH, 16+ui_button_width[0]+2+ui_button_width[1], 22+4+42+y*KEYH+KEYH );

        ui_vkey* pVKey = new ui_vkey;
        ASSERT( pVKey );
        pVKey->Create( m_UserID, m_pManager, Pos, this, ui_win::WF_VISIBLE );
        pVKey->SetLabel( "\020" );
        pVKey->SetNavPos( irect( x, y+1, x+4, y+2 ) );
        for( x=4 ; x<8 ; x++ )
            m_NavGraph[x+(y+1)*NCOLS] = pVKey;
    }

    // add cancel
    {
        s32 x=8;
        s32 y=5;
        irect Pos;
        Pos.Set( 16+ui_button_width[0]+2+ui_button_width[1]+2, 22+4+42+y*KEYH, 16+ui_button_width[0]+2+ui_button_width[1]+2+ui_button_width[2], 22+4+42+y*KEYH+KEYH );

        ui_vkey* pVKey = new ui_vkey;
        ASSERT( pVKey );
        pVKey->Create( m_UserID, m_pManager, Pos, this, ui_win::WF_VISIBLE );
        pVKey->SetLabel( "\030" );
        pVKey->SetNavPos( irect( x, y+1, x+4, y+2 ) );
        for( x=8 ; x<12 ; x++ )
            m_NavGraph[x+(y+1)*NCOLS] = pVKey;
    }

    // add OK
    {
        s32 x=12;
        s32 y=5;
        irect Pos;
        Pos.Set( 16+ui_button_width[0]+2+ui_button_width[1]+2+ui_button_width[2]+2, 22+4+42+y*KEYH, 16+ui_button_width[0]+2+ui_button_width[1]+2+ui_button_width[2]+2+ui_button_width[3], 22+4+42+y*KEYH+KEYH );

        ui_vkey* pVKey = new ui_vkey;
        ASSERT( pVKey );
        pVKey->Create( m_UserID, m_pManager, Pos, this, ui_win::WF_VISIBLE );
        pVKey->SetLabel( "\040" );
        pVKey->SetNavPos( irect( x, y+1, x+5, y+2 ) );
        for( x=12 ; x<17 ; x++ )
            m_NavGraph[x+(y+1)*NCOLS] = pVKey;
    }

    // Add String control to dialog
    m_pStringCtrl = new ui_vkString;
    ASSERT( m_pStringCtrl );
    irect Pos( 8, 22+8, 8+254, 22+8+24 );

    m_pStringCtrl->Create( m_UserID, m_pManager, Pos, this, ui_win::WF_VISIBLE );
    for( x=0 ; x<m_NavW ; x++ )
        m_NavGraph[x] = m_pStringCtrl;

    // Initialize Data
    m_iElement = m_pManager->FindElement( "frame2" );
    ASSERT( m_iElement != -1 );
    m_BackgroundColor   = xcolor (19,59,14,255); //xcolor(0, 20, 30,255);//FECOL_DIALOG2; //-- Jhowa
    m_pString       = NULL;
    m_MaxCharacters = -1;

    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    // play create sound
    g_AudioMgr.Play( "Select_VKB" );

    // Return success code
    return Success;
}

//=========================================================================

void ui_dlg_vkeyboard::ApplyInputLayout( ui_input_device Device )
{
    const ui_input_device LayoutDevice = (Device == ui_input_device::Gamepad)
                                       ? ui_input_device::Gamepad
                                       : ui_input_device::Keyboard;

    if( LayoutDevice == m_LayoutDevice )
        return;

    m_LayoutDevice = LayoutDevice;

    const xbool ShowGamepad = (m_LayoutDevice == ui_input_device::Gamepad);
    const s32 Height = ShowGamepad ? VKEYBOARD_GAMEPAD_HEIGHT
                                   : VKEYBOARD_KEYBOARD_HEIGHT;

    SetPosition( irect( m_LayoutCenterX - VKEYBOARD_WIDTH / 2,
                        m_LayoutCenterY - Height / 2,
                        m_LayoutCenterX + VKEYBOARD_WIDTH / 2,
                        m_LayoutCenterY + Height / 2 ) );

    for( s32 i = 0; i < m_Children.GetCount(); i++ )
    {
        ui_win* pChild = m_Children[i];
        pChild->SetFlag( WF_VISIBLE, (pChild == m_pStringCtrl) || ShowGamepad );
    }

    if( ShowGamepad )
        GotoControl( m_pGamepadDefault );
    else
        GotoControl( m_pStringCtrl );
}

//=========================================================================

void ui_dlg_vkeyboard::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Get window rectangle
        irect   r;
        r.Set( m_Position.l+ox, m_Position.t+oy, m_Position.r+ox, m_Position.b+oy );

        // Render background color
        if( m_BackgroundColor.A > 0 )
        {
            irect   rb = r;
            rb.Deflate( 1, 1 );
            m_pManager->RenderRect( rb, m_BackgroundColor, FALSE );
        }

        // Render Title
        irect rect = r;
        rect.Deflate( 1, 1 );
        rect.SetHeight( 22 );
        xcolor c1 = xcolor (25,77,18,255); //xcolor(20,30,40,224);//FECOL_TITLE1; //-- Jhowa
        xcolor c2 = xcolor (25,77,18,255); //xcolor(20,30,40,224);//FECOL_TITLE2; //-- Jhowa
        m_pManager->RenderGouraudRect( rect, c1, c1, c2, c2, FALSE );

        rect.Deflate( 8, 0 );
        rect.Translate( 1, -1 );
        m_pManager->RenderText( 0, rect, ui_font::h_left|ui_font::v_center, xcolor(XCOLOR_BLACK), m_Label );
        rect.Translate( -1, -1 );
        m_pManager->RenderText( 0, rect, ui_font::h_left|ui_font::v_center, xcolor(255,252,204,255), m_Label );

        // Render frame
        m_pManager->RenderElement( m_iElement, r, 0 );

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//=========================================================================
//-------------------------------------------------------------------------
// Physical keyboard key map
//-------------------------------------------------------------------------

struct pc_key_entry
{
    input_gadget    Key;
    char            Normal;
    char            Shifted;
};

//-------------------------------------------------------------------------

static const pc_key_entry s_PCKeyMap[] =
{
    // Letters
    { INPUT_KBD_A, 'a', 'A' }, { INPUT_KBD_B, 'b', 'B' }, { INPUT_KBD_C, 'c', 'C' },
    { INPUT_KBD_D, 'd', 'D' }, { INPUT_KBD_E, 'e', 'E' }, { INPUT_KBD_F, 'f', 'F' },
    { INPUT_KBD_G, 'g', 'G' }, { INPUT_KBD_H, 'h', 'H' }, { INPUT_KBD_I, 'i', 'I' },
    { INPUT_KBD_J, 'j', 'J' }, { INPUT_KBD_K, 'k', 'K' }, { INPUT_KBD_L, 'l', 'L' },
    { INPUT_KBD_M, 'm', 'M' }, { INPUT_KBD_N, 'n', 'N' }, { INPUT_KBD_O, 'o', 'O' },
    { INPUT_KBD_P, 'p', 'P' }, { INPUT_KBD_Q, 'q', 'Q' }, { INPUT_KBD_R, 'r', 'R' },
    { INPUT_KBD_S, 's', 'S' }, { INPUT_KBD_T, 't', 'T' }, { INPUT_KBD_U, 'u', 'U' },
    { INPUT_KBD_V, 'v', 'V' }, { INPUT_KBD_W, 'w', 'W' }, { INPUT_KBD_X, 'x', 'X' },
    { INPUT_KBD_Y, 'y', 'Y' }, { INPUT_KBD_Z, 'z', 'Z' },
    // Digits
    { INPUT_KBD_0, '0', ')' }, { INPUT_KBD_1, '1', '!' }, { INPUT_KBD_2, '2', '@' },
    { INPUT_KBD_3, '3', '#' }, { INPUT_KBD_4, '4', '$' }, { INPUT_KBD_5, '5', '%' },
    { INPUT_KBD_6, '6', '^' }, { INPUT_KBD_7, '7', '&' }, { INPUT_KBD_8, '8', '*' },
    { INPUT_KBD_9, '9', '(' },
    // Common symbols
    { INPUT_KBD_SPACE,      ' ',  ' '  },
    { INPUT_KBD_MINUS,      '-',  '_'  },
    { INPUT_KBD_EQUALS,     '=',  '+'  },
    { INPUT_KBD_LBRACKET,   '[',  '{'  },
    { INPUT_KBD_RBRACKET,   ']',  '}'  },
    { INPUT_KBD_SEMICOLON,  ';',  ':'  },
    { INPUT_KBD_APOSTROPHE, '\'', '"'  },
    { INPUT_KBD_COMMA,      ',',  '<'  },
    { INPUT_KBD_PERIOD,     '.',  '>'  },
    { INPUT_KBD_SLASH,      '/',  '?'  },
    { INPUT_KBD_BACKSLASH,  '\\', '|'  },
};

//-------------------------------------------------------------------------

static const s32 s_PCKeyMapCount = sizeof(s_PCKeyMap) / sizeof(s_PCKeyMap[0]);

//=========================================================================

void ui_dlg_vkeyboard::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    if( DeltaTime > 0.0f )
    {
        s_CursorBlinkTime += DeltaTime;
        if( s_CursorBlinkTime >= s_CursorBlinkPeriod )
            s_CursorBlinkTime = x_fmod( s_CursorBlinkTime, s_CursorBlinkPeriod );
    }

    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    if( m_pPopUp )
    {
        if( m_PopUpResult != DLG_POPUP_IDLE )
        {
            m_pPopUp = NULL;
        }
    }

    // Physical text entry belongs to the compact keyboard layout.
    if( (m_LayoutDevice == ui_input_device::Keyboard) && !m_pPopUp )
    {
        // Determine which character key is held or was pressed this frame.
        // Backspace repeat is handled by ui_manager through the Delete action.
        s32 activeKey = -1;
        for( s32 k = 0; k < s_PCKeyMapCount; k++ )
        {
            if( g_Input.GetFrameSnapshot().IsPressed( s_PCKeyMap[k].Key ) ||
                g_Input.GetFrameSnapshot().WasPressed( s_PCKeyMap[k].Key ) )
            {
                activeKey = k;
                break;
            }
        }

        // Hold-to-repeat for character keys
        xbool bFire = FALSE;
        if( activeKey >= 0 )
        {
            if( m_RepeatKeyIdx != activeKey )
            {
                m_RepeatKeyIdx   = activeKey;
                m_KeyRepeatTimer = 0.4f;
                bFire = TRUE;
            }
            else
            {
                m_KeyRepeatTimer -= DeltaTime;
                if( m_KeyRepeatTimer <= 0.0f )
                {
                    m_KeyRepeatTimer += 0.05f;
                    bFire = TRUE;
                }
            }
        }
        else
        {
            m_RepeatKeyIdx = -1;
        }

        if( bFire )
        {
            xbool bShift = g_Input.GetFrameSnapshot().IsPressed( INPUT_KBD_LSHIFT ) ||
                           g_Input.GetFrameSnapshot().WasPressed( INPUT_KBD_LSHIFT ) ||
                           g_Input.GetFrameSnapshot().IsPressed( INPUT_KBD_RSHIFT ) ||
                           g_Input.GetFrameSnapshot().WasPressed( INPUT_KBD_RSHIFT );
            char c = bShift ? s_PCKeyMap[activeKey].Shifted : s_PCKeyMap[activeKey].Normal;
            if( c && ( (m_MaxCharacters == -1) ||
                        (m_pStringCtrl->GetLabel().GetLength() < m_MaxCharacters) ) )
            {
                m_pStringCtrl->Character( xwstring( xfs("%c", c) ) );
                if( m_pString )
                    *m_pString = m_pStringCtrl->GetLabel();
                g_AudioMgr.Play( "Select_VKB" );
            }
            else if( c )
            {
                g_AudioMgr.Play( "InvalidEntry" );
            }
        }
    }
}

//=========================================================================

void ui_dlg_vkeyboard::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    ApplyInputLayout( m_pManager->GetInputDevice( m_UserID ) );

    if( m_LayoutDevice == ui_input_device::Keyboard )
    {
        if( (Code == ui_navigation::Left) || (Code == ui_navigation::Right) )
            m_pStringCtrl->OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
        return;
    }

    (void)WrapX;
    (void)WrapY;
    ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, TRUE, FALSE );
    g_AudioMgr.Play( "Cursor_VKB" );
}

//=========================================================================

void ui_dlg_vkeyboard::OnPage( ui_win* pWin, s32 Direction )
{
    m_pStringCtrl->OnPage( pWin, Direction );
}

//=========================================================================

void ui_dlg_vkeyboard::OnDelete( ui_win* pWin )
{
    if( m_pPopUp )
        return;

    m_pStringCtrl->OnDelete( pWin );
}

//=========================================================================

void ui_dlg_vkeyboard::OnAccept( ui_win* pWin )
{
    (void)pWin;

    // Keyboard Enter confirms the string. Gamepad Accept is handled by the
    // focused on-screen key instead.
    if( m_pManager->GetInputDevice( m_UserID ) == ui_input_device::Keyboard )
    {
        if( m_pPopUp )
            return;

        xwstring okLabel( "\040" );
        ui_notification const Event = { ui_notification_type::TextInput, this, 0, &okLabel };
        OnNotify( Event );
    }
}

//=========================================================================

void ui_dlg_vkeyboard::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_pPopUp )
        return;

    // Undo changes
    if( m_pString )
        *m_pString = m_BackupString;

    if( m_pResultDone )
        *m_pResultDone = TRUE;

    // Close dialog
    if ( m_Flags & WF_INPUTMODAL )
    {
        m_pManager->EndDialog( m_UserID, TRUE );
    }

    g_AudioMgr.Play( "Backup" );
}

//=========================================================================

s32 ui_dlg_vkeyboard::IsValid( const xwstring* pString, xbool bIsName )
{
    if( bIsName )
    {
        s32 Length = pString->GetLength();                                

        //
        // Length Filter.
        //
        if( Length < 3 )
        {
            return 2;
        }

        // Convert it to lower case.
        xwstring PotentialName = *pString;
        for( s32 i = 0; i < Length; i++ )
        {
            if( IN_RANGE( (xwchar)'A', PotentialName.GetAt( i ), (xwchar)'Z' ) )
            {
                PotentialName.SetAt( i, PotentialName.GetAt( i ) - ('A' - 'a') );
            }
        }

        //
        // Obscenity Filter.
        //
        VERIFY( g_StringTableMgr.LoadTable( "Obscenities", "ENG_OBSCENITIES.string" ) );
        xarray<xwstring> Words;
        s32 nStrings = g_StringTableMgr.GetStringCount( "Obscenities" );

        for( s32 i = 0; i < nStrings; i++ )
        {
            const xwstring& Word = g_StringTableMgr( "Obscenities", i );
            if( (Word.GetLength() > 0) &&
                (x_wstrcmp( (const xwchar*)Word, (const xwchar*)xwstring( "<null>" ) ) != 0) )
            {
                Words.Append() = Word;
            }
        }

        s32 NumWords = Words.GetCount();

        g_StringTableMgr.UnloadTable( "Obscenities" );
        
        s32 NameCharacters[ NET_SERVER_NAME_LENGTH ];
        x_memset( NameCharacters, 0, sizeof(NameCharacters) );

        for( s32 i = 0; i < PotentialName.GetLength(); i++ )
        {
            if( i>= NET_SERVER_NAME_LENGTH-1 )
            {
                break;
            }
            if( IN_RANGE( (xwchar)'a', PotentialName.GetAt( i ), (xwchar)'z' ) )
            {
                NameCharacters[ i ] = 2;
            }    
            else if( IN_RANGE( (xwchar)'0', PotentialName.GetAt( i ), (xwchar)'9' ) )
            {
                NameCharacters[ i ] = 1;
            }
        }
        
        const xwchar* pNameStr = (const xwchar*)PotentialName;

        // This probably could be more efficient, but it already runs instantly, 
        // and it's only ever run when the user tries to accept his profile name.
        for( s32 i = 0; i < NumWords; i++ )
        {
            // Look for this particular obscene word.
            xwchar* pObscenePos;
            const xwchar* pLastPos = pNameStr;

            while( (pObscenePos = x_wstrstr( pLastPos, (const xwchar*)Words[ i ]) ) )
            {
                // If the word is sufficiently small it has a chance to redeem itself.
                if( Words[ i ].GetLength() < 4 )
                {
                    s32 Position = pObscenePos - pNameStr;
                    for( s32 j = Position; j < Position + Words[ i ].GetLength(); j++ )
                    {
                        // Mark the characters bad.
                        NameCharacters[ j ] = -1;
                    }

                    pLastPos = pObscenePos + 1;
                }

                // Just go ahead and throw anything with a four or more letter curse word out.
                else
                {
                    return 1;
                }
            }
        }

        xbool bHasAlphaNumeric    = FALSE;
        xbool bCurseRedeemer      = FALSE;
        xbool bInCurse            = FALSE;

        // Here is the code that makes the world safe
        // for assassins everywhere.
        for( s32 i = 0; i < NET_SERVER_NAME_LENGTH; i++ )
        {
            if( (NameCharacters[ i ] == 1) ||
                (NameCharacters[ i ] == 2) )
            {
                bHasAlphaNumeric = TRUE;
            }

            if( (NameCharacters[ i ] == 2) )
            {
                bCurseRedeemer = TRUE;
            }
            else if( NameCharacters[ i ] != -1 )
            {
                bCurseRedeemer = FALSE;
            }

            if( (NameCharacters[ i ] == -1) &&
                !bCurseRedeemer )
            {
                bInCurse = TRUE;
            }
            else if( bInCurse )
            {
                if( !bCurseRedeemer )
                {
                    // Looks like we've found an isolated curse word! Hooray!
                    return FALSE;
                }
                else
                {
                    bCurseRedeemer = FALSE;
                    bInCurse       = FALSE;
                }
            }
        }

        if( !bHasAlphaNumeric )
        {
            return 2;
        }
    }
    
    return 0;
}

//=========================================================================

void ui_dlg_vkeyboard::OnNotify( ui_notification const& Event )
{
    (void)Event.m_pSender;

    switch( Event.m_Type )
    {
    case ui_notification_type::TextInput:
        {
            ASSERT( Event.m_pText );

            xwstring const& Text = *Event.m_pText;
            if( Text.GetAt(0) == '\010' )
            {
                // backspace
                if( m_pStringCtrl->GetCursorPos() > 0 )
                {
                    m_pStringCtrl->Backspace();
                    g_AudioMgr.Play( "Delete_VKB" );
                }
                else
                {
                    g_AudioMgr.Play( "InvalidEntry" );
                }
            }
            else if( Text.GetAt(0) == '\020' )
            {
                // space
                if( (m_MaxCharacters == -1) || (m_pStringCtrl->GetLabel().GetLength() < m_MaxCharacters) )
                {
                    m_pStringCtrl->Character( xwstring(" ") );
                }
                else
                {
                    g_AudioMgr.Play( "InvalidEntry" );
                }
            }
            else if( Text.GetAt(0) == '\030' )
            {
                // cancel
                if( m_pResultOk )
                    *m_pResultOk = FALSE;
                OnCancel( Event.m_pSender );
                break;
            }
            else if( Text.GetAt(0) == '\040' )
            {
                // accept
                ASSERT( m_pString );
                s32 ValidCode = IsValid( m_pString, m_bName );
                if( ValidCode != 0 )
                {
                    g_AudioMgr.Play( "InvalidEntry" );

                    irect r = m_pManager->GetUserBounds( m_UserID );
                    m_pPopUp = (dlg_popup*)m_pManager->OpenDialog( m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

                    // set nav text
                    xwstring navText( g_StringTableMgr( "ui", "IDS_NAV_OK" ) );
                    const xwchar* pNavtext = (const xwchar*)navText;

                    if( ValidCode == 1 )
                    {
                        m_pPopUp->ConfigureMessage( g_StringTableMgr( "ui", "IDS_OBSCENITY_FILTER" ),
                                                    g_StringTableMgr( "ui", "IDS_OBSCENE_PROFILE_PS2" ),
                                                    &m_PopUpResult );
                    }
                    else
                    {
                        m_pPopUp->ConfigureMessage( g_StringTableMgr( "ui", "IDS_INVALID_NAME" ),
                                                    g_StringTableMgr( "ui", "IDS_INVALID_NAME_TEXT" ),
                                                    &m_PopUpResult );
                    }
                }
                else
                {
                    if( m_pResultOk )
                        *m_pResultOk   = TRUE;
                    if( m_pResultDone )
                        *m_pResultDone = TRUE;

                    if( m_Flags & WF_INPUTMODAL )
                        m_pManager->EndDialog( m_UserID, TRUE );

                    g_AudioMgr.Play( "Select_Norm" );
                    break;
                }
            }
            else
            {
                if( (m_MaxCharacters == -1) || (m_pStringCtrl->GetLabel().GetLength() < m_MaxCharacters) )
                {
                    m_pStringCtrl->Character( Text );
                }
                else
                {
                    g_AudioMgr.Play( "InvalidEntry" );
                }
            }

            // Update connected string
            if( m_pString )
                *m_pString = m_pStringCtrl->GetLabel();
        }
        break;
    case ui_notification_type::TextRefresh:
        {
            // Update connected string
            if( m_pString )
                *m_pString = m_pStringCtrl->GetLabel();
        }
    }
}

//=========================================================================

void ui_dlg_vkeyboard::ConnectString( xwstring* pString, s32 BufferSize )
{
    ASSERT( pString );

    // Initialize all strings and pointers
    m_pString = pString;
    m_BackupString = *pString;
    m_pStringCtrl->SetLabel( *pString );
    m_pStringCtrl->SetCursorPos( pString->GetLength() );
    m_MaxCharacters = BufferSize - 1;
}

//=========================================================================

void ui_dlg_vkeyboard::SetReturn( xbool* pDone, xbool* pOk )
{
    m_pResultDone = pDone;
    m_pResultOk   = pOk;
}

//=========================================================================
