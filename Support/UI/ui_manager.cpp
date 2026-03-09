//=========================================================================
//
//  ui_manager.cpp
//
//=========================================================================

#include "entropy.hpp"
#include "ui_manager.hpp"
#include "ui_win.hpp"
#include "ui_font.hpp"
#include "ui_dialog.hpp"
#include "ui_button.hpp"
#include "ui_frame.hpp"
#include "ui_combo.hpp"
#include "ui_radio.hpp"
#include "ui_check.hpp"
#include "ui_edit.hpp"
#include "ui_text.hpp"
#include "ui_slider.hpp"
#include "ui_listbox.hpp"
#include "ui_blankbox.hpp"
#include "ui_bitmap.hpp"
#include "ui_joinlist.hpp"
#include "ui_playerlist.hpp"
#include "ui_friendlist.hpp"
#include "ui_maplist.hpp"
#include "ui_textbox.hpp"
#include "ui_dlg_vkeyboard.hpp"
#include "ui_tabbed_dialog.hpp"
#include "ui_dlg_list.hpp"
#include "ResourceMgr\ResourceMgr.hpp"
#include "StateMgr/StateMgr.hpp"

#ifndef X_RELEASE
#include "InputMgr/Monkey.hpp"
#include "InputMgr/GamePad.hpp"
#endif

#include "bitmap\aux_bitmap.hpp"

#include "stringmgr\stringmgr.hpp"
#include "AudioMgr\AudioMgr.hpp"

#ifdef TARGET_PC
static const char*  m_ButtonTexturesNames[] = {
PRELOAD_PS2_FILE("UI_ButtonsPS2_x.xbmp"),                
PRELOAD_PS2_FILE("UI_ButtonsPS2_square.xbmp"),           
PRELOAD_PS2_FILE("UI_ButtonsPS2_triangle.xbmp"),         
PRELOAD_PS2_FILE("UI_ButtonsPS2_circle.xbmp"),           
PRELOAD_PS2_FILE("UI_ButtonsPS2_directionalDown.xbmp"),  
PRELOAD_PS2_FILE("UI_ButtonsPS2_directionalLeft.xbmp"),  
PRELOAD_PS2_FILE("UI_ButtonsPS2_directionalUp.xbmp"),    
PRELOAD_PS2_FILE("UI_ButtonsPS2_directionalRight.xbmp"),
PRELOAD_PS2_FILE("UI_ButtonsPS2_directionalUpDown.xbmp"),
PRELOAD_PS2_FILE("UI_ButtonsPS2_directionalLeftRight.xbmp"),
PRELOAD_PS2_FILE("UI_ButtonsPS2_stickR.xbmp"),           
PRELOAD_PS2_FILE("UI_ButtonsPS2_stickL.xbmp"),           
PRELOAD_PS2_FILE("UI_ButtonsPS2_L1.xbmp"),               
PRELOAD_PS2_FILE("UI_ButtonsPS2_L2.xbmp"),               
PRELOAD_PS2_FILE("UI_ButtonsPS2_R1.xbmp"),               
PRELOAD_PS2_FILE("UI_ButtonsPS2_R2.xbmp"),               
PRELOAD_PS2_FILE("UI_ButtonsPS2_start.xbmp"),            
PRELOAD_PS2_FILE("UI_KillsIcon.xbmp"),
PRELOAD_PS2_FILE("UI_TeamKillsIcon.xbmp"),
PRELOAD_PS2_FILE("UI_DeathIcon.xbmp"),
PRELOAD_PS2_FILE("UI_FlagsIcon.xbmp"),
PRELOAD_PS2_FILE("UI_VotesIcon.xbmp"),
PRELOAD_PS2_FILE("UI_FlagsIcon.xbmp"),  // DUMMY
PRELOAD_PS2_FILE("UI_FlagsIcon.xbmp"),  // DUMMY
PRELOAD_PS2_FILE("UI_FlagsIcon.xbmp")   // DUMMY
};
#else
#endif

#define NUM_BUTTON_CODES        58

//==========================================================================
//  button code table entry
//==========================================================================

struct button_code
{
    xwstring        CodeString;
    s32             ButtonCode;
};

#ifdef TARGET_PC //DUMMY
static const button_code m_ButtonCodeTable[NUM_BUTTON_CODES] = {
    {   "x",            PS2_BUTTON_CROSS            },
    {   "q",            PS2_BUTTON_SQUARE           },
    {   "a",            PS2_BUTTON_TRIANGLE         },
    {   "o",            PS2_BUTTON_CIRCLE           },
    {   "d",            PS2_BUTTON_DPAD_DOWN        },
    {   "l",            PS2_BUTTON_DPAD_LEFT        },
    {   "u",            PS2_BUTTON_DPAD_UP          },
    {   "r",            PS2_BUTTON_DPAD_RIGHT       },
    {   "R",            PS2_BUTTON_STICK_RIGHT      },
    {   "L",            PS2_BUTTON_STICK_LEFT       },
    {   "1",            PS2_BUTTON_L1               },
    {   "2",            PS2_BUTTON_L2               },
    {   "3",            PS2_BUTTON_R1               },
    {   "4",            PS2_BUTTON_R2               },
    {   "S",            PS2_BUTTON_START            },
    {   "SQUARE",       PS2_BUTTON_SQUARE           },
    {   "CROSS",        PS2_BUTTON_CROSS            },
    {   "TRIANGLE",     PS2_BUTTON_TRIANGLE         },
    {   "CIRCLE",       PS2_BUTTON_CIRCLE           },
    {   "DOWN",         PS2_BUTTON_DPAD_DOWN        },
    {   "LEFT",         PS2_BUTTON_DPAD_LEFT        },
    {   "UP",           PS2_BUTTON_DPAD_UP          },
    {   "RIGHT",        PS2_BUTTON_DPAD_RIGHT       },
    {   "UPDOWN",       PS2_BUTTON_DPAD_UPDOWN      },
    {   "LEFTRIGHT",    PS2_BUTTON_DPAD_LEFTRIGHT   },
    {   "L1",           PS2_BUTTON_L1               },
    {   "L2",           PS2_BUTTON_L2               },
    {   "L3",           PS2_BUTTON_STICK_LEFT       },
    {   "R1",           PS2_BUTTON_R1               },
    {   "R2",           PS2_BUTTON_R2               },
    {   "R3",           PS2_BUTTON_STICK_RIGHT      },
    {   "PAUSE",        PS2_BUTTON_START            },
    {   "GRENADE",      PS2_BUTTON_SQUARE           }, 
    {   "RELOAD",       PS2_BUTTON_CROSS            }, 
    {   "USE",          PS2_BUTTON_CROSS            }, 
    {   "PREVWEAPON",   PS2_BUTTON_TRIANGLE         }, 
    {   "NEXTWEAPON",   PS2_BUTTON_CIRCLE           }, 
    {   "LEANLEFT",     PS2_BUTTON_DPAD_LEFT        }, 
    {   "LEANRIGHT",    PS2_BUTTON_DPAD_RIGHT       }, 
    {   "LEAN",         PS2_BUTTON_DPAD_LEFTRIGHT   }, 
    {   "MUTATE",       PS2_BUTTON_DPAD_UP          }, 
    {   "TRANSFORM",    PS2_BUTTON_DPAD_UP          }, 
    {   "JUMP",         PS2_BUTTON_L1               }, 
    {   "CROUCH",       PS2_BUTTON_L2               }, 
    {   "FLASHLIGHT",   PS2_BUTTON_STICK_LEFT       }, 
    {   "FIRE",         PS2_BUTTON_R1               }, 
    {   "SECONDARY",    PS2_BUTTON_R2               }, 
    {   "MELEE",        PS2_BUTTON_STICK_RIGHT      }, 
    {   "PARASITE",     PS2_BUTTON_R1               }, 
    {   "CONTAGION",    PS2_BUTTON_R2               }, 
    {   "KILLS",        KILL_ICON                   },
    {   "DEATHS",       DEATH_ICON                  },
    {   "TKS",          TEAM_KILL_ICON              },
    {   "FLAGS",        FLAG_ICON                   },
    {   "VOTES",        VOTE_ICON                   },
    {   "NEWPAGE",      NEW_CREDIT_PAGE,            },
    {   "TITLE",        CREDIT_TITLE_LINE,          },
    {   "CREDITEND",    CREDIT_END,                 }
};
#else
#endif

//=========================================================================
//  Defines
//=========================================================================

#define ENABLE_SCREENSHOTS  0
static const f32 s_ProgressBarScale      = 4.0f;

static const f32 s_PercentBetweenUpdates = 5.0f;

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

static xbool ScaleText = FALSE;

ui_manager* g_UiMgr    = NULL;
s32         g_UiUserID = -1;

s32 g_uiLastSelectController = 0;
static s32 s_EndDialogCount;

//=========================================================================
//  PC target manager
//=========================================================================

#ifdef TARGET_PC
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_composite.hpp"
#include "Entropy/D3DEngine/d3deng_draw_rt.hpp"
#endif

#ifdef TARGET_PC
static void UIStage_OnBeginFrame( void )
{
    const rtarget* pUITarget = draw_GetUITarget();
    if( !pUITarget )
        return;

    if( !rtarget_PushTargets() )
        return;

    rtarget_SetTargets( pUITarget, 1, NULL );

    f32 clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    rtarget_Clear( RTARGET_CLEAR_COLOR, clearColor, 1.0f, 0 );

    rtarget_PopTargets();
}

static void UIStage_OnBeforePresent( void )
{
    const rtarget* pUITarget = draw_GetUITarget();
    if( !pUITarget )
        return;

    rtarget_SetBackBuffer();
    composite_Blit( *pUITarget, COMPOSITE_BLEND_ALPHA );
}

static const eng_frame_stage s_UIFrameStage =
{
    UIStage_OnBeginFrame,
    UIStage_OnBeforePresent
};
#endif

//=========================================================================
//  Helpers
//=========================================================================

void ui_manager::UpdateButton( ui_manager::button& Button, xbool State, f32 DeltaTime )
{
    // Clear number of presses, repeats and releases
    Button.nPresses  = 0;
    Button.nRepeats  = 0;
    Button.nReleases = 0;

    if( m_EnableUserInput )
    {
        // Check for press
        if( !Button.State && State )
        {
            Button.nPresses++;
            Button.RepeatTimer = Button.RepeatDelay;
        }

        // Check for repeat
        if( Button.State && State )
        {
            // If repeat interval is 0 then repeat is disabled
            if( Button.RepeatInterval > 0.0f )
            {
                Button.RepeatTimer -= DeltaTime;
                while( Button.RepeatTimer < 0.0f )
                {
                    Button.nRepeats++;
                    Button.RepeatTimer += Button.RepeatInterval;
                }
            }
        }

        // Check for release
        if( Button.State && !State )
        {
            Button.nReleases++;
        }
    }
    else
    {
        State = 0;
    }

    // Set new state
    Button.State = State;
}

//=========================================================================

void ui_manager::UpdateAnalog( ui_manager::button& Button, f32 Value, f32 DeltaTime )
{
    xbool State = Button.State;

    ASSERT( Button.AnalogDisengage < 1.0f );

    // Clear number of presses, repeats and releases
    Button.nPresses  = 0;
    Button.nRepeats  = 0;
    Button.nReleases = 0;

    if( m_EnableUserInput )
    {
        // Do Analog mapping to a pressed button
        Value *= Button.AnalogScaler;
        if( Value > Button.AnalogEngage )
            State = TRUE;
        if( Value < Button.AnalogDisengage )
            State = FALSE;

        // Make time scaler from Value
        //Value = 1.0f + (Value - Button.AnalogDisengage) / (1.0f - Button.AnalogDisengage) * 3;
        if (Value < 0.0f)
            Value *= -1.0f;

        // Check for press
        if( !Button.State && State )
        {
            Button.nPresses++;
            Button.RepeatTimer = Button.RepeatDelay;
        }

        // Check for repeat
        if( Button.State && State )
        {
            // If repeat interval is 0 then repeat is disabled
            if( Button.RepeatInterval > 0.0f )
            {
                Button.RepeatTimer -= DeltaTime * Value;
                while( Button.RepeatTimer < 0.0f )
                {
                    Button.nRepeats++;
                    Button.RepeatTimer += Button.RepeatInterval;
                }
            }
        }

        // Check for release
        if( Button.State && !State )
        {
            Button.nReleases++;
        }
    }
    else
    {
        State = 0;
    }

    // Set new state
    Button.State = State;
}

//=========================================================================
//  ui_manager
//=========================================================================

ui_manager::ui_manager( void )
{
    m_AlphaTime       = 0.0f;
    m_EnableUserInput = 0;
    m_log             = 0;
}

//=========================================================================

ui_manager::~ui_manager( void )
{
    Kill();
}

//=========================================================================

