//==============================================================================
//
//  hud_TextRenderer.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================
//==============================================================================
//  INCLUDES
//==============================================================================

#include "hud_Text.hpp"
#include "HudObject.hpp"

#include "UI/ui_font.hpp"
#include "StringMgr/StringMgr.hpp"
#ifndef X_EDITOR
#include "UI/ui_manager.hpp"
#include "UI/ui_renderer.hpp"
#endif

// These must be positive and in increasing order.  Shouldn't overlap,
// but there can be gaps if so desired.  Units are in seconds.
#define SCROLL_DOWN_START    1.9f
#define SCROLL_DOWN_END      2.0f

#define SLIDE_IN_START       0.0f
#define SLIDE_IN_END         0.0f

#define FLARE_START          2.5f
#define FLARE_END            3.0f


// These must be negative and in decreasing order.
#define SLIDE_OUT_START     -0.0f
#define SLIDE_OUT_END       -0.0f

#define SCROLL_UP_START     -2.0f
#define SCROLL_UP_END       -2.1f 

#define BASE_MSG_STAY       1.8f
#define MSG_STAY_PER_CHAR   0.04f
#define MAX_MSG_STAY        8.0f

#define MAX_CHARS_SOFT      240     // Point after which to set message time left to SLIDE_OUT_START because we're getting too much to display.
#define MAX_CHARS_HARD      300     // Point after which to set message inuse field to FALSE.
#define MAX_LINES_SOFT      5      // Number of lines after which we start immediately fading.
#define MAX_LINES_HARD      5      // Number of lines after which we make any disappear immediately.


//==============================================================================
//  FUNCTIONS
//==============================================================================



//==============================================================================

hud_text::hud_text( void )
{
    m_NumDisplay            =  5;
    m_CursorPos             =  0.0f;
    m_TopLine               =  0;
    m_NumGoals              =  0;

    m_MaxTextWidth          = 0;

    m_TopGoal               = 0;

    m_TextBoxRect.l         = 0;
    m_TextBoxRect.t         = 0;
    m_TextBoxRect.r         = 0;
    m_TextBoxRect.b         = 0;

    m_PosX                  = 50;
    m_PosY                  = 50;

    s32 i;
    for( i = 0; i < MAX_QUEUE; i++ ) 
    {
       m_Lines[ i ].Reset();

    }  

    for( i = 0; i < MAX_GOALS; i++ ) 
    {
        m_Goals[ i ].Reset();
    } 

    m_Bonus.Reset();
    m_WeaponInfo.Reset();

    m_TextBoxRectState = TEXT_BOX_STATE_CLOSED;
    m_PercentOpen = 0.0f;
}

//==============================================================================

// I'm using this instead of a straight circular queue like the messages use
// because goals can come and go as they please, leaving gaps in any queue.
s32 hud_text::GetIthGoal( s32 Index )
{
    s32 Last = -1;

    s32 i;    
    for( i = 0; i <= Index; i++ )
    {
        s32 Current = -1;
        
        s32 j;
        for( j = 0; j < MAX_GOALS; j++ )
        {
            if( m_Goals[ j ].InUse )
            {
                
                if( (Last == -1) || (m_Goals[ j ].SeqNum > m_Goals[ Last ].SeqNum) )
                {
                    if( (Current == -1) ||  (m_Goals[ j ].SeqNum < m_Goals[ Current ].SeqNum) ) 
                    {
                        Current = j;
                    }
                }

            }
        }
        Last = Current;
    }
    
    return Last;
}

//==============================================================================

hud_text::~hud_text( void )
{
}

//==============================================================================
s32 s_wi_t = 340;
s32 s_wi_b = 360;
s32 s_wi_l = 400;
s32 s_wi_r = 500;

