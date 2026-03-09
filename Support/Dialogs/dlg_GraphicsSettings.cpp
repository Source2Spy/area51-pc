//=============================================================================
//
//  dlg_graphics_settings.cpp
//
//=============================================================================

#include "entropy.hpp"

#include "ui/ui_font.hpp"
#include "ui/ui_manager.hpp"
#include "ui/ui_control.hpp"
#include "ui/ui_combo.hpp"
#include "ui/ui_button.hpp"
#include "ui/ui_check.hpp"

#include "dlg_GraphicsSettings.hpp"
#include "stringmgr/stringmgr.hpp"
#include "AudioMgr/AudioMgr.hpp"

//=============================================================================
//  Main Options Dialog
//=============================================================================

enum graphics_controls
{
    IDC_GRAPHICS_RESOLUTION_TEXT,
    IDC_GRAPHICS_DISPLAY_MODE_TEXT,
    IDC_GRAPHICS_VSYNC_TEXT,
    IDC_GRAPHICS_GAMMA_TEXT,
    IDC_GRAPHICS_FOV_TEXT,

    IDC_GRAPHICS_RESOLUTION,
    IDC_GRAPHICS_DISPLAY_MODE,
    IDC_GRAPHICS_VSYNC,
    IDC_GRAPHICS_GAMMA,
    IDC_GRAPHICS_FOV,
    IDC_GRAPHICS_BUTTON_APPLY,

    IDC_GRAPHICS_NAV_TEXT,
};

//-------------------------------------------------------------------------

ui_manager::control_tem GraphicsSettingsControls[] =
{
    { IDC_GRAPHICS_RESOLUTION_TEXT,      "IDS_NULL",                   "text",     40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_DISPLAY_MODE_TEXT,    "IDS_NULL",                   "text",     40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_VSYNC_TEXT,           "IDS_NULL",                   "text",     40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_GAMMA_TEXT,           "IDS_NULL",                   "text",     40, 145, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_FOV_TEXT,             "IDS_NULL",                   "text",     40, 180, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
													   	               	          
    { IDC_GRAPHICS_RESOLUTION,           "IDS_NULL",                   "combo",   290,  40, 140, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_DISPLAY_MODE,         "IDS_NULL",                   "combo",   290,  75, 140, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_VSYNC,                "IDS_NULL",                   "check",   290, 110, 140, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_GAMMA,                "IDS_NULL",                   "slider",  300, 145, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_GRAPHICS_FOV,                  "IDS_NULL",                   "slider",  300, 180, 120, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },

    { IDC_GRAPHICS_BUTTON_APPLY,         "IDS_PROFILE_OPTIONS_ACCEPT", "button",  40,  285, 220, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },

    { IDC_GRAPHICS_NAV_TEXT,             "IDS_NULL",                   "text",      0,   0,   0,  0, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
};

//-------------------------------------------------------------------------

ui_manager::dialog_tem GraphicsSettingsDialog =
{
    "IDS_OPTIONS_GRAPHICS_SETTINGS",
    1, 9,
    sizeof(GraphicsSettingsControls)/sizeof(ui_manager::control_tem),
    &GraphicsSettingsControls[0],
    0
};

//=============================================================================
//  Registration function
//=============================================================================

void dlg_graphics_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "graphics settings", &GraphicsSettingsDialog, &dlg_graphics_settings_factory );
}

//=============================================================================
//  Factory function
//=============================================================================

ui_win* dlg_graphics_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_graphics_settings* pDialog = new dlg_graphics_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=============================================================================
//  dlg_graphics_settings
//=============================================================================

dlg_graphics_settings::dlg_graphics_settings( void )
{
}

//=============================================================================

dlg_graphics_settings::~dlg_graphics_settings( void )
{
    Destroy();
}

//=============================================================================