s32 ui_manager::Init( void )
{
    MEMORY_OWNER( "UI DATA" );
    // Register the default window classes
    RegisterWinClass( "button",     &ui_button_factory      );
    RegisterWinClass( "frame",      &ui_frame_factory       );
    RegisterWinClass( "frame1",     &ui_frame_factory       );
    RegisterWinClass( "frame2",     &ui_frame_factory       );
    RegisterWinClass( "combo",      &ui_combo_factory       );
    RegisterWinClass( "radio",      &ui_radio_factory       );
    RegisterWinClass( "check",      &ui_check_factory       );
    RegisterWinClass( "edit",       &ui_edit_factory        );
    RegisterWinClass( "listbox",    &ui_listbox_factory     );
    RegisterWinClass( "text",       &ui_text_factory        );
    RegisterWinClass( "slider",     &ui_slider_factory      );
    RegisterWinClass( "textbox",    &ui_textbox_factory     );
    RegisterWinClass( "blankbox",   &ui_blankbox_factory    );
    RegisterWinClass( "bitmap",     &ui_bitmap_factory      );

    // extended listbox classes
    RegisterWinClass( "joinlist",   &ui_joinlist_factory    );
    RegisterWinClass( "playerlist", &ui_playerlist_factory  );
    RegisterWinClass( "friendlist", &ui_friendlist_factory  );
    RegisterWinClass( "maplist",    &ui_maplist_factory     );


//=--  Register the default dialog classes
    //RegisterDialogClass( "ui_combolist",     (dialog_tem*)0, &ui_dlg_combolist_factory );
    RegisterDialogClass( "ui_vkeyboard",     (dialog_tem*)0, &ui_dlg_vkeyboard_factory );
    RegisterDialogClass( "ui_tabbed_dialog", (dialog_tem*)0, &ui_tabbed_dialog_factory );
    ui_dlg_list_register( this );

    // set the button colors to some initial values
    ui_button::SetTextColorNormal       (xcolor(150,150,150,255));   // light grey
    ui_button::SetTextColorHightlight   (XCOLOR_WHITE);
    ui_button::SetTextColorDisabled     (XCOLOR_GREY);
    ui_button::SetTextColorShadow       (XCOLOR_BLACK);

    // set default dialog colors 
    ui_dialog::SetTextColorNormal       (XCOLOR_WHITE);
    ui_dialog::SetTextColorShadow       (XCOLOR_BLACK);

    // Set capacity of the bitmap pointer xarray
    m_Bitmaps.SetCapacity( 32 );

    s32 MemoryStart = x_MemGetFree();

    //-- Load String Table.
    g_StringTableMgr.LoadTable( "ui", "ENG_ui_strings.stringbin" );

    // load scan strings
    g_StringTableMgr.LoadTable( "scan", "ENG_character_scan_strings.stringbin" );

    // load lore_ingame strings
    g_StringTableMgr.LoadTable( "lore_ingame", "ENG_ingame_lore_strings.stringbin");

    //-- Load Elements
    (void)m_ButtonTexturesNames;

    //-- Fonts
    LoadFont        ( "large",          PRELOAD_FILE("UI_A51FontLarge.xbmp"  ) ); // PRELOAD_FILE("UI_A51FontLarge.font"  )  
    LoadFont        ( "small",          PRELOAD_FILE("UI_A51FontLegal.xbmp"  ) ); // PRELOAD_FILE("UI_A51FontLegal.font"  ) 
    LoadFont        ( "hudnum",         PRELOAD_FILE("UI_A51FontHUD.xbmp"    ) ); // PRELOAD_FILE("UI_A51FontHUD.font"    )
    LoadFont        ( "loadscr",        PRELOAD_FILE("UI_A51FontLoadscr.xbmp") ); // PRELOAD_FILE("UI_A51FontLoadscr.font")

    //-- Frame
    LoadElement     ( "frame",          PRELOAD_FILE("UI_frame1.xbmp"),                2, 3, 3 );
    LoadElement     ( "frame2",         PRELOAD_FILE("UI_frame2.xbmp"),                1, 3, 3 );
    LoadElement     ( "glow",           PRELOAD_FILE("UI_barglow.xbmp"),               1, 1, 1 );
    //-- Highlight
    LoadElement     ( "highlight",      PRELOAD_FILE("UI_highlight.xbmp"),             1, 3, 3 );
    LoadElement     ( "screenglow",     PRELOAD_FILE("UI_screenglow.xbmp"),            1, 3, 3 );
    //-- Button
    LoadElement     ( "button",         PRELOAD_FILE("UI_uibutton.xbmp"),              5, 3, 1 );
    //-- Check Box
    LoadElement     ( "button_check",   PRELOAD_FILE("UI_checkbox.xbmp"),              5, 1, 1 );
    //-- Listbox
    LoadElement     ( "sb_frame",       PRELOAD_FILE("UI_frame2.xbmp"),                1, 3, 3 );
    LoadElement     ( "sb_arrowdown",   PRELOAD_FILE("UI_downarrow.xbmp"),             5, 1, 1 );
    LoadElement     ( "sb_arrowup",     PRELOAD_FILE("UI_uparrow.xbmp"),               5, 1, 1 );
    LoadElement     ( "sb_container",   PRELOAD_FILE("UI_container.xbmp"),             5, 3, 3 );
    LoadElement     ( "sb_thumb",       PRELOAD_FILE("UI_thumb.xbmp"),                 5, 1, 3 );
    //-- Slider
    LoadElement     ( "slider_bar",     PRELOAD_FILE("UI_slidercontainer.xbmp"),       1, 3, 1 );
    LoadElement     ( "slider_thumb",   PRELOAD_FILE("UI_sliderthumb.xbmp"),           5, 1, 1 );
    //-- Combo Box
    LoadElement     ( "button_combo1",  PRELOAD_FILE("UI_combobox.xbmp"),              5, 3, 1 );
    LoadElement     ( "button_combo2",  PRELOAD_FILE("UI_combobox_128.xbmp"),          1, 3, 1 );
    //-- Edit Box
    LoadElement     ( "button_edit",    PRELOAD_FILE("UI_editbox.xbmp"),               5, 3, 1 );

    //-- Presence Icons
#ifdef TARGET_PC //DUMMY
    LoadBitmap      ( "icon_friend",            PRELOAD_PS2_FILE("UI_PS2_Icon_Friend.xbmp"              ) );
    LoadBitmap      ( "icon_voice_on",          PRELOAD_PS2_FILE("UI_PS2_Icon_Voice_On.xbmp"            ) );
    LoadBitmap      ( "icon_voice_muted",       PRELOAD_PS2_FILE("UI_PS2_Icon_Voice_Muted.xbmp"         ) );
    LoadBitmap      ( "icon_voice_thru_tv",     PRELOAD_PS2_FILE("UI_PS2_Icon_Voice_Thru_TV.xbmp"       ) );
    LoadBitmap      ( "icon_friend_req_sent",   PRELOAD_PS2_FILE("UI_PS2_Icon_Friend_Req_Sent.xbmp"     ) );
    LoadBitmap      ( "icon_friend_req_rcvd",   PRELOAD_PS2_FILE("UI_PS2_Icon_Friend_Req_Rcvd.xbmp"     ) );
    LoadBitmap      ( "icon_invite_sent",       PRELOAD_PS2_FILE("UI_PS2_Icon_Invite_Sent.xbmp"         ) );
    LoadBitmap      ( "icon_invite_rcvd",       PRELOAD_PS2_FILE("UI_PS2_Icon_Invite_Rcvd.xbmp"         ) );
    LoadBitmap      ( "icon_voice_speaking",    PRELOAD_PS2_FILE("UI_PS2_Icon_Voice_Speaking.xbmp"      ) );
    LoadBitmap      ( "gamespy_logo",           PRELOAD_PS2_FILE("UI_PS2_GameSpy_Logo.xbmp"             ) );
#else
#endif

#ifdef TARGET_PC //DUMMY
    for (s32 i=0;i<NUM_BUTTON_TEXTURES;i++)
    {
        m_ButtonTextures[i].SetName(m_ButtonTexturesNames[i]);
    }    
#endif

    s32 MemoryBudget = MemoryStart - x_MemGetFree();

    //-- Create a user
    irect r;
    s32 width,height;
    eng_GetRes(width,height);
    r.Set( 0, 0, width,height );

    g_UiUserID = CreateUser( -1, r );
    EnableUser(g_UiUserID,FALSE);
    ASSERT( g_UiUserID );

    // set scaling factors based on resolution
    SetRes();

    // Allow processing of user input
    m_EnableUserInput = TRUE;
    m_ActiveController = 0;

    // Disable debugging aids
    m_RenderSafeArea = FALSE;

    // set scaling flag
    m_isScaling         = FALSE;

    // initialize wipe
    m_wipeActive        = FALSE;
    m_EnableBackground  = TRUE;

    // initialize screen highlight
    InitScreenHighlight();

    // Init ClipStack
    m_ClipStack.SetCapacity( 32 );
    m_ClipStack.SetLocked( TRUE );

    m_GlowID = -255;

#ifdef TARGET_PC
    d3deng_RegisterFrameStage( s_UIFrameStage );
#endif

    return( MemoryBudget );
}

//=========================================================================

void ui_manager::Kill( void )
{
#ifdef TARGET_PC
    d3deng_UnregisterFrameStage( s_UIFrameStage );
#endif
	
    //-- Destroy Strings
    g_StringTableMgr.UnloadTable( "ui" );

    // unload string table
    g_StringTableMgr.UnloadTable( "scan" );

    // unload lore_ingame strings
    g_StringTableMgr.UnloadTable( "lore_ingame" );

    // Destroy Users
    while( m_Users.GetCount() > 0 )
    {
        user*   pUser = m_Users[0];

        // Destroy Dialog Stack
        while( pUser->DialogStack.GetCount() > 0 )
        {
            ui_dialog* pDialog = pUser->DialogStack[0];

            pDialog->Destroy();
            delete pDialog;
            pUser->DialogStack.Delete( 0 );
        }
        pUser->DialogStack.FreeExtra();

        // Destroy User
        delete pUser;
        m_Users.Delete( 0 );
    }

    // Destroy Fonts
    while( m_Fonts.GetCount() > 0 )
    {
        m_Fonts[0]->pFont->Kill();
        delete m_Fonts[0]->pFont;
        delete m_Fonts[0];
        m_Fonts.Delete( 0 );
    }

    // Destroy Elements
    while( m_Elements.GetCount() > 0 )
    {
        //vram_Unregister( m_Elements[0]->Bitmap );
        m_Elements[0]->Bitmap.Destroy();
        delete m_Elements[0];
        m_Elements.Delete( 0 );
    }

    // Destroy Backgrounds
    while( m_Backgrounds.GetCount() > 0 )
    {
        //vram_Unregister( m_Backgrounds[0]->Bitmap );
        m_Backgrounds[0]->Bitmap.Destroy();
        delete m_Backgrounds[0];
        m_Backgrounds.Delete( 0 );
    }

    // Destroy Window Classes
    m_WindowClasses.Delete( 0, m_WindowClasses.GetCount() );

    // Destroy Dialog Classes
    m_DialogClasses.Delete( 0, m_DialogClasses.GetCount() );
}

//=========================================================================

s32 ui_manager::LoadBackground( const char* pName, const char* pPathName )
{
    // Check if background already exists
    {
        s32 ID = FindBackground( pName );
        if( ID != -1 )
            return ID;
    }

    // Create new background
    background* pBackground = new background;
    ASSERT( pBackground );

    // Set data
    pBackground->Name = pName;
    pBackground->BitmapName = pPathName;

    // Load the bitmap
    pBackground->Bitmap.SetName( pPathName );

    // Add background to array and return ID
    m_Backgrounds.Append() = pBackground;
    return m_Backgrounds.GetCount()-1;
}

//=========================================================================

void ui_manager::UnloadBackground( const char* pName )
{
    s32 ID = FindBackground( pName );
    
    if( ID == -1 )
        return;

    g_RscMgr.Unload( m_Backgrounds[ID]->BitmapName );
    m_Backgrounds[ID]->Bitmap.Destroy();
    delete m_Backgrounds[ID];
    m_Backgrounds.Delete( ID );
}


//=========================================================================

s32 ui_manager::FindBackground( const char* pName ) const
{
    s32 iFound = -1;
    s32 i;

    for( i=0 ; i<m_Backgrounds.GetCount() ; i++ )
    {
        if( m_Backgrounds[i]->Name == pName )
        {
            iFound = i;
            break;
        }
    }

    return iFound;
}


//=========================================================================

void ui_manager::RenderBackground( const char* pName ) const
{
    s32 Width, Height;
    eng_GetRes(Width, Height);

    // if we're not to show the background, then render a rectangle
    if( m_EnableBackground==FALSE )
    {
        irect r;

        r.Set( 0, 0, Width, Height );
        RenderRect( r, XCOLOR_BLACK, FALSE );
        return;
    }
    s32 iBackground = FindBackground( pName );
    if( iBackground == -1 )
        return;

    background* pBackground = m_Backgrounds[iBackground];
    ASSERT( pBackground );

    xbitmap *pBitmap;

    eng_GetRes(Width, Height);

    pBitmap = pBackground->Bitmap.GetPointer();
    if( pBitmap )
    {
        draw_Begin( DRAW_SPRITES, DRAW_2D|DRAW_TEXTURED|DRAW_NO_ZBUFFER|DRAW_UV_CLAMP|DRAW_CULL_NONE|DRAW_UI_RTARGET );
        draw_SetTexture( *pBitmap );

        f32 S = 1.0f;
        f32 T = 1.0f;
        f32 Y = 0.0f;

        draw_SpriteUV(vector3(0.0f,Y,0.0f),vector2((f32)Width, (f32)Height), vector2(0,0), vector2( S,T ), XCOLOR_WHITE );
        draw_End();
    }
}