static xcolor CHAT_RECT_COLOR_GREEN = xcolor( 0,31,0,127 );
static xbool  CHAT_DROP_SHADOW = FALSE;
static xcolor TEXT_BOX_BACK_COLOR = xcolor( 0,0,0,80);
#define CHAT_BOX_LINEFEED   17
void hud_text::OnRender( player* pPlayer )
{
#ifndef X_EDITOR 
    (void)pPlayer;

    // Get the hardware resolution for use later
    s32 XRes;
    s32 YRes;
    eng_GetRes( XRes, YRes );

    ui_font* pFont      = g_UiMgr->GetFont( "small" );    

    irect iRect;

    s32 TopFullLine     = (s32)(m_CursorPos);
    f32 Offset          = m_CursorPos - TopFullLine;
    
    u8 Alpha = 255;
    u8 RectAlpha = 180;

    s32 i;

    f32 CurrentOffset = m_YPos;

    //
    // Render the goals
    //
    for( i = 0; i < m_NumGoals; i++ )
    {
        s32 iGoal = GetIthGoal( i );

        if( m_Goals[ iGoal ].InUse )
        {
            f32 PercentOnScreen     = 0.0f;
            const xwchar* pLine     = m_Goals[ iGoal ].Wrapped.Text;
            
            PercentOnScreen = 1.0f;

            //Find the ith goal
            iRect.t = (s32)CurrentOffset;
            iRect.b = (s32)CurrentOffset + pFont->TextHeight(pLine);//CHAT_BOX_LINEFEED;
            iRect.l = (s32)0;
            iRect.r = (s32)0;

            CurrentOffset  += pFont->TextHeight(pLine);//CHAT_BOX_LINEFEED;

            iRect.l = (s32)m_XPos;
            iRect.r = (s32)pFont->TextWidth(pLine)+(s32)m_XPos+8;

            if( m_Goals[iGoal].KeyingPos < 400 )
            {
                // scissor region
                // TODO: ?
            }

            xcolor TextColor = XCOLOR_WHITE;
            RectAlpha = CHAT_RECT_COLOR_GREEN.A;
            Alpha = (u8)(255 * (m_Goals[ iGoal ].Time > 0.25f ? 1.0f : (m_Goals[ iGoal ].Time * 4.0f)));
            RectAlpha = (u8)(CHAT_RECT_COLOR_GREEN.A * (m_Goals[ iGoal ].Time > 0.25f ? 1.0f : (m_Goals[ iGoal ].Time * 4.0f)));

            // full rect area

            xcolor greenColor;
            greenColor = CHAT_RECT_COLOR_GREEN;
            greenColor.A = RectAlpha;

            g_UIRenderer.DrawRect( iRect, greenColor );

            // blended end
            irect iFadeOut;
            iFadeOut.l = iRect.r;
            iFadeOut.r = iFadeOut.l+8;
            iFadeOut.t = iRect.t;
            iFadeOut.b = iRect.b;
            g_UIRenderer.DrawGradientRect( iFadeOut, greenColor, greenColor, xcolor(0,31,0,0), xcolor(0,31,0,0) );

            // blended start
            irect iFadeIn;
            iFadeIn.l = iRect.l-8;
            iFadeIn.r = iRect.l;
            iFadeIn.t = iRect.t;
            iFadeIn.b = iRect.b;
            g_UIRenderer.DrawGradientRect( iFadeIn, xcolor(0,180,0,80), xcolor(0,180,0,80), greenColor, greenColor );

            // Bright side line.
            irect rLine;
            rLine.Set(iRect.l-8,iRect.t,iRect.l-7,iRect.b);
            g_UIRenderer.DrawRect( rLine, g_HudColor, TRUE );

            // text
            iRect.t-=1;
            iRect.b-=1;
            RenderLine( m_Goals[ iGoal ].Wrapped, iRect, Alpha, TextColor, 1, ui_font::h_left|ui_font::v_top, CHAT_DROP_SHADOW );

            // restore scissor region
            // TODO ?
        }
    }

    iRect.l = (s32)m_XPos+1;
    iRect.r = (s32)300;

    CurrentOffset += 0.0f;

    i = TopFullLine;
    if( Offset > 0.0f )
    {
        i               += 1;
        CurrentOffset   -= (1.0f - Offset) * pFont->TextHeight( m_Lines[ i % MAX_QUEUE ].Wrapped.Text );
    }


    //
    // Render the messages to the display
    //
    for( ; 
        (i >= 0) && (i > (TopFullLine - m_NumDisplay)); 
        i-- )
    {
        s32 QueueIndex = i % MAX_QUEUE;

        const xwchar* pLine = m_Lines[ QueueIndex ].Wrapped.Text;

        iRect.t = (s32)CurrentOffset;
        iRect.b = iRect.t + pFont->TextHeight(pLine);//CHAT_BOX_LINEFEED;    

        iRect.l = (s32)m_XPos;
        iRect.r = (s32)pFont->TextWidth(pLine)+(s32)m_XPos+8;

        // Blank line?
        if( iRect.r-8 <= iRect.l )
        {
            iRect.r = iRect.l;
        }
        else
        {       
            CurrentOffset += pFont->TextHeight(pLine);//CHAT_BOX_LINEFEED;

            RectAlpha = CHAT_RECT_COLOR_GREEN.A;

            // Alpha for top line as it scrolls in.
            if( i == (TopFullLine + 1) )
            {
                Alpha = (u8)(255 * Offset);
                RectAlpha = (u8)(CHAT_RECT_COLOR_GREEN.A * Offset);
            }

            // Alpha for bottom line that is potentially timing out or scrolling out.
            else if( i == (TopFullLine - m_NumDisplay + 1) ) 
            {
                u8 Option1 = (u8)(255 * (1.0f - Offset));
                u8 Option2 = (u8)(255 * (m_Lines[ QueueIndex ].Time > 0.25f ? 1.0f : (m_Lines[ QueueIndex ].Time * 4.0f)));

                Alpha = x_min(Option1, Option2);

                Option1 = (u8)(CHAT_RECT_COLOR_GREEN.A * (1.0f - Offset));
                Option2 = (u8)(CHAT_RECT_COLOR_GREEN.A * (m_Lines[ QueueIndex ].Time > 0.25f ? 1.0f : (m_Lines[ QueueIndex ].Time * 4.0f)));

                RectAlpha = x_min(Option1, Option2);
            }

            // Alpha for all other lines that could potentially be timing out.
            else
            {
                Alpha =     (u8)(255 * (m_Lines[ QueueIndex ].Time > 0.25f ? 1.0f : (m_Lines[ QueueIndex ].Time * 4.0f)));
                RectAlpha = (u8)(CHAT_RECT_COLOR_GREEN.A * (m_Lines[ QueueIndex ].Time > 0.25f ? 1.0f : (m_Lines[ QueueIndex ].Time * 4.0f)));
            }

            xcolor TextColor = XCOLOR_WHITE;
           
            xcolor greenColor = CHAT_RECT_COLOR_GREEN;
            greenColor.A = RectAlpha;

            // full rect area
            g_UIRenderer.DrawRect( iRect, greenColor );

            // blended end
            irect iFadeOut;
            iFadeOut.l = iRect.r;
            iFadeOut.r = iFadeOut.l+8;
            iFadeOut.t = iRect.t;
            iFadeOut.b = iRect.b;
            g_UIRenderer.DrawGradientRect( iFadeOut, greenColor, greenColor, xcolor(0,31,0,0), xcolor(0,31,0,0) );

            // blended Start
            irect iFadeIn;
            iFadeIn.l = iRect.l-8;
            iFadeIn.r = iRect.l;
            iFadeIn.t = iRect.t;
            iFadeIn.b = iRect.b;
            g_UIRenderer.DrawGradientRect( iFadeIn, xcolor(0,180,0,80), xcolor(0,180,0,80), greenColor, greenColor );

            // Bright side line.
            irect rLine;
            rLine.Set(iRect.l-8,iRect.t,iRect.l-7,iRect.b);
            g_UIRenderer.DrawRect( rLine, g_HudColor, TRUE );

            // text
            iRect.t-=1;
            iRect.b-=1;
            RenderLine( m_Lines[ QueueIndex ].Wrapped, iRect, Alpha, TextColor, 1, ui_font::h_left|ui_font::v_top, CHAT_DROP_SHADOW );
        }
    }
    
    //
    // Render the bonus text
    //
    if( m_Bonus.InUse )
    {
        static irect BonusRect = irect( 0,
                                        148,
                                        ui_viewport::CONTENT_WIDTH,
                                        ui_viewport::CONTENT_HEIGHT );
        
        xcolor TextColor = XCOLOR_YELLOW;
        RenderLine( m_Bonus.Source, BonusRect, 255, TextColor, 1, ui_font::h_center|ui_font::v_top, TRUE );
    }

    // Render Weapon Info Text
    if( m_WeaponInfo.InUse )
    {
        iRect.t = s_wi_t; // 340
        iRect.b = s_wi_b; // 360
        iRect.l = s_wi_l; // 400
        iRect.r = s_wi_r; // 500

        xcolor TextColor = XCOLOR_BLUE;
        RenderLine( m_WeaponInfo.Source, iRect, 255, TextColor, 0, ui_font::h_left|ui_font::v_bottom, TRUE );
    }

#endif
}

