//==============================================================================
//
//  dlg_Credits.cpp
//
//  Copyright (c) 2005 Midway West. All rights reserved.
//
//  define HERE
//
//==============================================================================

//==========================================================================
// INCLUDE
//=========================================================================
#include "Entropy.hpp"

#include "UI/ui_manager.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_bitmap.hpp"

#include "dlg_Credits.hpp"  
#include "StringMgr/StringMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "UI/ui_font.hpp"
#include "MoviePlayer/MoviePlayer.hpp"

#include "UI/ui_manager.hpp"

extern xstring SelectBestClip( const char* pName );

typedef struct _Sweep
{
    f32 StartTime,EndTime,StartValue,EndValue;
}Sweep;

#define SWEEP_FRAME_MAX     30.0f
#define SWEEP_FRAME_RATE    30.0f
#define FINAL_PIXEL_COLOR   140.0f
Sweep Sweeps[2] = 
{
    {5.0f,SWEEP_FRAME_MAX,     255.0f,FINAL_PIXEL_COLOR},
    {0.0f  ,5.0f            ,    0.0f,255.0f}
};

//=========================================================================
// DEFINES
//=========================================================================
#define DEFAULT_LINE_TIME       (8.0f)
#define FADE_BOUNDS_OFFSETS     (80)
#define FADE_MIN_START          (0.5f)
#define FADE_MAX_START          (2.5f)
//=========================================================================
// GLOBAL
//=========================================================================
enum controls
{
    IDC_CREDIT_STRING_1,
};

ui_manager::control_tem CreditsControls[] =
{
    { IDC_CREDIT_STRING_1, "IDS_PRESS_START_TEXT", "text",     0, 308, 480,  30, 0, 0, 1, 1, 0 },
};

ui_manager::dialog_tem CreditsDialog =
{
        "IDS_CREDITS",
        1, 9,
        sizeof(CreditsControls)/sizeof(ui_manager::control_tem),
        &CreditsControls[0],
        0
};

//=========================================================================
// FUNTIONS
//=========================================================================
//  Registration function
//=========================================================================

void dlg_credits_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "credits screen", &CreditsDialog, &dlg_credits_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_credits_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_credits* pDialog = new dlg_credits;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_credits
//=========================================================================

dlg_credits::dlg_credits( void )
{
}

//=========================================================================

dlg_credits::~dlg_credits( void )
{
}

//=========================================================================