//=========================================================================

s32 ui_manager::LoadBitmap( const char* pName, const char* pPathName )
{
    // Check if bitmap already exists
    {
        s32 ID = FindBitmap( pName );
        if( ID != -1 )
            return ID;
    }

    // Create new background
    bitmap* pBitmap = new bitmap;
    ASSERT( pBitmap );

    // Set data
    pBitmap->Name = pName;
    pBitmap->BitmapName = pPathName;

    // Load the bitmap
    pBitmap->Bitmap.SetName( pPathName );

    // Add background to array and return ID
    m_Bitmaps.Append() = pBitmap;
    return m_Bitmaps.GetCount()-1;
}

//=========================================================================

void ui_manager::UnloadBitmap( const char* pName )
{
    s32 ID = FindBitmap( pName );
    
    if( ID == -1 )
        return;

    g_RscMgr.Unload( m_Bitmaps[ID]->BitmapName );
    m_Bitmaps[ID]->Bitmap.Destroy();
    delete m_Bitmaps[ID];
    m_Bitmaps.Delete( ID );
}

//=========================================================================

s32 ui_manager::FindBitmap( const char* pName )
{
    s32 iFound = -1;
    s32 i;

    for( i=0; i<m_Bitmaps.GetCount(); i++ )
    {
        if( m_Bitmaps[i]->Name == pName )
        {
            iFound = i;
            break;
        }
    }

    return iFound;
}

//=========================================================================

void ui_manager::RenderBitmap( s32 iBitmap, const irect& Position, xcolor Color ) const
{
    bitmap* pBitmap = m_Bitmaps[iBitmap];
    ASSERT( pBitmap );
    xbitmap *pXBitmap = pBitmap->Bitmap.GetPointer();

    vector3     p(0.0f, 0.0f, 0.5f );
    p.GetX() = (f32)Position.l;
    p.GetY() = (f32)Position.t;

    vector2     wh( (f32)(Position.r - Position.l), (f32)(Position.b - Position.t));

    // If we have a bitmap, render it. If not, just render a gouraud rect.
    if( pXBitmap )
    {
        draw_Begin( DRAW_SPRITES, DRAW_2D|DRAW_TEXTURED|DRAW_USE_ALPHA|DRAW_NO_ZBUFFER|DRAW_UV_CLAMP|DRAW_CULL_NONE|DRAW_UI_RTARGET );
        draw_SetTexture( *pXBitmap );   
        draw_DisableBilinear(); 
        draw_SpriteUV( p, wh, vector2(0,0), vector2( 1.0f, 1.0f ), Color );
        draw_End();
    }
    else
    {
        static xcolor c1 (146, 226, 100,  64);
        static xcolor c2 (146, 226, 100,   0);

        irect rb;

        rb = Position;

        RenderGouraudRect( Position, c1, c2, c2, c1, FALSE );
    }
}

//=========================================================================
void ui_manager::RenderBitmapUV( s32 iBitmap, const irect& Position, const vector2& UV0In, const vector2& UV1In, xcolor Color ) const
{
    vector2 UV0 = UV0In;
    vector2 UV1 = UV1In;

    bitmap* pBitmap = m_Bitmaps[iBitmap];
    ASSERT( pBitmap );
    xbitmap *pXBitmap = pBitmap->Bitmap.GetPointer();
    if( !pXBitmap )
        return;

    vector3 p(0.0f, 0.0f, 0.5f );
    p.GetX() = (f32)Position.l;
    p.GetY() = (f32)Position.t;

    vector2 wh( (f32)(Position.r - Position.l), (f32)(Position.b - Position.t));

    // If we have a bitmap, render it. If not, just render a gouraud rect.
    if( pXBitmap )
    {
        draw_Begin( DRAW_SPRITES, DRAW_2D|DRAW_TEXTURED|DRAW_USE_ALPHA|DRAW_NO_ZBUFFER|DRAW_UV_CLAMP|DRAW_CULL_NONE|DRAW_UI_RTARGET );
        draw_SetTexture( *pXBitmap );   
        draw_DisableBilinear(); 
        draw_SpriteUV( p, wh, UV0, UV1, Color );
        draw_End();
    }
    else
    {
        static xcolor c1 (146, 226, 100,  64);
        static xcolor c2 (146, 226, 100,   0);

        irect rb;

        rb = Position;

        RenderGouraudRect( Position, c1, c2, c2, c1, FALSE );
    }
}

//=========================================================================

s32 ui_manager::LoadElement( const char* pName, const char* pPathName, s32 nStates, s32 cx, s32 cy )
{
    element*    pElement;
    xarray<s32> x;
    xarray<s32> y;
    s32         i;
    s32         ix;
    s32         iy;
    xcolor      RegColor;
    xbitmap*    pBitmap;

    // Check if element already exists
    {
        s32 ID = FindElement( pName );
        if( ID != -1 )
            return ID;
    }

    // Create new element
    pElement = new element;
    ASSERT( pElement );

    // Set data
    pElement->Name      = pName;
    pElement->nStates   = nStates;
    pElement->cx        = cx;
    pElement->cy        = cy;

    // Load the bitmap
    //VERIFYS( pElement->Bitmap.Load( pPathName ),xfs("%s",pPathName) );
    pElement->Bitmap.SetName(pPathName);
    pBitmap = pElement->Bitmap.GetPointer();

    if( (nStates > 0) && (cx > 0) && (cy > 0) )
    {
        ASSERT( (cx==1) || (cx==3) );
        ASSERT( (cy==1) || (cy==3) );

        // Pick out registration mark color
        RegColor = pBitmap->GetPixelColor( 0, 0 );

        // Find the registration markers
        x.SetCapacity( cx+1 );
        y.SetCapacity( (cy*nStates)+1 );
        for( i=0 ; i<pBitmap->GetWidth() ; i++ )
        {
            if( pBitmap->GetPixelColor( i, 0 ) == RegColor )
                x.Append() = i;
        }
        for( i=0 ; i<pBitmap->GetHeight() ; i++ )
        {
            if( pBitmap->GetPixelColor( 0, i ) == RegColor )
                y.Append() = i;
        }

        ASSERT( x.GetCount() == (cx+1) );
        ASSERT( y.GetCount() == ((cy*nStates)+1) );

        // Setup the rectangles
        pElement->r.SetCapacity( cx*cy*nStates );
        for( iy=0 ; iy<(cy*nStates) ; iy++ )
        {
            for( ix=0 ; ix<cx ; ix++ )
            {
                pElement->r.Append().Set( x[ix]+1, y[iy]+1, (x[ix+1]), (y[iy+1]) );
            }
        }
    }

    // Register the bitmap for VRAM
    //vram_Register( pElement->Bitmap );

    // Add element to array and return ID
    m_Elements.Append() = pElement;
    return m_Elements.GetCount()-1;
}

//=========================================================================

s32 ui_manager::FindElement( const char* pName ) const
{
    s32 i;

    for( i=0 ; i<m_Elements.GetCount() ; i++ )
    {
        if( m_Elements[i]->Name == pName )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

void ui_manager::RenderElement( s32 iElement, const irect& Position, s32 State, const xcolor& Color, xbool IsAdditive ) const
{
    xbool       ScaleX = FALSE;                                                       
    xbool       ScaleY = FALSE;
    s32         ix;
    s32         iy;
    s32         ie;
    vector2     p(0.0f, 0.0f);
    vector2     wh;
    vector2     uv0;
    vector2     uv1;
    element*    pElement;
    xbitmap*    pBitmap;

    ASSERT( (iElement >= 0) && (iElement < m_Elements.GetCount()) );

    // Get Element pointer
    pElement = m_Elements[iElement];

    // Use passed color instead of forced white
    xcolor RenderColor = Color;

    // Validate arguments
    ASSERT( (State >= 0) && (State < pElement->nStates) );

    // Determine what type we are, scaled horizontal, vertical, or both
    if( Position.GetWidth()  != 0 ) ScaleX = TRUE;
    if( Position.GetHeight() != 0 ) ScaleY = TRUE;

    // Setup draw flags
    u32 Flags = DRAW_2D|DRAW_TEXTURED|DRAW_USE_ALPHA|DRAW_NO_ZBUFFER|DRAW_UV_CLAMP|DRAW_CULL_NONE|DRAW_UI_RTARGET;
    
    // Add blend mode if needed
    if( IsAdditive )
        Flags |= DRAW_BLEND_ADD;

    // Begin drawing with proper flags
    draw_Begin( DRAW_SPRITES, Flags );

    // get a pointer to the texture
    pBitmap = pElement->Bitmap.GetPointer();
    draw_SetTexture( *pBitmap );

    // Disable Bilinear
    draw_DisableBilinear(); 

    // Render all the parts of the element
    ie  = State * (pElement->cx*pElement->cy);

    // Center single-tile elements within postion
    if( pElement->cy == 1 )
        p.Y = (f32)Position.t + (f32)(Position.GetHeight() - pElement->r[ie].GetHeight()) / 2.0f;
    else
        p.Y = (f32)Position.t;

    // Loop on y
    for( iy=0 ; iy<pElement->cy ; iy++ )
    {
        // Reset x position
        if( pElement->cx == 1 )
            p.X = (f32)Position.l + (f32)(Position.GetWidth() - pElement->r[ie].GetWidth()) / 2.0f;
        else
            p.X = (f32)Position.l;

        // Set Height
        if( (pElement->cy == 3) && (iy == 1) )
        {
            wh.Y = (f32)Position.GetHeight() - ((f32)pElement->r[2*pElement->cx].GetHeight() + (f32)pElement->r[0].GetHeight());
        }
        else
        {
            wh.Y = (f32)pElement->r[ie].GetHeight();
        }

        // Loop on x
        for( ix=0 ; ix<pElement->cx ; ix++ )
        {
            // Calculate UVs
            uv0.X = ((f32)pElement->r[ie].l + 0.5f) / pBitmap->GetWidth();
            uv0.Y = ((f32)pElement->r[ie].t + 0.5f) / pBitmap->GetHeight();
            uv1.X = ((f32)pElement->r[ie].r - 0.5f) / pBitmap->GetWidth();
            uv1.Y = ((f32)pElement->r[ie].b - 0.5f) / pBitmap->GetHeight();

            // Set Width
            if( (pElement->cx == 3) && (ix == 1) )
            {
                wh.X = (f32)Position.GetWidth() - ((f32)pElement->r[2].GetWidth() + (f32)pElement->r[0].GetWidth());
            }
            else
            {
                wh.X = (f32)pElement->r[ie].GetWidth();
            }

            // Draw sprite
            draw_SpriteUV( vector3( p.X, p.Y, 0.0f ), wh, uv0, uv1, RenderColor );

            // Advance position on x
            p.X += wh.X;
            // Advance index to element
            ie++;
        }
        // Advance position on y
        p.Y += wh.Y;
    }

    // End drawing
    draw_End();

    // Enable Bilinear
    draw_EnableBilinear();  
}

//=========================================================================

void ui_manager::RenderElementUV( s32 iElement, const irect& Position, const irect& UV, const xcolor& Color, xbool IsAdditive ) const
{
    vector3     p(0.0f, 0.0f, 0.5f );
    vector2     wh;
    vector2     uv0;
    vector2     uv1;
    element*    pElement;
    xbitmap*    pBitmap;

    ASSERT( (iElement >= 0) && (iElement < m_Elements.GetCount()) );

    // Get Element pointer
    pElement = m_Elements[iElement];

    // Setup draw flags
    u32 Flags = DRAW_2D|DRAW_TEXTURED|DRAW_USE_ALPHA|DRAW_NO_ZBUFFER|DRAW_CULL_NONE|DRAW_UI_RTARGET;
    
    // Add blend mode if needed
    if( IsAdditive )
        Flags |= DRAW_BLEND_ADD;

    // Begin drawing
    draw_Begin( DRAW_SPRITES, Flags );

    // Get bitmap pointer
    pBitmap = pElement->Bitmap.GetPointer();
    draw_SetTexture( *pBitmap );

    // Disable Bilinear
    draw_DisableBilinear(); 

    // Calculate Position and Size
    p.GetX() = (f32)Position.l;
    p.GetY() = (f32)Position.t;
    wh.X     = (f32)Position.GetWidth();
    wh.Y     = (f32)Position.GetHeight();

    // Calculate UVs
    uv0.X = (UV.l + 0.5f) / pBitmap->GetWidth();
    uv0.Y = (UV.t + 0.5f) / pBitmap->GetHeight();
    uv1.X = (UV.r + 0.5f) / pBitmap->GetWidth();
    uv1.Y = (UV.b + 0.5f) / pBitmap->GetHeight();

    // Draw sprite
    draw_SpriteUV( p, wh, uv0, uv1, Color );

    // End drawing
    draw_End();

    // Enable Bilinear
    draw_EnableBilinear();  
}

//=========================================================================

void ui_manager::RenderElementUV( s32 iElement, const irect& Position, const vector2& UV0, const vector2& UV1, const xcolor& Color, xbool IsAdditive ) const
{
    vector3     p(0.0f, 0.0f, 0.5f );
    vector2     wh;
    element*    pElement;
    xbitmap*    pBitmap;

    ASSERT( (iElement >= 0) && (iElement < m_Elements.GetCount()) );

    // Get Element pointer
    pElement = m_Elements[iElement];

    // Setup draw flags
    u32 Flags = DRAW_2D|DRAW_TEXTURED|DRAW_USE_ALPHA|DRAW_NO_ZBUFFER|DRAW_UV_CLAMP|DRAW_CULL_NONE|DRAW_UI_RTARGET;
    
    // Add blend mode if needed
    if( IsAdditive )
        Flags |= DRAW_BLEND_ADD;

    // Begin drawing
    draw_Begin( DRAW_SPRITES, Flags );

    // Get bitmap pointer
    pBitmap = pElement->Bitmap.GetPointer();
    draw_SetTexture( *pBitmap );

    // Disable Bilinear
    draw_DisableBilinear(); 

    // Calculate Position and Size
    p.GetX() = (f32)Position.l;
    p.GetY() = (f32)Position.t;
    wh.X     = (f32)Position.GetWidth();
    wh.Y     = (f32)Position.GetHeight();

    // Draw sprite
    draw_SpriteUV( p, wh, UV0, UV1, Color );

    // End drawing
    draw_End();

    // Enable Bilinear
    draw_EnableBilinear();  
}

//=========================================================================

const ui_manager::element* ui_manager::GetElement( s32 iElement ) const
{
    ASSERT( (iElement >= 0) && (iElement < m_Elements.GetCount()) );

    return m_Elements[iElement];
}

//=========================================================================

s32 ui_manager::LoadFont( const char* pName, const char* pPathName )
{
    // Check if font already exists
    {
        s32 ID = FindFont( pName );
        if( ID != -1 )
            return ID;
    }

    // Create font record
    font* pFont = new font;
    ASSERT( pFont );

    // Setup Font
    pFont->Name = pName;

    // Create and load font
    pFont->pFont = new ui_font;
    ASSERT( pFont->pFont );
    VERIFY( pFont->pFont->Load( pPathName ) );

    // Add to array of fonts
    m_Fonts.Append() = pFont;

    // Return Font ID
    return m_Fonts.GetCount()-1;
}

//=========================================================================

s32 ui_manager::FindFont( const char* pName ) const
{
    s32 iFound = -1;
    s32 i;

    for( i=0 ; i<m_Fonts.GetCount() ; i++ )
    {
        if( m_Fonts[i]->Name == pName )
        {
            iFound = i;
            break;
        }
    }

    return( iFound );
}

//=========================================================================
ui_font* ui_manager::GetFont( const char* pName ) const
{
    s32 iFound = -1;
    s32 i;
    
    for( i=0 ; i<m_Fonts.GetCount() ; i++ )
    {
        if( m_Fonts[i]->Name == pName )
        {
            iFound = i;
            break;
        }
    }

    return( m_Fonts[iFound]->pFont );

}

//=========================================================================

void ui_manager::RenderText( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color, const char* pString, xbool IgnoreEmbeddedColor,  xbool UseGradient, f32 FlareAmount ) const
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;

    pFont->RenderText( Position, Flags, Color, pString, IgnoreEmbeddedColor, UseGradient, FlareAmount );
}

//=========================================================================

void ui_manager::RenderText( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color, const xwchar* pString, xbool IgnoreEmbeddedColor,  xbool UseGradient, f32 FlareAmount ) const
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;

    pFont->RenderText( Position, Flags, Color, pString, IgnoreEmbeddedColor, UseGradient, FlareAmount );
}