//==============================================================================

inline void hud_text::AddLinesAndChars( s32& NumLines, s32& NumChars, const xwchar* pLine )
{
    if( *pLine == 0 )
        return;
    
    {    
        NumLines++;
        while( *pLine != 0 )
        {
            // Linebreak?
            if( *pLine == '\n' )
            {
                NumLines++;
            }

            else
            {
                NumChars++;
            }

            pLine++;
        }
    }
}

//==============================================================================
                                       
inline void hud_text::ClearAllBelow( s32 MsgIndex )
{
    while( (MsgIndex >= 0) && ( (MsgIndex % MAX_QUEUE != m_TopLine % MAX_QUEUE) || (MsgIndex == m_TopLine)) )
    {
        m_Lines[ MsgIndex % MAX_QUEUE ].Time  = 0.0f;
        m_Lines[ MsgIndex % MAX_QUEUE ].InUse = FALSE;

        m_Lines[ MsgIndex % MAX_QUEUE ].Reset();

        MsgIndex--;
    }
}

//==============================================================================

static s32  TEXT_BOX_POS_L = 16;
static s32  TEXT_BOX_POS_T = 16;
static s32  TEXT_BOX_POS_R = 350;
static s32  TEXT_BOX_LINE_HEIGHT = CHAT_BOX_LINEFEED;
static f32  TEXT_BOX_OPEN_SPEED = 6.0f;