xbool dlg_graphics_settings::Create( s32                        UserID,
                                     ui_manager*                pManager,
                                     ui_manager::dialog_tem*    pDialogTem,
                                     const irect&               Position,
                                     ui_win*                    pParent,
                                     s32                        Flags,
                                     void*                      pUserData )
{
    xbool   Success = FALSE;

    (void)pUserData;

    ASSERT( pManager );

    Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_pResolution           = (ui_combo*)   FindChildByID( IDC_GRAPHICS_RESOLUTION      );
    m_pDisplayMode          = (ui_combo*)   FindChildByID( IDC_GRAPHICS_DISPLAY_MODE    );
    m_pGamma                = (ui_slider*)  FindChildByID( IDC_GRAPHICS_GAMMA           );
    m_pFov                  = (ui_slider*)  FindChildByID( IDC_GRAPHICS_FOV             );
    m_pVSync                = (ui_check*)   FindChildByID( IDC_GRAPHICS_VSYNC           );
    m_pButtonApply          = (ui_button*)  FindChildByID( IDC_GRAPHICS_BUTTON_APPLY    );

    m_pResolutionText       = (ui_text*)    FindChildByID( IDC_GRAPHICS_RESOLUTION_TEXT     );
    m_pDisplayModeText      = (ui_text*)    FindChildByID( IDC_GRAPHICS_DISPLAY_MODE_TEXT   );
    m_pGammaText            = (ui_text*)    FindChildByID( IDC_GRAPHICS_GAMMA_TEXT         );
    m_pFovText              = (ui_text*)    FindChildByID( IDC_GRAPHICS_FOV_TEXT           );
    m_pVSyncText            = (ui_text*)    FindChildByID( IDC_GRAPHICS_VSYNC_TEXT         );
    m_pNavText              = (ui_text*)    FindChildByID( IDC_GRAPHICS_NAV_TEXT           );

    GotoControl( (ui_control*)m_pResolution );
    m_CurrentControl = IDC_GRAPHICS_RESOLUTION;
    m_CurrHL = 0;

    m_pGamma->SetRange( 0, 200 );
    m_pGamma->SetValue( 100 );
    m_pGamma->UseDefaultSound( FALSE );

    m_pFov->SetRange( 60, 120 );
    m_pFov->SetValue( 90 );
    m_pFov->UseDefaultSound( FALSE );

    m_pVSync->SetChecked( TRUE );

    m_pButtonApply   ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    m_pResolutionText ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pDisplayModeText->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pGammaText      ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pFovText        ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pVSyncText      ->SetLabelFlags( ui_font::h_left|ui_font::v_center );

    m_pNavText->SetLabelFlags( ui_font::h_center|ui_font::v_top|ui_font::is_help_text );
    m_pNavText->UseSmallText( TRUE );

    m_pResolution    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pDisplayMode   ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pGamma         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFov           ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVSync         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonApply   ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_pResolutionText ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pDisplayModeText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pGammaText      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFovText        ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVSyncText      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pNavText        ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_pResolution->DeleteAllItems();
    m_pDisplayMode->DeleteAllItems();

    m_pResolution->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_RESOLUTION_1080" ), 0 );
    m_pResolution->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_RESOLUTION_1440" ), 1 );
    m_pResolution->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_RESOLUTION_2160" ), 2 );
    m_pResolution->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_RESOLUTION_768" ), 3 );
    m_pResolution->SetSelection( 0 );

    m_pDisplayMode->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_MODE_FULLSCREEN" ), 0 );
    m_pDisplayMode->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_MODE_BORDERLESS" ), 1 );
    m_pDisplayMode->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_MODE_WINDOWED" ), 2 );
    m_pDisplayMode->SetSelection( 0 );

    m_pResolutionText ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_RESOLUTION" ) );
    m_pDisplayModeText->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_DISPLAY_MODE" ) );
    m_pVSyncText      ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_VERTICAL_SYNC" ) );
    m_pGammaText      ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_GAMMA" ) );
    m_pFovText        ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_FIELD_OF_VIEW" ) );

    xwstring navText( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) );
    navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    m_pNavText->SetLabel( navText );

    InitScreenScaling( Position );

    m_bRenderBlackout = FALSE;
    m_State = DIALOG_STATE_ACTIVE;

    return Success;
}

//=============================================================================

void dlg_graphics_settings::Destroy( void )
{
    ui_dialog::Destroy();

    g_UiMgr->ResetScreenWipe();
}

//=============================================================================