//=========================================================================
void ui_manager::RenderText_Wrap( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color,  const xwstring& Text, xbool IgnoreEmbeddedColor,  xbool UseGradient, f32 FlareAmount )
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;

    xwstring wrapped;
    WordWrapString(iFont, Position, Text, wrapped );

    pFont->RenderText( Position, Flags, Color, wrapped, IgnoreEmbeddedColor, UseGradient, FlareAmount );
}

//=========================================================================

s32 ui_manager::TextWidth( s32 iFont, const xwchar* pString, s32 Count ) const
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;

    return( pFont->TextWidth( pString, Count ) );
}

//=========================================================================

s32 ui_manager::TextHeight( s32 iFont, const xwchar* pString, s32 Count ) const
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;

    return( pFont->TextHeight( pString, Count ) );
}

//=========================================================================

void ui_manager::TextSize( s32 iFont, irect& Rect, const xwchar* pString, s32 Count ) const
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;

    pFont->TextSize( Rect, pString, Count );
}

//=========================================================================

s32 ui_manager::GetLineHeight( s32 iFont ) const
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;

    return pFont->GetLineHeight();
}

//=========================================================================

void ui_manager::RenderRect( const irect& r, const xcolor& Color, xbool IsWire ) const
{
    draw_Rect( r, Color, IsWire, DRAW_UI_RTARGET );
}

//=========================================================================

void ui_manager::RenderGouraudRect( const irect& r, const xcolor& c1, const xcolor& c2, const xcolor& c3, const xcolor& c4, xbool IsWire, xbool IsAdditive ) const
{
    s32 drawFlags = DRAW_UI_RTARGET | (IsAdditive ? DRAW_BLEND_ADD : 0);
    draw_GouraudRect( r, c1, c2, c3, c4, IsWire, drawFlags );
}

//=========================================================================

xbool ui_manager::RegisterWinClass ( const char* ClassName, ui_pfn_winfact pFactory )
{
    xbool   Success = FALSE;
    s32     iFound = -1;
    s32     i;

    // Find the winclass entry
    for( i=0 ; i<m_WindowClasses.GetCount() ; i++ )
    {
        if( m_WindowClasses[i].ClassName == ClassName )
        {
            iFound = i;
        }
    }

    // If not found then add a new one
    if( iFound == -1 )
    {
        winclass& wc = m_WindowClasses.Append();
        wc.ClassName = ClassName;
        wc.pFactory  = pFactory;
        Success = TRUE;
    }

    // Return success code
    return Success;
}

//=========================================================================

ui_win* ui_manager::CreateWin( s32 UserID, const char* ClassName, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_win*         pWin        = NULL;
    ui_pfn_winfact  pFactory    = NULL;
    s32             i;

    // Find the winclass entry
    for( i=0 ; i<m_WindowClasses.GetCount() ; i++ )
    {
        if( m_WindowClasses[i].ClassName == ClassName )
        {
            pFactory = m_WindowClasses[i].pFactory;
        }
    }

    // If we have a factory function then continue
    if( pFactory )
    {
        pWin = pFactory( UserID, this, Position, pParent, Flags );
    }

    // Return pointer to new window
    return pWin;
}

//=========================================================================

s32 ui_manager::CreateUser( s32 ControllerID, const irect& Bounds, s32 Data )
{
    // Create new user struct
    user*   pUser = new user;
    ASSERT( pUser );
    if( pUser )
    {
        s32     i;

        // Fill out the struct
        pUser->Enabled                  = TRUE;
        pUser->ControllerID             = ControllerID;
        pUser->Bounds                   = Bounds;
        pUser->Data                     = Data;
        pUser->Height                   = 0;
        pUser->pCaptureWindow           = 0;
        pUser->pLastWindowUnderCursor   = 0;
        pUser->iHighlightElement        = FindElement( "highlight" );
        ASSERT( pUser->iHighlightElement );

        pUser->CursorVisible            = TRUE;
        pUser->MouseActive              = FALSE;
        pUser->CursorX                  = Bounds.GetWidth()/2  + Bounds.l;
        pUser->CursorY                  = Bounds.GetHeight()/2 + Bounds.t;
        pUser->LastCursorX              = Bounds.GetWidth()/2  + Bounds.l;
        pUser->LastCursorY              = Bounds.GetHeight()/2 + Bounds.t;

        // Set Analog Scalers
        for( i=0 ; i<SM_MAX_PLAYERS ; i++ ) // This may not be the best define for this, but it's better than hard numbers! 
        {
            //pUser->DPadUp[i]       .SetupRepeat( 0.200f, 0.060f );
            //pUser->DPadDown[i]     .SetupRepeat( 0.200f, 0.060f );
            //pUser->DPadLeft[i]     .SetupRepeat( 0.200f, 0.060f );
            //pUser->DPadRight[i]    .SetupRepeat( 0.200f, 0.060f );
            //pUser->PadSelect[i]    .SetupRepeat( 0.200f, 0.060f );
            //pUser->PadBack[i]      .SetupRepeat( 0.200f, 0.060f );
            //pUser->PadDelete[i]    .SetupRepeat( 0.200f, 0.060f );
            //pUser->PadActivate[i]  .SetupRepeat( 0.200f, 0.060f );
        }
        static const s32 MAX_DIALOGS_EVER = 20;
        pUser->DialogStack.SetCapacity( MAX_DIALOGS_EVER ); // Plenty of room

        // Add an entry to the users list
        m_Users.Append() = pUser;
    }

    return (s32)pUser;
}

//=========================================================================

void ui_manager::DeleteAllUsers( void )
{
    while( m_Users.GetCount() > 0 )
    {
        DeleteUser( (s32)m_Users[0] );
    }
}

//=========================================================================

void ui_manager::DeleteUser( s32 UserID )
{
    s32     Index;

    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    // Find the users index
    Index = m_Users.Find( (user*)UserID );
    ASSERT( Index != -1 );

    // Close all the dialog that may be open
    while( m_Users[Index]->DialogStack.GetCount() > 0 )
    {
        s32 i = m_Users[Index]->DialogStack.GetCount()-1;
        m_Users[Index]->DialogStack[i]->Destroy();
        m_Users[Index]->DialogStack.Delete( i );
    }
    m_Users[Index]->DialogStack.FreeExtra();

    // Delete the user
    delete m_Users[Index];
    m_Users.Delete( Index );
}

//=========================================================================

ui_manager::user* ui_manager::GetUser( s32 UserID ) const
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    return pUser;
}

//=========================================================================

void ui_manager::SetUserController( s32 UserID, s32 ControllerID )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    pUser->ControllerID = ControllerID;
}

//=========================================================================

s32 ui_manager::GetUserData( s32 UserID ) const
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    return pUser->Data;
}

//=========================================================================
ui_win* ui_manager::GetWindowUnderCursor( s32 UserID ) const
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    return pUser->pLastWindowUnderCursor;
}

//=========================================================================

void ui_manager::SetCursorVisible( s32 UserID, xbool State )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    pUser->CursorVisible = State;
}

//=========================================================================

xbool ui_manager::GetCursorVisible( s32 UserID ) const
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    return pUser->CursorVisible;
}

//=========================================================================

void ui_manager::SetCursorPos( s32 UserID, s32 x, s32 y )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    pUser->CursorX = x;
    pUser->CursorY = y;

    ui_win* pWin = GetWindowAtXY( pUser, x, y );

    // Has window under cursor changed?
    if( pWin != pUser->pLastWindowUnderCursor )
    {
        // Call exit function if there was a window under the cursor
        if( pUser->pLastWindowUnderCursor )
        {
            pUser->pLastWindowUnderCursor->OnCursorExit( pUser->pLastWindowUnderCursor );
        }

        // Set new window under cursor and call enter function
        pUser->pLastWindowUnderCursor = pWin;
        if( pUser->pLastWindowUnderCursor )
        {
            pUser->pLastWindowUnderCursor->OnCursorEnter( pUser->pLastWindowUnderCursor );
        }
    }

}

//=========================================================================

void ui_manager::GetCursorPos( s32 UserID, s32& x, s32& y ) const
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    x = pUser->CursorX;
    y = pUser->CursorY;
}

//=========================================================================

ui_win* ui_manager::SetCapture( s32 UserID, ui_win* pWin )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    ui_win* pOldCaptureWin = pUser->pCaptureWindow;
    pUser->pCaptureWindow = pWin;

    return pOldCaptureWin;
}

//=========================================================================

void ui_manager::ReleaseCapture( s32 UserID )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    pUser->pCaptureWindow = NULL;
}

//=========================================================================

void ui_manager::SetUserBackground( s32 UserID, const char* pName )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    m_EnableBackground = TRUE;
    user*   pUser = (user*)UserID;
    pUser->Background = pName;
}

//=========================================================================

const irect& ui_manager::GetUserBounds( s32 UserID ) const
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    return pUser->Bounds;
}

//=========================================================================

void ui_manager::EnableUser( s32 UserID, xbool State )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    pUser->Enabled = State;
}

//=========================================================================

xbool ui_manager::IsUserEnabled( s32 UserID ) const
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );

    user*   pUser = (user*)UserID;
    return pUser->Enabled;
}

//=========================================================================

void ui_manager::AddHighlight( s32 UserID, irect& r, xbool Flash )
{
    (void)UserID;
    (void)r;
    (void)Flash;
}

//=========================================================================

xbool ui_manager::ProcessInput( f32 DeltaTime )
{
    s32     i;
    xbool   Continue = TRUE;

    // disable input when screen is scaling
    if( !m_isScaling )
    {
        // Loop through each user
        for( i=0 ; i<m_Users.GetCount() ; i++ )
        {
            user* pUser = m_Users[i];
            ASSERT( pUser );

            // Only process input for enabled users
            if( pUser->Enabled )
            {
                Continue &= ProcessInput( DeltaTime, (s32)pUser );
            }
        }
    }

    return Continue;
}

//=========================================================================