void hud_text::OnAdvanceSimulation( player* pPlayer, f32 DeltaTime )
{
    (void)pPlayer;

    s32 LinesDisplayed = 0;
    s32 CharsDisplayed = 0;
    s32 TextWidth = 0;

#ifndef X_EDITOR
    ui_font* pFont = g_UiMgr->GetFont( "small" );
#endif

    // Advance and expire objective entries.
    for( s32 i = 0; i < MAX_GOALS; i++ )
    {
        if( !m_Goals[ i ].InUse )
            continue;

        AddLinesAndChars( LinesDisplayed, CharsDisplayed, m_Goals[ i ].Wrapped.Text );
#ifndef X_EDITOR
        TextWidth = MAX( TextWidth, pFont->TextWidth( m_Goals[ i ].Wrapped.Text ) );
#endif

        if( m_Goals[ i ].Time >= 0.0f )
        {
            m_Goals[ i ].Time -= DeltaTime;
            if( m_Goals[ i ].Time <= 0.0f )
            {
                m_Goals[ i ].Reset();
                m_NumGoals = MAX( 0, m_NumGoals - 1 );
            }
        }
    }

    // Advance the normal message queue and remove expired/hidden lines.
    {
        const f32 ScrollSpeed = 0.8f;
        if( m_CursorPos < m_TopLine )
        {
            const f32 Scroll = (5.0f + (m_TopLine - m_CursorPos)) * DeltaTime * ScrollSpeed;
            m_CursorPos += Scroll;
            if( m_CursorPos > m_TopLine )
                m_CursorPos = (f32)m_TopLine;
        }

        ClearAllBelow( (s32)m_CursorPos - m_NumDisplay );

        for( s32 i = m_TopLine; i >= 0; i-- )
        {
            AddLinesAndChars( LinesDisplayed, CharsDisplayed, m_Lines[ i % MAX_QUEUE ].Wrapped.Text );

#ifndef X_EDITOR
            TextWidth = MAX( TextWidth, pFont->TextWidth( m_Lines[ i % MAX_QUEUE ].Wrapped.Text ) );
#endif

            if( (LinesDisplayed > MAX_LINES_HARD) || (CharsDisplayed > MAX_CHARS_HARD) )
            {
                m_Lines[ i % MAX_QUEUE ].Time = 0.0f;
                ClearAllBelow( i );
                break;
            }

            if( (LinesDisplayed > MAX_LINES_SOFT) || (CharsDisplayed > MAX_CHARS_SOFT) )
            {
                m_Lines[ i % MAX_QUEUE ].Time = x_min( 0.25f, m_Lines[ i % MAX_QUEUE ].Time );
                ClearAllBelow( i - 1 );
            }

            // The oldest visible line is the one that controls queue expiry.
            // Do not index the circular buffer with -1 on the first line.
            if( (i == 0) || !m_Lines[ (i - 1) % MAX_QUEUE ].InUse )
            {
                m_Lines[ i % MAX_QUEUE ].Time -= DeltaTime;
                if( m_Lines[ i % MAX_QUEUE ].Time <= 0.0f )
                    ClearAllBelow( i );
                break;
            }
        }
    }

    if( m_Bonus.InUse )
    {
        m_Bonus.Time -= DeltaTime;
        if( m_Bonus.Time < 0.0f )
            m_Bonus.InUse = FALSE;
    }

    if( m_WeaponInfo.InUse )
    {
        m_WeaponInfo.Time -= DeltaTime;
        if( m_WeaponInfo.Time < 0.0f )
            m_WeaponInfo.InUse = FALSE;
    }

    // Keep the text-box animation state driven by the same update callback as
    // the message timers.
    m_TextBoxRect.l = TEXT_BOX_POS_L;
    m_TextBoxRect.t = TEXT_BOX_POS_T;

    if( LinesDisplayed > 0 )
    {
        m_TextBoxRect.b = TEXT_BOX_LINE_HEIGHT * (LinesDisplayed + 1);
        m_TextBoxRect.r = TEXT_BOX_POS_R;
        m_RenderTextBoxRect = TRUE;

        switch( m_TextBoxRectState )
        {
            case TEXT_BOX_STATE_CLOSED:
                m_TextBoxRectState = TEXT_BOX_STATE_OPENING;
                m_PercentOpen = 0.0f;
                break;

            case TEXT_BOX_STATE_OPENING:
                m_PercentOpen += DeltaTime * TEXT_BOX_OPEN_SPEED;
                if( m_PercentOpen >= 1.0f )
                    m_TextBoxRectState = TEXT_BOX_STATE_OPEN;
                break;

            case TEXT_BOX_STATE_OPEN:
                m_PercentOpen = 1.0f;
                break;

            case TEXT_BOX_STATE_CLOSEING:
                m_TextBoxRectState = TEXT_BOX_STATE_OPENING;
                break;
        }
    }
    else
    {
        m_RenderTextBoxRect = FALSE;

        switch( m_TextBoxRectState )
        {
            case TEXT_BOX_STATE_OPEN:
            case TEXT_BOX_STATE_OPENING:
                m_TextBoxRectState = TEXT_BOX_STATE_CLOSEING;
                break;

            case TEXT_BOX_STATE_CLOSEING:
                m_PercentOpen -= DeltaTime * TEXT_BOX_OPEN_SPEED;
                if( m_PercentOpen <= 0.0f )
                    m_TextBoxRectState = TEXT_BOX_STATE_CLOSED;
                break;

            case TEXT_BOX_STATE_CLOSED:
                m_PercentOpen = 0.0f;
                break;
        }
    }

    m_PercentOpen = MINMAX( 0.0f, m_PercentOpen, 1.0f );
    m_TextBoxRect.b = MAX( (s32)(m_TextBoxRect.b * m_PercentOpen), TEXT_BOX_POS_T ) + 8;
    m_TextBoxRect.r = (s32)((TextWidth + 8 + 16) * m_PercentOpen) + TEXT_BOX_POS_L;

    if( m_TextBoxRect.l > m_TextBoxRect.r )
        m_TextBoxRect.l = m_TextBoxRect.r;
    if( m_TextBoxRect.r < m_TextBoxRect.l )
        m_TextBoxRect.r = m_TextBoxRect.l;
    if( m_TextBoxRect.t > m_TextBoxRect.b )
        m_TextBoxRect.t = m_TextBoxRect.b;
    if( m_TextBoxRect.b < m_TextBoxRect.t )
        m_TextBoxRect.b = m_TextBoxRect.t;
}