void dlg_graphics_settings::Render( s32 ox, s32 oy )
{
    static s32 offset   =  0;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;

    if( m_bRenderBlackout )
    {
        s32 XRes, YRes;
        eng_GetRes( XRes, YRes );
#ifdef TARGET_PS2
        rb.Set( -1, 0, XRes, YRes );
#else
        rb.Set( 0, 0, XRes, YRes );
#endif
        g_UiMgr->RenderGouraudRect( rb,
                                    xcolor( 0, 0, 0, 180 ),
                                    xcolor( 0, 0, 0, 180 ),
                                    xcolor( 0, 0, 0, 180 ),
                                    xcolor( 0, 0, 0, 180 ),
                                    FALSE );
    }

    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;

    g_UiMgr->RenderGouraudRect( rb,
                                xcolor( 56, 115, 58, 64 ),
                                xcolor( 56, 115, 58, 64 ),
                                xcolor( 56, 115, 58, 64 ),
                                xcolor( 56, 115, 58, 64 ),
                                FALSE );

    s32 y = rb.t + offset;

    while( y < rb.b )
    {
        irect bar;

        if( ( y + width ) > rb.b )
        {
            bar.Set( rb.l, y, rb.r, rb.b );
        }
        else
        {
            bar.Set( rb.l, y, rb.r, y + width );
        }

        g_UiMgr->RenderGouraudRect( bar,
                                    xcolor( 56, 115, 58, 30 ),
                                    xcolor( 56, 115, 58, 30 ),
                                    xcolor( 56, 115, 58, 30 ),
                                    xcolor( 56, 115, 58, 30 ),
                                    FALSE );

        y += gap;
    }

    ui_dialog::Render( ox, oy );

    g_UiMgr->RenderGlowBar();
}

//=============================================================================

void dlg_graphics_settings::OnNotify( ui_win* pWin, ui_win* pSender, s32 Command, void* pData )
{
    (void)pWin;
    (void)pSender;
    (void)Command;
    (void)pData;
}

//=============================================================================

void dlg_graphics_settings::OnPadSelect( ui_win* pWin )
{
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonApply )
        {
            g_AudioMgr.Play( "Select_Norm" );
            x_DebugMsg( "Graphics settings apply placeholder\n" );
            m_State = DIALOG_STATE_BACK;
        }
    }
}

//=============================================================================

void dlg_graphics_settings::OnPadBack( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
    }
}

//=============================================================================

void dlg_graphics_settings::OnPadDelete( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Select_Norm" );

        m_pResolution->SetSelection( 2 );
        m_pDisplayMode->SetSelection( 1 );
        m_pGamma->SetValue( 120 );
        m_pFov->SetValue( 100 );
        m_pVSync->SetChecked( FALSE );

        x_DebugMsg( "Graphics settings random preset placeholder\n" );
    }
}

//=============================================================================

void dlg_graphics_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    s32 highLight = -1;

    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            m_pResolution     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pDisplayMode    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pGamma          ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pFov            ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVSync          ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pButtonApply    ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            m_pResolutionText ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pDisplayModeText->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pGammaText      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pFovText        ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVSyncText      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pNavText        ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            GotoControl( (ui_control*)m_pResolution );
            m_pResolution->SetFlag( WF_HIGHLIGHT, TRUE );
            g_UiMgr->SetScreenHighlight( m_pResolutionText->GetPosition() );
        }
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    if( m_pResolution->GetFlags( WF_HIGHLIGHT ) )
    {
        highLight = 0;
        m_pResolutionText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pResolutionText->GetPosition() );
    }
    else
    {
        m_pResolutionText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pDisplayMode->GetFlags( WF_HIGHLIGHT ) )
    {
        highLight = 1;
        m_pDisplayModeText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pDisplayModeText->GetPosition() );
    }
    else
    {
        m_pDisplayModeText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pVSync->GetFlags( WF_HIGHLIGHT ) )
    {
        highLight = 2;
        m_pVSyncText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pVSyncText->GetPosition() );
    }
    else
    {
        m_pVSyncText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pGamma->GetFlags( WF_HIGHLIGHT ) )
    {
        highLight = 3;
        m_pGammaText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pGammaText->GetPosition() );
    }
    else
    {
        m_pGammaText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pFov->GetFlags( WF_HIGHLIGHT ) )
    {
        highLight = 4;
        m_pFovText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pFovText->GetPosition() );
    }
    else
    {
        m_pFovText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pButtonApply->GetFlags( WF_HIGHLIGHT ) )
    {
        highLight = 5;
        g_UiMgr->SetScreenHighlight( m_pButtonApply->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play( "Cusor_Norm" );

        m_CurrHL = highLight;
    }
}

//=============================================================================