ui_win* ui_manager::GetWindowAtXY( user* pUser, s32 x, s32 y )
{
    ui_win* pWindow = NULL;

    // Check if anything on dialog stack
    if( pUser->DialogStack.GetCount() > 0 )
    {
        s32     i = pUser->DialogStack.GetCount()-1;

        // Yes search from topmost dialog back
        while( (pWindow == NULL) && (i >= 0) )
        {
            ui_dialog* pDialog = pUser->DialogStack[i];
            pDialog->ScreenToLocal( x, y );
            pWindow = pDialog->GetWindowAtXY( x, y );

            // Don't select a disabled window
            if( pWindow && (pWindow->GetFlags() & ui_win::WF_DISABLED) )
                pWindow = pDialog;

            // If modal then exit, otherwise step back to next dialog
            if( pDialog->GetFlags() & ui_win::WF_INPUTMODAL )
            {
                if( pWindow == NULL )
                    pWindow = pDialog;
                break;
            }
            else
            {
                i--;
            }
        }
    }
    return pWindow;
}

//=========================================================================

#if !defined(X_RETAIL)
xbool   bInProcessInput;
#endif

xbool ui_manager::ProcessInput( f32 DeltaTime, s32 UserID )
{
    xbool   Iterate         = FALSE;
    s32     IterateCount    = 0;
    s32     StartController = 0;
    s32     EndController   = 0;

#if !defined(X_RETAIL)
    bInProcessInput = FALSE;
#endif

    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );
    user*   pUser = (user*)UserID;

    do
    {
        // Don't iterate unless set later
        if( Iterate )
        {
            IterateCount++;
            Iterate = FALSE;
        }

        // Get pointer to window to receive input
        ui_win* pWin = pUser->pCaptureWindow;

        // Update mouse cursor position
#if defined( TARGET_PC ) && !defined( X_EDITOR )
        pUser->CursorX += (s32)input_GetValue(INPUT_MOUSE_X_REL);
        pUser->CursorY += (s32)input_GetValue(INPUT_MOUSE_Y_REL);
#endif

        pUser->CursorX = MAX( pUser->CursorX, 0 );
        pUser->CursorX = MIN( pUser->CursorX, (pUser->Bounds.GetWidth()-1) );
        pUser->CursorY = MAX( pUser->CursorY, 0 );
        pUser->CursorY = MIN( pUser->CursorY, (pUser->Bounds.GetHeight()-1) );

        // Determine which window cursor is now over and call appropriate EXIT/ENTER functions
        if( pWin == NULL )
        {
            ui_win* pWindowUnderCursor = GetWindowAtXY( pUser, pUser->CursorX, pUser->CursorY );

            // Has window under cursor changed?
            if( pWindowUnderCursor != pUser->pLastWindowUnderCursor )
            {
                // Call exit function if there was a window under the cursor
                if( pUser->pLastWindowUnderCursor )
                {
                    pUser->pLastWindowUnderCursor->OnCursorExit( pUser->pLastWindowUnderCursor );
                }

                // Set new window under cursor and call enter function
                pUser->pLastWindowUnderCursor = pWindowUnderCursor;
                if( pUser->pLastWindowUnderCursor )
                {
                    pUser->pLastWindowUnderCursor->OnCursorEnter( pUser->pLastWindowUnderCursor );
                }
            }

            // Set pointer to window to receive input
            pWin = pUser->pLastWindowUnderCursor;
        }

#ifdef TARGET_PC

        for( s32 i=StartController ; i<=EndController ; i++ )
        {
            if ( g_MonkeyOptions.Enabled && g_MonkeyOptions.ModeEnabled[MONKEY_MENUMONKEY] )
            {
#ifndef X_RELEASE
                //UpdateButton( pUser->DPadUp[i],         g_Monkey.GetUIButtonValue( MONKEY_UI_UP ),          DeltaTime);                    
                //UpdateButton( pUser->DPadDown[i],       g_Monkey.GetUIButtonValue( MONKEY_UI_DOWN ),        DeltaTime);
                //UpdateButton( pUser->DPadLeft[i],       g_Monkey.GetUIButtonValue( MONKEY_UI_LEFT ),        DeltaTime);
                //UpdateButton( pUser->DPadRight[i],      g_Monkey.GetUIButtonValue( MONKEY_UI_RIGHT ),       DeltaTime);
                //UpdateButton( pUser->PadSelect[i],      g_Monkey.GetUIButtonValue( MONKEY_UI_SELECT ),      DeltaTime);
                //UpdateButton( pUser->PadBack[i],        g_Monkey.GetUIButtonValue( MONKEY_UI_BACK ),        DeltaTime);
                //UpdateButton( pUser->PadDelete[i],      g_Monkey.GetUIButtonValue( MONKEY_UI_DELETE ),      DeltaTime);
                //UpdateButton( pUser->PadActivate[i],    g_Monkey.GetUIButtonValue( MONKEY_UI_ACTIVATE ),    DeltaTime);
                //UpdateButton( pUser->PadShoulderL[i],   g_Monkey.GetUIButtonValue( MONKEY_UI_SHOULDERL ),   DeltaTime);
                //UpdateButton( pUser->PadShoulderR[i],   g_Monkey.GetUIButtonValue( MONKEY_UI_SHOULDERR ),   DeltaTime);
                //UpdateButton( pUser->PadShoulderL2[i],  g_Monkey.GetUIButtonValue( MONKEY_UI_SHOULDERL2 ),  DeltaTime);
                //UpdateButton( pUser->PadShoulderR2[i],  g_Monkey.GetUIButtonValue( MONKEY_UI_SHOULDERR2 ),  DeltaTime);
                //UpdateButton( pUser->PadHelp[i],        g_Monkey.GetUIButtonValue( MONKEY_UI_HELP ),        DeltaTime);
                x_throw ("INTERVELOP!" );
#endif
            }
            else
            {
                UpdateButton( pUser->DPadUp[i],    (input_IsPressed( INPUT_KBD_UP,    i ) || input_IsPressed( INPUT_KBD_W, i )), DeltaTime );
                UpdateButton( pUser->DPadDown[i],  (input_IsPressed( INPUT_KBD_DOWN,  i ) || input_IsPressed( INPUT_KBD_S, i )), DeltaTime );
                UpdateButton( pUser->DPadLeft[i],  (input_IsPressed( INPUT_KBD_LEFT,  i ) || input_IsPressed( INPUT_KBD_A, i )), DeltaTime );
                UpdateButton( pUser->DPadRight[i], (input_IsPressed( INPUT_KBD_RIGHT, i ) || input_IsPressed( INPUT_KBD_D, i )), DeltaTime );
                
                UpdateButton( pUser->PadSelect[i],     input_WasPressed( INPUT_KBD_RETURN, i ), DeltaTime );
                UpdateButton( pUser->PadBack[i],       input_WasPressed( INPUT_KBD_ESCAPE, i ), DeltaTime );
                
                
                //Idk, it should be like this?
                UpdateButton( pUser->PadDelete[i], (input_WasPressed( INPUT_KBD_DELETE, i ) || input_WasPressed( INPUT_KBD_BACK, i )), DeltaTime ); //Usefull for deleting player profiles and etc.
                //TEMP!!!
                UpdateButton( pUser->PadActivate[i],   input_WasPressed( INPUT_KBD_R,   i ), DeltaTime ); //Usefull for editing player profiles and etc.                
            }
            // Keep index of last controller that pressed a select button so we can hack
            // the controller number into the players controller for 1 player games
            if( pUser->PadSelect[i].nPresses > 0 )
            {
                g_uiLastSelectController = i;
            }
        }

        // Update mouse buttons
        UpdateButton( pUser->ButtonLB, input_IsPressed( INPUT_MOUSE_BTN_L ), DeltaTime );
        UpdateButton( pUser->ButtonMB, input_IsPressed( INPUT_MOUSE_BTN_C ), DeltaTime );
        UpdateButton( pUser->ButtonRB, input_IsPressed( INPUT_MOUSE_BTN_R ), DeltaTime );
#endif

        // Only do this if there is a target window
        if( pWin )
        {
            // Issue window calls for mouse
            if( (pUser->LastCursorX != pUser->CursorX) || (pUser->LastCursorY != pUser->CursorY) )
            {
                pWin->OnCursorMove( pWin, pUser->CursorX, pUser->CursorY );
            }
            if( pUser->ButtonLB.nPresses  ) pWin->OnLBDown( pWin );
            if( pUser->ButtonLB.nReleases ) pWin->OnLBUp  ( pWin );
            if( pUser->ButtonMB.nPresses  ) pWin->OnMBDown( pWin );
            if( pUser->ButtonMB.nReleases ) pWin->OnMBUp  ( pWin );
            if( pUser->ButtonRB.nPresses  ) pWin->OnRBDown( pWin );
            if( pUser->ButtonRB.nReleases ) pWin->OnRBUp  ( pWin );

            // Sum up button presses
            s32 pDPadUp         = 0;
            s32 pDPadDown       = 0;
            s32 pDPadLeft       = 0;
            s32 pDPadRight      = 0;
            s32 rDPadUp         = 0;
            s32 rDPadDown       = 0;
            s32 rDPadLeft       = 0;
            s32 rDPadRight      = 0;
            s32 tDPadUp         = 0;
            s32 tDPadDown       = 0;
            s32 tDPadLeft       = 0;
            s32 tDPadRight      = 0;
            s32 PadSelect       = 0;
            s32 PadBack         = 0;
            s32 PadDelete       = 0;
            s32 PadActivate     = 0;            
            //s32 PadShoulderL    = 0;
            //s32 PadShoulderR    = 0;
            //s32 PadShoulderL2   = 0;
            //s32 PadShoulderR2   = 0;        
            //s32 PadHelp         = 0;               
            {
#if !defined(X_RETAIL)
                bInProcessInput = TRUE;
#endif
                s32 i;
                for( i=StartController ; i<=EndController ; i++ )
                {
                    // set active controller
                    m_ActiveController = i;

                    // check input for each controller
                    pDPadUp         = pUser->DPadUp[i].nPresses;
                    pDPadDown       = pUser->DPadDown[i].nPresses;
                    pDPadLeft       = pUser->DPadLeft[i].nPresses;
                    pDPadRight      = pUser->DPadRight[i].nPresses;
                    rDPadUp         = pUser->DPadUp[i].nRepeats;
                    rDPadDown       = pUser->DPadDown[i].nRepeats;
                    rDPadLeft       = pUser->DPadLeft[i].nRepeats;
                    rDPadRight      = pUser->DPadRight[i].nRepeats;
                    tDPadUp         = pUser->DPadUp[i].nPresses       + pUser->DPadUp[i].nRepeats;
                    tDPadDown       = pUser->DPadDown[i].nPresses     + pUser->DPadDown[i].nRepeats;
                    tDPadLeft       = pUser->DPadLeft[i].nPresses     + pUser->DPadLeft[i].nRepeats;
                    tDPadRight      = pUser->DPadRight[i].nPresses    + pUser->DPadRight[i].nRepeats;
                    PadSelect       = pUser->PadSelect[i].nPresses;
                    PadBack         = pUser->PadBack[i].nPresses;
                    PadDelete       = pUser->PadDelete[i].nPresses;
                    PadActivate     = pUser->PadActivate[i].nPresses;               
                    //PadShoulderL    = pUser->PadShoulderL[i].nPresses + pUser->PadShoulderL[i].nRepeats;
                    //PadShoulderR    = pUser->PadShoulderR[i].nPresses + pUser->PadShoulderR[i].nRepeats;
                    //PadShoulderL2   = pUser->PadShoulderL2[i].nPresses + pUser->PadShoulderL2[i].nRepeats;
                    //PadShoulderR2   = pUser->PadShoulderR2[i].nPresses + pUser->PadShoulderR2[i].nRepeats;
					//
                    //PadHelp         = pUser->PadHelp[i].nPresses;
					//
                    //pDPadUp         += pUser->LStickUp[i].nPresses;
                    //pDPadDown       += pUser->LStickDown[i].nPresses;
                    //pDPadLeft       += pUser->LStickLeft[i].nPresses;
                    //pDPadRight      += pUser->LStickRight[i].nPresses;
                    //rDPadUp         += pUser->LStickUp[i].nRepeats;
                    //rDPadDown       += pUser->LStickDown[i].nRepeats;
                    //rDPadLeft       += pUser->LStickLeft[i].nRepeats;
                    //rDPadRight      += pUser->LStickRight[i].nRepeats;
                    //tDPadUp         += pUser->LStickUp[i].nPresses    + pUser->LStickUp[i].nRepeats;
                    //tDPadDown       += pUser->LStickDown[i].nPresses  + pUser->LStickDown[i].nRepeats;
                    //tDPadLeft       += pUser->LStickLeft[i].nPresses  + pUser->LStickLeft[i].nRepeats;
                    //tDPadRight      += pUser->LStickRight[i].nPresses + pUser->LStickRight[i].nRepeats;

                    // send commands for each controller
                    s_EndDialogCount=0;
                    // Issue window calls for pad navigation
                    if( tDPadUp    ) 
                    { 
                        Iterate = TRUE; pWin->OnPadNavigate( pWin, NAV_UP,    pDPadUp,    rDPadUp,   FALSE,  TRUE ); 
                    }
                    
                    if( tDPadDown  ) 
                    { 
                        Iterate = TRUE; pWin->OnPadNavigate( pWin, NAV_DOWN,  pDPadDown,  rDPadDown, FALSE,  TRUE ); 
                    }

                    if( tDPadLeft  ) 
                    { 
                        Iterate = TRUE; pWin->OnPadNavigate( pWin, NAV_LEFT,  pDPadLeft,  rDPadLeft  ); 
                    }
                    
                    if( tDPadRight ) 
                    {
                        Iterate = TRUE; pWin->OnPadNavigate( pWin, NAV_RIGHT, pDPadRight, rDPadRight ); 
                    }

                    // Issue window calls for pad select / back / help
                    if( !Iterate && PadSelect   && !s_EndDialogCount ) 
                    { 
                        Iterate = TRUE; pWin->OnPadSelect  ( pWin ); 
                    }

                    if( !Iterate && PadBack     && !s_EndDialogCount ) 
                    { 
                        Iterate = TRUE; pWin->OnPadBack    ( pWin ); 
                    }

                    if( !Iterate && PadDelete   && !s_EndDialogCount ) 
                    { 
                        Iterate = TRUE; pWin->OnPadDelete  ( pWin ); 
                    }

                    if( !Iterate && PadActivate && !s_EndDialogCount ) 
                    { 
                        Iterate = TRUE; pWin->OnPadActivate( pWin ); 
                    }                
                    //if( !Iterate && PadHelp     && !s_EndDialogCount ) 
                    //{ 
                    //    Iterate = TRUE; pWin->OnPadHelp    ( pWin ); 
                    //}
					//
                    //// Issue window calls for pad shoulders
                    //if( PadShoulderL && !s_EndDialogCount ) 
                    //{ 
                    //    pWin->OnPadShoulder ( pWin, -1 ); 
                    //}
                    //else if( PadShoulderR && !s_EndDialogCount ) 
                    //{ 
                    //    pWin->OnPadShoulder ( pWin,  1 ); 
                    //};
					//
                    //if( PadShoulderL2 && !s_EndDialogCount ) 
                    //{ 
                    //    pWin->OnPadShoulder2( pWin, -1 ); 
                    //}
                    //else if( PadShoulderR2 && !s_EndDialogCount )
                    //{ 
                    //    pWin->OnPadShoulder2( pWin,  1 ); 
                    //}
                    s_EndDialogCount=0;
                }
#if !defined(X_RETAIL)
            bInProcessInput = FALSE;
#endif
            }
        }

        // Save Last Cursor Position for next time around
        pUser->LastCursorX = pUser->CursorX;
        pUser->LastCursorY = pUser->CursorY;

        // Clear DeltaTime in case of next iteration
        DeltaTime = 0.0f;

    } while( Iterate && !IterateCount );

    // Do Global inputs