void hud_text::SetMaxWidth( s32 MaxWidth )
{
    if( MaxWidth < 0 || MaxWidth == m_MaxTextWidth )
        return;

    m_MaxTextWidth = MaxWidth;

    for( s32 i = 0; i < MAX_QUEUE; i++ )
    {
        if( m_Lines[ i ].InUse )
            RewrapDisplayText( m_Lines[ i ] );
    }

    for( s32 i = 0; i < MAX_GOALS; i++ )
    {
        if( m_Goals[ i ].InUse )
            RewrapDisplayText( m_Goals[ i ] );
    }
}

//==============================================================================

void hud_text::SetDisplayText( text_display& Display, const ui_text_data& Text )
{
    Display.Source = Text;
    RewrapDisplayText( Display );
}

//==============================================================================

void hud_text::RewrapDisplayText( text_display& Display )
{
#ifndef X_EDITOR
    if( m_MaxTextWidth > 0 )
    {
        g_UiMgr->GetFont( "small" )->TextWrap( Display.Source, irect( 0, 0, m_MaxTextWidth, 400 ), Display.Wrapped );
        return;
    }
#endif
    Display.Wrapped = Display.Source;
}

//==============================================================================

void hud_text::AddLine( const xwchar* pLine )
{
    ui_text_data Text;
    Text.Append( pLine );

    AddLine( Text );
}