xbool dlg_credits::Create( s32                        UserID,
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

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    // Load Credits String Table
    VERIFY( g_StringTableMgr.LoadTable( "credits", "ENG_CREDITS_STRINGS.string" ) );

    // init
    m_LineIndex = 0;
    m_PageLineCount = 0;
    m_CreditsDone = FALSE;
    InitCreditLines();
    
    // Build First Page
    BuildPage();

    // disable the highlight
    g_UiMgr->DisableScreenHighlight();

    // diable the background movie
    //DisableBackgoundMovie();

    // start up the start movie
    //g_StateMgr.PlayMovie( "MenuBackGround", TRUE, TRUE );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_credits::Destroy( void )
{
    // unload credits strings
    g_StringTableMgr.UnloadTable("credits");

    // enable the highlight
    g_UiMgr->EnableScreenHighlight();

    ui_dialog::Destroy();
}

//=========================================================================

void dlg_credits::Render( s32 ox, s32 oy )
{
    for( s32 index = 0 ; index < m_PageLineCount ; index ++)
    {
        g_UiMgr->GetFont(m_CreditLines[index].m_Font)->RenderStateControlledText(
                             m_CreditLines[index].m_Rect, 
                             m_CreditLines[index].m_RenderFlags,                            
                             m_CreditLines[index].m_Color, 
                             m_CreditLines[index].m_String,
                             m_CreditLines[index].m_CustomRenderStruct.GetPtr());
    }

    // finally render all the normal dialog stuff
    ui_dialog::Render( ox, oy );
}

//=========================================================================

void dlg_credits::OnAccept( ui_win* pWin )
{
    (void)pWin;
}

//=========================================================================

void dlg_credits::OnCancel( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        // Credits EXIT
        
        // close the credits background movie
        //g_StateMgr.CloseMovie();
        // renable the menu background
        //g_StateMgr.EnableBackgroundMovie();
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================


void dlg_credits::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    const f32 FadeOutRate = 480.0f;

    // Total Page Time
    m_CurrentPageTime += DeltaTime;
    
    if( m_CurrentPageTime >= m_PageTimer )
    {
        // if this page is done then fade the chars out.
        xbool TurnPage = TRUE;

        for( s32 index = 0 ; index < m_PageLineCount ; index ++)
        {       
            for( s32 i = 0 ; i < m_CreditLines[index].m_CharCount ; i ++ )
            {
                // Fade All Chars
                if( m_CreditLines[index].m_CustomRenderStruct[i].m_Value > 0.0f )
                {
                    m_CreditLines[index].m_CustomRenderStruct[i].m_Value -= (FadeOutRate * DeltaTime);
                    if( m_CreditLines[index].m_CustomRenderStruct[i].m_Value < 0.0f )
                        m_CreditLines[index].m_CustomRenderStruct[i].m_Value = 0.0f;

                    TurnPage = FALSE;
                }
            }
        }

        // Next page.
        if( TurnPage )
        { 
            if( BuildPage() ) 
            {
                // EXIT here ..
                m_State = DIALOG_STATE_BACK;

                // Use this for looping.
                //m_LineIndex = 0;
                //m_CreditsDone = FALSE;
            }
        }
    }
    else
    {
        // handle effects of chars poping in.

        for( s32 index = 0 ; index < m_PageLineCount ; index ++)
        {       
            for( s32 i = 0 ; i < m_CreditLines[index].m_CharCount ; i ++ )
            {
                m_CreditLines[index].m_FadeDelay[i]-=DeltaTime;

                if( (m_CreditLines[index].m_FadeDelay[i] <= 0.0f) && m_CreditLines[index].m_CustomRenderStruct[i].m_Frame == 0)
                {
                    m_CreditLines[index].m_CustomRenderStruct[i].m_State = ui_font::s_render;
                    m_CreditLines[index].m_SweepTime[i] = 1.0f;
                    m_CreditLines[index].m_CustomRenderStruct[i].m_Frame = 1;
                    m_CreditLines[index].m_CustomRenderStruct[i].m_Value = 0;
                }
                else if( m_CreditLines[index].m_CustomRenderStruct[i].m_State == ui_font::s_render )
                {
                    m_CreditLines[index].m_SweepTime[i] += DeltaTime * SWEEP_FRAME_RATE;
                    m_CreditLines[index].m_CustomRenderStruct[i].m_Frame = (s32)m_CreditLines[index].m_SweepTime[i];

                    // Find what alpha we are in.
                    {
                        if(m_CreditLines[index].m_CustomRenderStruct[i].m_Frame < SWEEP_FRAME_MAX)
                        {
                            for( s32 s = 0 ; s < 2 ; s++ )
                            {
                                if( m_CreditLines[index].m_CustomRenderStruct[i].m_Frame <= Sweeps[s].EndTime && m_CreditLines[index].m_CustomRenderStruct[i].m_Frame >= Sweeps[s].StartTime )
                                {
                                    m_CreditLines[index].m_CustomRenderStruct[i].m_Value = ((((m_CreditLines[index].m_CustomRenderStruct[i].m_Frame - Sweeps[s].StartTime) / (Sweeps[s].EndTime - Sweeps[s].StartTime)) * (Sweeps[s].EndValue-Sweeps[s].StartValue)) + Sweeps[s].StartValue);
                                }
                            }
                        }
                        else
                        {
                            m_CreditLines[index].m_CustomRenderStruct[i].m_Value = FINAL_PIXEL_COLOR;
                        }
                    }
                }
            }
        }
    }
}

//=========================================================================

void dlg_credits::InitCreditLines( void )
{
    for( s32 index = 0 ; index < MAX_CREDIT_LINES_PER_PAGE ; index++ )
    {
        m_CreditLines[index].m_Type = LINE_TYPE_NORMAL;
        m_CreditLines[index].m_Color = xcolor(255,255,255,255);
        x_strcpy( m_CreditLines[index].m_Font, "large" );
        m_CreditLines[index].m_Rect = irect(0,0,0,0);
        m_CreditLines[index].m_String.Clear();
        m_CreditLines[index].m_CustomRenderStruct.Clear();
        m_CreditLines[index].m_FadeDelay.Clear();
        m_CreditLines[index].m_SweepTime.Clear();
        m_CreditLines[index].m_CharCount = 0;
        m_CreditLines[index].m_RenderFlags = 0;
        m_CreditLines[index].m_StringLength= 0;
    }
}

//=========================================================================

xbool dlg_credits::BuildPage( void )
{
    InitCreditLines();

    s32 CurrentLine = 0;

    if( m_CreditsDone )
    {
        return( TRUE );
    }

    while( TRUE )
    {
        const xwstring* pText = g_StringTableMgr.FindString( "credits", xfs( "IDS_CREDIT_LINE_%03d", m_LineIndex ) );

        if( !pText )
        {
            m_PageLineCount = 0;
            m_CreditsDone = TRUE;
            x_throw( "Missing credits line or end marker" );
            return TRUE;
        }

        xwstring Text = *pText;
        s32 Type = CheckLine( Text );

        if( Type == CREDITS_NEW_PAGE || Type == CREDITS_DONE )
        {
            m_CreditsDone = Type == CREDITS_DONE;
            break;
        }

        if( CurrentLine >= MAX_CREDIT_LINES_PER_PAGE )
        {
            m_PageLineCount = 0;
            m_CreditsDone = TRUE;
            x_throw( "Credits page exceeds its line limit" );
            return TRUE;
        }

        CreditLine& Line = m_CreditLines[CurrentLine];
        Line.m_String = Text;
        Line.m_CharCount = Line.m_String.GetLength();
        Line.m_CustomRenderStruct.SetCount( Line.m_CharCount );
        Line.m_FadeDelay.SetCount( Line.m_CharCount );
        Line.m_SweepTime.SetCount( Line.m_CharCount );

        for( s32 i = 0; i < Line.m_CharCount; i++ )
        {
            Line.m_CustomRenderStruct[i].m_State = ui_font::s_no_render;
            Line.m_CustomRenderStruct[i].m_Value = 0.0f;
            Line.m_CustomRenderStruct[i].m_Frame = 0;
            Line.m_FadeDelay[i]                  = x_frand( FADE_MIN_START, FADE_MAX_START );
            Line.m_SweepTime[i]                  = 0.0f;
        }

        if( Type == CREDITS_TITLE_CODE )
        {
            x_strcpy( Line.m_Font, "loadscr" );
            Line.m_Type = LINE_TYPE_TITLE;
        }

        Line.m_RenderFlags = ui_font::h_center | ui_font::v_center;
        m_LineIndex++;
        CurrentLine++;
    }

    // Set number of Lines this page.
    ASSERT( CurrentLine <= MAX_CREDIT_LINES_PER_PAGE );
    m_PageLineCount = CurrentLine;

    // Given number of lines.. build the rects for rendering.
    s32 line;
    s32 TotalPageHeight = 0;
    s32 LineOffset = 0;

    // Get Total line height in font pixels
    for( line = 0 ; line < m_PageLineCount ; line++ )
    {
        TotalPageHeight += g_UiMgr->GetLineHeight( g_UiMgr->FindFont(m_CreditLines[line].m_Font) );

        irect TextRect;
        g_UiMgr->TextSize( g_UiMgr->FindFont(m_CreditLines[line].m_Font), TextRect, m_CreditLines[line].m_String, -1);
        m_CreditLines[line].m_StringLength = TextRect.GetWidth();
    }

    const irect UserBounds = g_UiMgr->GetUserBounds( m_UserID );
    const s32   PageCenter = (UserBounds.t + UserBounds.b) / 2;

    // Setup spaceing 
    for( line = 0 ; line < m_PageLineCount ; line++ )
    {
        m_CreditLines[line].m_Rect.l = UserBounds.l;
        m_CreditLines[line].m_Rect.r = UserBounds.r;

        m_CreditLines[line].m_Rect.t = (PageCenter - (TotalPageHeight / 2)) +  LineOffset;
        m_CreditLines[line].m_Rect.b = m_CreditLines[line].m_Rect.t + g_UiMgr->GetLineHeight( g_UiMgr->FindFont(m_CreditLines[line].m_Font) ); 

        LineOffset += g_UiMgr->GetLineHeight( g_UiMgr->FindFont(m_CreditLines[line].m_Font) );
    }

    m_PageTimer =  DEFAULT_LINE_TIME;
    m_CurrentPageTime = 0.0f;
    m_LineIndex++;

    return( FALSE );
}

//=========================================================================

s32 dlg_credits::CheckLine( xwstring& String )
{
    const xwchar* pString = String;

    while( *pString )
    {
        const xwchar* pStart = pString;
        xwchar c = *pString++;

        if( c == '{' && x_wstrncmp( pString, xwstring( "Input:" ), 6 ) == 0 )
        {
            pString += 6;
            xwstring Token;

            while( *pString && *pString != '}' )
            {
                Token += *pString++;
            }

            if( !*pString )
            {
                x_throw( "Unterminated credits marker" );
                return CREDITS_DONE;
            }

            if( x_wstrcmp( Token, xwstring( "NEWPAGE" ) ) == 0 )
            {
                return CREDITS_NEW_PAGE;
            }

            if( x_wstrcmp( Token, xwstring( "TITLE" ) ) == 0 )
            {
                String.Delete( (s32)(pStart - (const xwchar*)String), (s32)(pString - pStart + 1) );
                return CREDITS_TITLE_CODE;
            }

            if( x_wstrcmp( Token, xwstring( "CREDITEND" ) ) == 0 )
            {
                return CREDITS_DONE;
            }
        }
    }
    return ( CREDITS_NO_CODE );
}