#ifdef TARGET_PC
    if( input_IsPressed( INPUT_MSG_EXIT ) )
        return FALSE;
#endif

    // Return TRUE if not exiting
    return TRUE;
}

//=========================================================================

void ui_manager::EnableUserInput( void )
{
    m_EnableUserInput = TRUE;
}

//=========================================================================

void ui_manager::DisableUserInput( void )
{
    m_EnableUserInput = FALSE;

    for( s32 i=0 ; i<m_Users.GetCount() ; i++ )
    {
        user* pUser = m_Users[i];
        ASSERT( pUser );

        for( s32 j=0 ; j<INPUT_MAX_CONTROLLER_COUNT ; j++ )
        {
            pUser->DPadDown[j]    .Clear();
            pUser->DPadLeft[j]    .Clear();
            pUser->DPadRight[j]   .Clear();
            pUser->DPadUp[j]      .Clear();
            pUser->PadSelect[j]   .Clear();
            pUser->PadBack[j]     .Clear();   
            //pUser->LStickDown[j]  .Clear();
            //pUser->LStickLeft[j]  .Clear();
            //pUser->LStickRight[j] .Clear();
            //pUser->LStickUp[j]    .Clear();
            //pUser->PadShoulderL[j].Clear();
            //pUser->PadShoulderR[j].Clear();         
        }
    }
}

//=========================================================================

void ui_manager::Update( f32 DeltaTime )
{
    // Update AlphaTime
    m_AlphaTime += DeltaTime;
    m_AlphaTime = x_fmod( m_AlphaTime, 1.0f );

    // Update highlight alpha
    if( m_HighlightFadeUp )
    {
        if( ++m_HighlightAlpha == 32)
        {
            m_HighlightFadeUp = FALSE;
        }
    }
    else
    {
        if( --m_HighlightAlpha == 0 )
        {
            m_HighlightFadeUp = TRUE;
        }
    }

    // update the screen wipe
    UpdateScreenWipe(DeltaTime);

    // update the refresh bar
    UpdateRefreshBar(DeltaTime);

    // Loop through each user
    for( s32 i=0 ; i<m_Users.GetCount() ; i++ )
    {
        user* pUser = m_Users[i];
        ASSERT( pUser );

        // Only update enabled users
        if( pUser->Enabled )
        {
            // Update all Dialogs on Stack
            for( s32 j=0 ; j<pUser->DialogStack.GetCount() ; j++ )
            {
                pUser->DialogStack[j]->OnUpdate( pUser->DialogStack[j], DeltaTime );
            }
        }
    }
}

//=========================================================================

void ui_manager::Render( void )
{
    s32 i;

    if( eng_Begin( "UI" ) )
    {
#ifdef TARGET_PC
        CheckRes();
        xbool RenderCursor = FALSE;
#endif

        // Loop through each user to render
        for( i=0 ; i<m_Users.GetCount() ; i++ )
        {
            user* pUser = m_Users[i];
            ASSERT( pUser );

            // Only render enabled users
            if( pUser->Enabled )
            {
#ifdef TARGET_PC
                // If there are visible dialogs, render the stack
                if (pUser->DialogStack.GetCount())
                    RenderCursor = TRUE;
#endif            
                // Render Background
                RenderBackground( pUser->Background );

                // Find Topmost Render Modal Dialog
                s32 j = pUser->DialogStack.GetCount()-1;
                while( (j > 0) && !(pUser->DialogStack[j]->GetFlags() & ui_win::WF_RENDERMODAL) )
                    j--;

                // Make sure we start with a legal dialog
                if( j < 0 ) j = 0;

                // Render all Dialogs from the Render Modal one
                for( ; j<pUser->DialogStack.GetCount() ; j++ )
                {
                    pUser->DialogStack[j]->Render( pUser->Bounds.l, pUser->Bounds.t );
                }
            }
        }

#ifdef TARGET_PC
        // Only render the curosr if the dialogs are visible.
        if( RenderCursor )
        {
            irect r;
            POINT   Pos;

            // Get the last user.
            user* pUser = m_Users[m_Users.GetCount()-1];

            Pos.x = pUser->CursorX;
            Pos.y = pUser->CursorY;
            
            // Set position to draw the sprite.
            r.Set( Pos.x, Pos.y, Pos.x+m_Mouse.GetWidth(), Pos.y+m_Mouse.GetHeight() );
 
            draw_ClearL2W();
            draw_Begin( DRAW_LINES, DRAW_2D|DRAW_NO_ZBUFFER|DRAW_CULL_NONE|DRAW_UI_RTARGET );
            draw_Color( XCOLOR_WHITE );

            draw_Vertex( vector3(r.l-3,r.t-3,0) );
            draw_Vertex( vector3(r.l+4,r.t+4,0) );
            draw_Vertex( vector3(r.l-3,r.t+3,0) );
            draw_Vertex( vector3(r.l+4,r.t-4,0) );

            draw_End();
        }
#endif

        // render safe area
        if ( m_RenderSafeArea )
        {
        }

        eng_End();
    }
}

//=========================================================================

xbool ui_manager::RegisterDialogClass( const char* ClassName, dialog_tem* pDialogTem, ui_pfn_dlgfact pFactory )
{
    xbool   Success = FALSE;
    s32     iFound = -1;
    s32     i;

    // Find the winclass entry
    for( i=0 ; i<m_DialogClasses.GetCount() ; i++ )
    {
        if( m_DialogClasses[i].ClassName == ClassName )
        {
            iFound = i;
        }
    }

    // If not found then add a new one
    if( iFound == -1 )
    {
        dialogclass& dc = m_DialogClasses.Append();
        dc.ClassName  = ClassName;
        dc.pDialogTem = pDialogTem;
        dc.pFactory   = pFactory;
        Success = TRUE;
    }

    // Return success code
    return Success;
}

//=========================================================================

ui_dialog* ui_manager::OpenDialog( s32 UserID, const char* ClassName, irect Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    s32             i;
    ui_dialog*      pDialog     = NULL;
    ui_pfn_dlgfact  pFactory    = NULL;
    dialog_tem*     pDialogTem  = NULL;

    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );
    user* pUser = (user*)UserID;

    // Find the dialogclass entry
    for( i=0 ; i<m_DialogClasses.GetCount() ; i++ )
    {
        if( m_DialogClasses[i].ClassName == ClassName )
        {
            pFactory   = m_DialogClasses[i].pFactory;
            pDialogTem = m_DialogClasses[i].pDialogTem;
        }
    }

    // If Found
    if( pFactory )
    {
        // Check for centering the dialog
        if( Flags & ui_win::WF_DLG_CENTER )
        {
#ifdef TARGET_PC
//            irect    b(0,0,640,448);
            irect    b(0,0,800,600);
            Position.Translate( b.l + (b.GetWidth ()-Position.GetWidth ())/2 - Position.l,
                                b.t + (b.GetHeight()-Position.GetHeight())/2 - Position.t );
#else
            irect b = GetUserBounds( UserID );
            
            if (b.GetWidth() > Position.GetWidth())
            {
                b.Translate( -b.l, -b.t );
                Position.Translate( b.l + (b.GetWidth ()-Position.GetWidth ())/2 - Position.l,
                                b.t + (b.GetHeight()-Position.GetHeight())/2 - Position.t );
            }
#endif
        }

        // get screen resolution
        s32 XRes, YRes;
        eng_GetRes( XRes, YRes );

        if (!(Flags & ui_win::WF_USE_ABSOLUTE))
        {
            // scale the width of the dialog
            s32 X = Position.r - Position.l;
            X = (s32)( (f32)X * m_ScaleX );

            // center it
            Position.l = ( XRes - X ) / 2;
            Position.r = Position.l + X;

            // scale height of dialog
            s32 Y = Position.b - Position.t;
            Y = (s32)( (f32)Y * m_ScaleY );
            
            // Position it
            Position.t = (s32)((f32)Position.t * m_ScaleY);
            Position.t += SAFE_ZONE;
            Position.b = Position.t + Y;
            //s32 midY = YRes>>1;
            //s32 dy = midY - 224;
            //Position.Translate( 0, dy );
        }

        // Old version, repositions without scaling
        //if (!(Flags & ui_win::WF_USE_ABSOLUTE))
        //{
        //    // Adjust the position of the dialogs according to the resolution.
        //    if( (pParent == NULL)  )
        //    {
        //        s32 midX = XRes>>1;
        //        s32 midY = YRes>>1;
        //
        //        s32 dx = midX - 256;
        //        s32 dy = midY - 224;
        //
        //        Position.Translate( dx, dy );
        //    }
        //}

        irect CreatePosition = Position;        
                                    
        // Create the Dialog Window
        pDialog = (ui_dialog*)pFactory( UserID, this, pDialogTem, Position, pParent, Flags, pUserData );
        ASSERT( pDialog );

        pDialog->m_CreatePosition = CreatePosition;
        pDialog->m_XRes = XRes;
        pDialog->m_YRes = YRes;

        LOG_MESSAGE( "ui_manager::OpenDialog", "New dialog opened. ID:0x%08x, Name:%s, Position:(%d,%d,%d,%d)", pDialog, ClassName, CreatePosition.l, CreatePosition.t, CreatePosition.r, CreatePosition.b );

        // If this is not a TAB dialog page
        if( !(pDialog->GetFlags() & ui_win::WF_TAB) )
        {
            // Add to the Dialog Stack
            if( pParent == NULL )
                pUser->DialogStack.Append() = pDialog;

            // Activate the dialog if it has controls
            if( !(Flags & ui_win::WF_NO_ACTIVATE) && (pDialog->m_Children.GetCount() > 0) )
                pDialog->GotoControl( (s32)0 );
        }
    }

    // Return pointer to new dialog
    return pDialog;
}

//=========================================================================

void ui_manager::EndDialog( s32 UserID, xbool ResetCursor )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );
    user*   pUser = (user*)UserID;
    s32     Count = pUser->DialogStack.GetCount();

    // Check if there are any dialogs to end
    if( Count > 0 )
    {
        // Store the dialog to a kill stack, which will get released after the input has been processed.
//        m_KillDialogStack.Append() = pUser->DialogStack[Count-1];

        // Get dialog pointer
        ui_dialog* pDialog = pUser->DialogStack[Count-1];

        // Reset the cursor
        if( ResetCursor )
        {
            pUser->CursorX = pDialog->m_OldCursorX;
            pUser->CursorY = pDialog->m_OldCursorY;
        }

        // Clear LastWindow under cursor if it was part of this dialog
        if (pUser->pLastWindowUnderCursor)
        {
            if( (pUser->pLastWindowUnderCursor == (ui_win*)pDialog) ||
                (pUser->pLastWindowUnderCursor->IsChildOf( pDialog )) )
            {
                pUser->pLastWindowUnderCursor = NULL;
            }
        }
        // End the dialog
        pUser->DialogStack.Delete( Count-1 );
        LOG_MESSAGE( "ui_manager::EndDialog", "Dialog closed. ID:0x%08x", pDialog );
        pDialog->Destroy();
        delete pDialog;

        s_EndDialogCount++;
    }
}