//==============================================================================

void hud_text::UpdateGoal( s32 Seq, xbool Enabled, const xwchar* pGoal, f32 Time )
{
    ui_text_data Text;
    Text.Append( pGoal );

    UpdateGoal( Seq, Enabled, Text, Time );
}

//==============================================================================

void hud_text::SetBonus( const xwchar* pText, f32 Time )
{
    ui_text_data Text;
    Text.Append( pText );

    SetBonus( Text, Time );
}

//==============================================================================

void hud_text::SetWeaponInfo( const xwchar* pText, f32 Time )
{
    ui_text_data Text;
    Text.Append( pText );

    SetWeaponInfo( Text, Time );
}

//==============================================================================

void hud_text::AddLine( const ui_text_data& Line )
{
#ifndef X_EDITOR
    m_TopLine++;
    text_display& Display = m_Lines[ m_TopLine % MAX_QUEUE ];
    Display.Reset();
    SetDisplayText( Display, Line );
    f32 StayTime = BASE_MSG_STAY + Display.Wrapped.Text.GetLength() * MSG_STAY_PER_CHAR;
    Display.InUse = TRUE;
    Display.Time = MIN( StayTime, MAX_MSG_STAY );
#else
    (void)Line;
#endif
}

//==============================================================================

void hud_text::AddGoal( s32 GoalID, const ui_text_data& Goal, f32 Time )
{
#ifndef X_EDITOR
    for( s32 i = 0; i < MAX_GOALS; i++ )
    {
        if( !m_Goals[ i ].InUse )
        {
            SetDisplayText( m_Goals[ i ], Goal );

            // Play Sound here?
            g_AudioMgr.Play("HUD_Text_Alert", TRUE );

            m_Goals[ i ].ScrollState    = 1;
            m_Goals[ i ].SeqNum         = ++m_TopGoal;
            m_Goals[ i ].InUse          = TRUE;
            m_Goals[ i ].Time           = Time;
            m_Goals[ i ].GoalID         = GoalID;
            m_Goals[ i ].KeyingPos      = 0;

            m_NumGoals++;

            return;
        }
    }
#else
    (void)GoalID;
    (void)Goal;
    (void)Time;
#endif
}

//==============================================================================

void hud_text::UpdateGoal( s32 GoalID, xbool Enabled, const ui_text_data& Goal, f32 Time )
{
    s32 i;
    for( i = 0; i < MAX_GOALS; i++ )
    {
        // This means that whoever created this goal does not want to manipulate it
        // in the future and are planning on it fading on its own.
        if( GoalID == -1 )
        {
            if( Enabled )
            {
                break;
            }
            else
            {
                return; // You can't clear a goal created with -1
            }
        }

        // Updating or deleting:
        if( GoalID == m_Goals[ i ].GoalID && m_Goals[ i ].InUse )
        {
            // If the time has changed, set that
            if( Time >= 0 )
            {
                m_Goals[ i ].Time = Time;
            }
            
            // Copy the new source text and refresh its wrapped display cache.
            SetDisplayText( m_Goals[ i ], Goal );

            //if( m_Goals[ i ].ScrollState > FLARE_START )
            //{
            //    m_Goals[ i ].ScrollState    = FLARE_START;
            //}
            //else
            //{
            //    m_Goals[ i ].ScrollState    = SCROLL_DOWN_START;
            //}

            //if( !Enabled )  
            //{
            //    m_Goals[ i ].ScrollState    = SLIDE_OUT_START;
            //}
            return;
        }
    }

    // Creating:
    if( Enabled )
    {
        AddGoal( GoalID, Goal, Time );
    }
}

//==============================================================================

void hud_text::SetBonus( const ui_text_data& Bonus, f32 Time )
{
    m_Bonus.Time  = Time;
    m_Bonus.InUse = TRUE;

    SetDisplayText( m_Bonus, Bonus );
}

//==============================================================================

void hud_text::SetWeaponInfo( const ui_text_data& WeaponInfo, f32 Time )
{
    m_WeaponInfo.Time  = Time;
    m_WeaponInfo.InUse = TRUE;

    SetDisplayText( m_WeaponInfo, WeaponInfo );

}



//==============================================================================