//=========================================================================

void ui_manager::EndUsersDialogs( s32 UserID )
{
    // Loop until all dialogs gone
    while( GetNumUserDialogs( UserID ) > 0 )
    {
        // End last dialog on stack
        EndDialog( UserID );
    }
}

//=========================================================================

s32 ui_manager::GetNumUserDialogs( s32 UserID )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );
    user*   pUser = (user*)UserID;

    // Return the number of stacked dialogs
    return pUser->DialogStack.GetCount();
}

//=========================================================================

ui_dialog* ui_manager::GetTopmostDialog( s32 UserID )
{
    ASSERT( (m_Users.Find( (user*)UserID )) != -1 );
    user*   pUser = (user*)UserID;

    if( pUser->DialogStack.GetCount() > 0 )
        return pUser->DialogStack.GetAt( pUser->DialogStack.GetCount()-1 );
    else
        return NULL;
}

//=========================================================================

void ui_manager::PushClipWindow( const irect &r )
{
    s32 X0,Y0,X1,Y1;

    // Read View
    const view& v = *eng_GetView();

    // Save current viewport
    cliprecord& cr = m_ClipStack.Append();
    v.GetViewport( X0, Y0, X1, Y1 );
    cr.r.Set( X0, Y0, X1, Y1 );
}

//=========================================================================

void ui_manager::PopClipWindow( void )
{
    ASSERT( m_ClipStack.GetCount() > 0 );

    // Read previous viewport from stack
    irect& r = m_ClipStack[m_ClipStack.GetCount()-1].r;

    // Delete from stack
    m_ClipStack.Delete( m_ClipStack.GetCount()-1 );
}

//=========================================================================

void ui_manager::WordWrapString( s32 iFont, const irect& r, const char* pString, xwstring& RetVal )
{
    s32 i;
    s32 x           = 0;
    s32 iString     = 0;
    s32 iLineStart  = 0;
    s32 iStringWrap = -1;
    s32 cPrev       = 0;
    s32 c;
    s32 w;

    RetVal.Clear();
    RetVal.FreeExtra();

    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );
    ui_font* pFont = m_Fonts[iFont]->pFont;

    // Word Wrap Text
    while( pString[iString] )
    {
        // Get Character
        c = pString[iString++];

        // Check for end of word
        if( x_isspace(c) && !x_isspace(cPrev) )
        {
            iStringWrap = iString-1;
        }

        // Update previous character
        cPrev = c;

        // Advance cursor before checking wrap
        w = pFont->GetCharacter(c).W;
        x += w+1;

        // Check for NewLine
        if( c == '\n' )
        {
            // Copy String up to wrap point
            for( i=iLineStart ; i<iString ; i++ )
            {
                RetVal += pString[i];
            }
            
            iLineStart  = iString;
            iStringWrap = -1;
            x           = 0;
        }
        else if( x > r.GetWidth() )
        {
            ASSERT( iStringWrap != -1 );

            // Copy String up to wrap point
            for( i=iLineStart ; i<iStringWrap ; i++ )
            {
                RetVal += pString[i];
            }
            RetVal += '\n';

            // Skip Space
            while( x_isspace(pString[i]) )
                i++;

            // Reset line scanner
            iLineStart  = i;
            iString     = i;
            iStringWrap = -1;
            x           = 0;
        }
    }

    // Output last line
    while( iLineStart < iString )
        RetVal += pString[iLineStart++];
}

//=========================================================================
void ui_manager::WordWrapString( s32 iFont, const irect& r, const xwstring& String, xwstring& RetVal )
{
    s32 i;
    s32 x           = 0;
    s32 iString     = 0;
    s32 iLineStart  = 0;
    s32 iStringWrap = -1;
    s32 cPrev       = 0;
    s32 c;
    s32 w;
    f32 ScaleX=1;
    f32 ScaleY=1;

    RetVal.Clear();
    RetVal.FreeExtra();

    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );
    ui_font* pFont = m_Fonts[iFont]->pFont;

    // check for text scaling
    if( ScaleText )
    {
        s32 XRes, YRes;
        eng_GetRes( XRes, YRes );
        ScaleX = (f32)XRes / 512.0f;
        ScaleY = (f32)YRes / 448.0f;
    }

    // Word Wrap Text
    while( String[iString] )
    {
        // Get Character
        c = String[iString++];

        // Skip Color Codes
        if( (c & 0xff00) == 0xff00 )
        {
            iString++;
        }
        else
        {
            // Check for end of word
            if( x_isspace(c) && !x_isspace(cPrev) )
            {
                iStringWrap = iString-1;
            }

            // Update previous character
            cPrev = c;

            // Advance cursor before checking wrap
            if( ScaleText )
            {
                w = (u32)((f32)pFont->GetCharacter(c).W * ScaleX );
            }
            else
            {
                w = pFont->GetCharacter(c).W;
            }
			
            x += w+1;

            // Check for NewLine
            if( c == '\n' )
            {
                // Copy String up to wrap point
                for( i=iLineStart ; i<iString ; i++ )
                {
                    RetVal += String[i];
                }
            
                iLineStart  = iString;
                iStringWrap = -1;
                x           = 0;
            }
            else if( x > r.GetWidth() )
            {
                //ASSERT( iStringWrap != -1 );
                // In case of REALLY REALLY REALLY long lines, we still need to
                // break somewhere, so let's do it at the end of the line.
                if (iStringWrap == -1)
                    iStringWrap = iString-1;

                // Copy String up to wrap point
                for( i=iLineStart ; i<iStringWrap ; i++ )
                {
                    RetVal += String[i];
                }
                RetVal += '\n';

                // Skip Space
                while( x_isspace(String[i]) )
                    i++;

                // Reset line scanner
                iLineStart  = i;
                iString     = i;
                iStringWrap = -1;
                x           = 0;
            }
        }
    }

    // Output last line
    while( iLineStart < iString )
        RetVal += String[iLineStart++];
}


//=========================================================================

xbitmap* ui_manager::GetButtonTexture( s32 buttonCode )      
{
    return (m_ButtonTextures[buttonCode].GetPointer());
}

//=========================================================================

s32 ui_manager::LookUpButtonCode( const xwchar* pString, s32 iStart ) const
{
    s32      c;
    xwstring codeString;

    c = pString[iStart];
    while( c && (c != 0xBB ) ) // '»'
    {
        // add this character to the string
        codeString += pString[iStart];
        iStart++;
        c = pString[iStart];
    }
    
    if( codeString.GetLength() == 0 )
    {
        return -1;
    }

    // look for the string in the code table
    for (s32 i=0; i<NUM_BUTTON_CODES; i++)
    {
        if( x_wstrcmp( codeString, m_ButtonCodeTable[i].CodeString ) == 0 )
        {
            // found a match
            return (m_ButtonCodeTable[i].ButtonCode);
        }
    }

    // not found
    return -1;
}

//=========================================================================

void ui_manager::SetRes( void )
{
    s32 XRes, YRes;
    eng_GetRes( XRes, YRes );

#if defined(TARGET_PC) && !defined(X_EDITOR)
    m_ScaleX = (f32)XRes / 512.0f;
    m_ScaleY = (f32)YRes / 448.0f;
#else   // is editor
    m_ScaleX = 1.0f;
    m_ScaleY = 1.0f;
#endif
}

//=========================================================================

void ui_manager::CheckRes( void )
{
    // Adjust the position of the dialogs according to the resolution.
    s32 XRes, YRes;
    eng_GetRes( XRes, YRes );
    s32 midX = XRes>>1;
    s32 midY = YRes>>1;

    s32 dx = midX - 256;
    s32 dy = midY - 256;

    for( s32 i = 0; i < m_Users.GetCount(); i++)
    {
        user* pUser = m_Users[i];
        
        // Render all Dialogs from the Render Modal one
        for( s32 j = 0; j<pUser->DialogStack.GetCount(); j++)
        {
            // If the resolution of the Dialog don't then we have to reposition them.
            if( pUser->DialogStack[j]->m_XRes != XRes && pUser->DialogStack[j]->m_YRes != YRes )
            {
                pUser->DialogStack[j]->m_Position = pUser->DialogStack[j]->m_CreatePosition;
                pUser->DialogStack[j]->m_Position.Translate( dx, dy );
                pUser->DialogStack[j]->m_XRes = XRes;
               pUser->DialogStack[j]->m_YRes = YRes;
            }
        }        
    }
}

//=========================================================================

void ui_manager::InitScreenWipe ( void )
{
    // Set up the screen wipe data
    m_wipeActive    = TRUE;
    m_wipeDown      = TRUE;
    m_wipeWidth     = 32;
    m_wipeSpeed     = 960.0f;

    m_wipeStartY    = m_CurrScreenSize.t + 8;
    m_wipeEndY      = m_CurrScreenSize.b - 8;

    m_wipePos.l     = m_CurrScreenSize.l + 21;
    m_wipePos.t     = m_CurrScreenSize.t + 8;
    m_wipePos.r     = m_CurrScreenSize.r - 23;
    m_wipePos.b     = m_wipePos.t + m_wipeWidth; 

    m_wipeCount     = 16;

    // initialize trail
    m_wipeTrail[0].Active = TRUE;
    m_wipeTrail[0].Position = m_wipePos;

    for (s32 i=1; i<16; i++)
    {
        m_wipeTrail[i].Active = FALSE;
        m_wipeTrail[i].Position = m_wipeTrail[i-1].Position;
        m_wipeTrail[i].Position.t -= m_wipeWidth;
        m_wipeTrail[i].Position.b -= m_wipeWidth;
    }

    // play wipe sound effect
    g_AudioMgr.Play( "ScreenWipe" );
}

//=========================================================================

void ui_manager::RenderScreenWipe( void )
{
    u8 val;
    xcolor col, col2;

    if (!m_wipeActive)
        return;

    for (s32 i=0; i<16; i++)
    {
        if (m_wipeTrail[i].Active)
        {
            val  = 256-(i*16);
    
            //col.R = (134 * val) / 256;
            //col.G = (239 * val) / 256;
            //col.B = ( 51 * val) / 256;
            col.R = (146 * val) / 256;
            col.G = (226 * val) / 256;
            col.B = (100 * val) / 256;
            col.A = val;

            val = 256-((i+1)*16);

            //col2.R = (134 * val) / 256;
            //col2.G = (239 * val) / 256;
            //col2.B = ( 51 * val) / 256;
            col2.R = (146 * val) / 256;
            col2.G = (226 * val) / 256;
            col2.B = (100 * val) / 256;
            col2.A = val;

            RenderGouraudRect(m_wipeTrail[i].Position, col2, col, col, col2, FALSE, TRUE);
        }
    }
}

//=========================================================================

void ui_manager::UpdateScreenWipe( f32 DeltaTime )
{
    s32 deltaPos;

    if (!m_wipeActive)
        return;

#ifdef TARGET_PC
    DeltaTime = DeltaTime * m_ScaleY;
#endif

    deltaPos = (s32)(m_wipeSpeed * DeltaTime);

    if (m_wipeDown)
    {
        m_wipePos.b += deltaPos;

        if (m_wipePos.b >= m_wipeEndY)
        {
            m_wipePos.t = m_wipePos.b - deltaPos;
            m_wipePos.b = m_wipeEndY;
            m_wipeDown = FALSE;
        }
        else
        {
            m_wipePos.t = m_wipePos.b - deltaPos;
        }
    }
    else
    {
        //m_wipePos.t = -1;
        if (--m_wipeCount == 0)
            m_wipeActive = FALSE;

        if( m_wipePos.t < m_wipeEndY )
        {
            m_wipePos.t += deltaPos;

            if( m_wipePos.t > m_wipeEndY )
            {
                m_wipePos.t = m_wipeEndY;
            }
        }
    }

    for (s32 i=15; i>0; i--)
    {
        m_wipeTrail[i].Position = m_wipeTrail[i-1].Position;
        m_wipeTrail[i].Active = m_wipeTrail[i-1].Active;
    }
    m_wipeTrail[0].Position = m_wipePos;

    if (m_wipeCount == 15)
        m_wipeTrail[0].Active = FALSE;

}

//=========================================================================
void ui_manager::ResetScreenWipe( void )
{
    // reset flag
    m_wipeActive    = FALSE;

    // reset trail
    for (s32 i=0; i<16; i++)
    {
        m_wipeTrail[i].Active = FALSE;
    }
}

//=============================================================================

void ui_manager::InitRefreshBar( void )
{
    m_RefreshSpeed  = 80;
    m_RefreshWidth  = 5;
    m_RefreshPos.l  = m_CurrScreenSize.l + 22;      
    m_RefreshPos.t  = m_CurrScreenSize.b - m_RefreshWidth;
    m_RefreshPos.r  = m_CurrScreenSize.r - 23;
    m_RefreshPos.b  = m_CurrScreenSize.b;
}

//=============================================================================

void ui_manager::RenderRefreshBar( void )
{
    //xcolor c1 (146, 226, 100,  64);
    //xcolor c2 (146, 226, 100,   0);

    //RenderGouraudRect( m_RefreshPos, c1, c2, c2, c1, FALSE, TRUE );
}

//=============================================================================

void ui_manager::UpdateRefreshBar( f32 deltaTime )
{
#ifdef TARGET_PC
    deltaTime = deltaTime * m_ScaleY;
#endif

    m_RefreshPos.t  -= (s32)( ( m_RefreshSpeed * deltaTime ) + 0.5f );

    if( m_RefreshPos.t < m_CurrScreenSize.t )
        m_RefreshPos.t = m_CurrScreenSize.b - m_RefreshWidth;
    
    m_RefreshPos.l  = m_CurrScreenSize.l + 22;      
    m_RefreshPos.r  = m_CurrScreenSize.r - 23;
    m_RefreshPos.b  = m_RefreshPos.t + m_RefreshWidth;
}


//=========================================================================

void ui_manager::SetScreenSize ( const irect& size )
{    
    m_CurrScreenSize    = size; 
    m_GlowStartX        = m_CurrScreenSize.l + 46;
    m_GlowEndX          = m_CurrScreenSize.r - 46 - 16;
}

//=========================================================================

void ui_manager::InitScreenHighlight( void )
{
    m_ScreenHighlightID      = g_UiMgr->FindElement( "highlight" );
    m_ScreenGlowID           = g_UiMgr->FindElement( "screenglow" );
    m_HighlightAlpha         = 0;
    m_ScreenHighlightEnabled = FALSE;
    m_HighlightFadeUp        = TRUE;
    m_CycleFadeUp            = TRUE;
}

//=========================================================================

void ui_manager::SetScreenHighlight( const irect& pos )
{ 
    m_ScreenHighlightPos.t = m_CurrScreenSize.t + pos.t - 12;
    m_ScreenHighlightPos.b = m_CurrScreenSize.t + pos.b + 12;
    m_ScreenHighlightPos.l = m_CurrScreenSize.l + 22;
    m_ScreenHighlightPos.r = m_CurrScreenSize.r - 22;
    
    m_ScreenHighlightEnabled = TRUE;
}

//=========================================================================

void ui_manager::RenderScreenHighlight( void )
{
    // check if enabled
    if (!m_ScreenHighlightEnabled)
        return;

    // don't render if the screen is scaling
    if( m_isScaling )
        return;

    // render the background highlight
    u32 val = 64 + (m_HighlightAlpha * 1);
    g_UiMgr->RenderElement( m_ScreenHighlightID, m_ScreenHighlightPos, 0, xcolor(val,val,val,val), TRUE );
}

//=========================================================================

s32 ui_manager::GetHighlightAlpha( s32 cycle )
{
    s32 val;
    s32 returnVal;

    if ( m_HighlightFadeUp )
    {
        val = m_HighlightAlpha % cycle;
    }
    else
    {
        val = (32 - m_HighlightAlpha) % cycle;
    }

    if ( m_CycleFadeUp )
    {
        returnVal = val;
    }
    else
    {
        returnVal = ( cycle - val );
    }

    if ( val == ( cycle - 1 ) )
    {
        m_CycleFadeUp = !m_CycleFadeUp;
    }

    return ( returnVal );
}

//=========================================================================

void ui_manager::RenderScreenGlow( void )
{
    // check if enabled
    if (!m_ScreenHighlightEnabled)
        return;

    // don't render if the screen is scaling
    if( m_isScaling )
        return;

    // render the highlight glow
    irect pos  = m_ScreenHighlightPos;
    pos.l -= 4;
    pos.r += 4;
    u32 val = 128 + (m_HighlightAlpha * 2);
    g_UiMgr->RenderElement( m_ScreenGlowID, pos, 0, xcolor(val,val,val,val), TRUE );
}

//=========================================================================

void ui_manager::InitGlowBar ( void )
{
    // Set up the glow bar data
    m_GlowID = g_UiMgr->FindElement( "glow" );

    m_GlowStartX    = m_CurrScreenSize.l + 46;
    m_GlowEndX      = m_CurrScreenSize.r - 46 - 16;

    m_GlowPos.l     = m_GlowStartX;
    m_GlowPos.t     = m_CurrScreenSize.t;
    m_GlowPos.r     = m_GlowStartX + 16;
    m_GlowPos.b     = m_CurrScreenSize.t + 7;

    m_GlowSpeed     = 120;
    m_GlowOnTop     = TRUE;

    // initialize trail
    for (s32 i=0; i<8; i++)
    {
        m_GlowTrail[i].l = -1;
    }
}

//=========================================================================

void ui_manager::RenderGlowBar( void )
{
    u8 val;

    if (!m_ScreenIsOn)
        return;

    for (s32 i=0; i<8; i++)
    {
        if (m_GlowTrail[i].l != -1)
        {
            val = 255-(i*32);
            g_UiMgr->RenderElement(m_GlowID, m_GlowTrail[i], 0, xcolor(val,val,val,val), TRUE );
        }
    }
}

//=========================================================================

void ui_manager::UpdateGlowBar( f32 deltaTime )
{
    (void) deltaTime;

    if (!m_ScreenIsOn)
        return;

#ifdef TARGET_PC
    deltaTime = deltaTime * m_ScaleX;
#endif

    if (m_GlowOnTop)
    {
        m_GlowPos.l += (s32)((m_GlowSpeed * deltaTime) + 0.5f);
        m_GlowPos.r  = m_GlowPos.l + 16;

        if (m_GlowPos.l > m_GlowEndX)
        {
            m_GlowPos.l = m_GlowEndX;
            m_GlowPos.r = m_GlowEndX + 16;
            m_GlowPos.t = m_CurrScreenSize.b - 7;
            m_GlowPos.b = m_CurrScreenSize.b;
            m_GlowOnTop = FALSE;
        }
        else if (m_GlowPos.l < m_GlowStartX)
        {
            m_GlowPos.l = m_GlowStartX;
            m_GlowPos.r = m_GlowStartX + 16;

            for (s32 i=0; i<8; i++)
            {
                if (m_GlowTrail[i].l < m_GlowStartX)
                    m_GlowTrail[i].l = -1;
            }
        }
    }
    else
    {
        m_GlowPos.l -= (s32)((m_GlowSpeed * deltaTime) + 0.5f);
        m_GlowPos.r  = m_GlowPos.l + 16;

        if (m_GlowPos.l < m_GlowStartX)
        {
            m_GlowPos.l = m_GlowStartX;
            m_GlowPos.r = m_GlowStartX + 16;
            m_GlowPos.t = m_CurrScreenSize.t;
            m_GlowPos.b = m_CurrScreenSize.t + 7;
            m_GlowOnTop = TRUE;
        }
        else if (m_GlowPos.l > m_GlowEndX)
        {
            m_GlowPos.l = m_GlowEndX;
            m_GlowPos.r = m_GlowEndX + 16;

            for (s32 i=0; i<8; i++)
            {
                if (m_GlowTrail[i].l > m_GlowEndX)
                    m_GlowTrail[i].l = -1;
            }
        }
    }

    for (s32 i=7; i>0; i--)
    {
        m_GlowTrail[i] = m_GlowTrail[i-1];
    }
    m_GlowTrail[0] = m_GlowPos;
}

//=============================================================================

void ui_manager::RenderProgressBar( xbool mustDraw )
{
    xcolor TextColor( 94, 205, 241, 255 ); //xcolor(93,228,223,255)

    // Decide if we've moved far enough to update
    if ( !mustDraw )
    {
        if ( (m_LastProgressUpdatePercent > 0.0f) && (m_PercentLoaded < (m_LastProgressUpdatePercent + s_PercentBetweenUpdates)) )
        {
            // not far enough
            return;
        }
    }

    m_LastProgressUpdatePercent = m_PercentLoaded;

    // calculate the translation based on the resolution
    s32 XRes, YRes;
    eng_GetRes( XRes, YRes );
                
    s32 midX = XRes>>1;
    s32 midY = YRes>>1;

    s32 dx = midX - 256;
    s32 dy = midY - 224;

    if( eng_Begin("Progress Bar") )
    {
        // render the background image.
        RenderBackground( "loadscreen" );

        // render text
        irect rb( 196, 308, 316, 338 );
        rb.Translate( dx, dy );
        RenderText( FindFont( "large" ), rb, ui_font::h_center, TextColor, g_StringTableMgr( "ui", "IDS_LOADING_MSG" ));

        // calculate scale factors
        f32 ScaleX = (f32)XRes / 512.0f;
        f32 ScaleY = (f32)YRes / 448.0f;

        // render the inner bar
        rb.l = 58;
        rb.t = 270;
        rb.b = 278;
        rb.r = rb.l + (s32)(s_ProgressBarScale * m_PercentLoaded);

        // scale it for screen resolution
        rb.l = (u32)((f32)rb.l * ScaleX);
        rb.r = (u32)((f32)rb.r * ScaleX);
        rb.t = (u32)((f32)rb.t * ScaleY);
        rb.b = (u32)((f32)rb.b * ScaleY);

        RenderRect(rb, xcolor(199,236,249,255), FALSE);

        // render the alien shell
        rb.l = 53;
        rb.r = 463;
        rb.t = 262;
        rb.b = 286;

        // scale it for screen resolution
        rb.l = (u32)((f32)rb.l * ScaleX);
        rb.r = (u32)((f32)rb.r * ScaleX);
        rb.t = (u32)((f32)rb.t * ScaleY);
        rb.b = (u32)((f32)rb.b * ScaleY);
        //RenderElementUV( g_UiMgr->FindElement( "loadbar" ), rb, vector2( 0.0f, 0.0f ), vector2( 0.8f, 1.0f ) );

        rb.l = 151;
        rb.t = 360;
        rb.r = 361;
        rb.b = 400;
        rb.Translate( dx, dy );

#ifndef X_RETAIL
        RenderText( FindFont( "small" ), rb, ui_font::h_left, TextColor, "Memory used:" );
        xwstring memString  = (const char *)xfs( "%dk",  (x_MemGetUsed() / 1024) );
        RenderText( FindFont( "small" ), rb, ui_font::h_right, TextColor, memString );

        rb.t += 20;
        rb.b += 20;

        RenderText( FindFont( "small" ), rb, ui_font::h_left, TextColor, "Memory remaining:");
        xwstring memString2  = (const char *)xfs( "%dk",  (x_MemGetFree() / 1024) );
        RenderText( FindFont( "small" ), rb, ui_font::h_right, TextColor, memString2 );

        rb.t += 20;
        rb.b += 20;
        rb.l -= 50;
        rb.r += 50;

        RenderText( FindFont( "small" ), rb, ui_font::h_left, TextColor, "Level Name:");
        xwstring LevelString  = (const char *)xfs( "%s",  g_ActiveConfig.GetLevelName() );
        RenderText( FindFont( "small" ), rb, ui_font::h_right, TextColor, LevelString );
#endif

        eng_End();
    }

    eng_PageFlip();
}

//=========================================================================

void ui_manager::SetPercentLoaded( f32 percent )
{
    m_PercentLoaded = percent;

    if (m_PercentLoaded > 100.0f)
        m_PercentLoaded = 100.0f;

    if ( m_PercentLoaded <= 1.0f )
    {
        m_LastProgressUpdatePercent = 0.0f;
    }

    if ( percent <= 1.0f )
    {
        RenderProgressBar(TRUE);
    }
    else
    {
        RenderProgressBar(FALSE);
    }
}

//=========================================================================

void ui_manager::AddPercentLoaded( f32 percent )
{
    m_PercentLoaded += percent;

    if (m_PercentLoaded > 100.0f)
        m_PercentLoaded = 100.0f;

    RenderProgressBar(FALSE);
}

//=========================================================================

void ui_manager::EnableSafeArea( void )                
{ 
    m_RenderSafeArea = TRUE; 
}

//=========================================================================

void ui_manager::DisableSafeArea( void )                
{ 
    m_RenderSafeArea = FALSE; 
}

//=========================================================================

s32 ui_manager::PingToColor( f32 ping, xcolor& responsecolor )
{
    xcolor S_RED   ( 210,  50,  50, 240 );
    xcolor S_YELLOW( 230, 230,   0, 240 );
    xcolor S_GREEN ( 50,  220,  50, 240 );

    if      (ping > 500.0f)  { responsecolor = S_RED;   return 8;   }
    else if (ping > 400.0f)  { responsecolor = S_RED;   return 7;   }
    else if (ping > 300.0f)  { responsecolor = S_RED;   return 6;   }
    else if (ping > 250.0f)  { responsecolor = S_YELLOW;return 5;   }
    else if (ping > 200.0f)  { responsecolor = S_YELLOW;return 4;   }
    else if (ping > 150.0f)  { responsecolor = S_YELLOW;return 3;   }
    else if (ping > 125.0f)  { responsecolor = S_GREEN; return 2;   }
    else if (ping > 100.0f)  { responsecolor = S_GREEN; return 2;   }
    else if (ping > 75.0f)   { responsecolor = S_GREEN; return 1;   }
    else                     { responsecolor = S_GREEN; return 0;   }
}
