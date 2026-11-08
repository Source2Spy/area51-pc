//=========================================================================
//
//  ui_manager.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "ui_manager.hpp"
#include "ui_win.hpp"
#include "ui_font.hpp"
#include "ui_text_data.hpp"
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
#include "ui_maplist.hpp"
#include "ui_textbox.hpp"
#include "ui_dlg_vkeyboard.hpp"
#include "ui_tabbed_dialog.hpp"
#include "ui_dlg_list.hpp"
#include "ui_renderer.hpp"
#include "Render/Render.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "InputMgr/GamePad.hpp"

#include "Bitmap/aux_Bitmap.hpp"

#include "StringMgr/StringMgr.hpp"
#include "AudioMgr/AudioMgr.hpp"

static const char*  m_ButtonTexturesNames[] = {
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_A.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_B.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_X.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_Y.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_DIRECTIONALDOWN.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_DIRECTIONALLEFT.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_DIRECTIONALUP.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_DIRECTIONALRIGHT.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALUPDOWN.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALLEFTRIGHT.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_STICKR.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_STICKL.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_TRIGGERL.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_TRIGGERR.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_BLACK.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_WHITE.XBMP"),
PRELOAD_XBOX_FILE("CONTROLS/XBOX/UI_BUTTONSXBOX_START.XBMP"),

PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_X.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_SQUARE.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_TRIANGLE.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_CIRCLE.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALDOWN.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALLEFT.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALUP.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALRIGHT.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALUPDOWN.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_DIRECTIONALLEFTRIGHT.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_STICKR.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_STICKL.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_L1.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_L2.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_R1.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_R2.XBMP"),
PRELOAD_PS2_FILE("CONTROLS/PS2/UI_BUTTONSPS2_START.XBMP"),

PRELOAD_FILE("ICONS/UI_KILLS.XBMP"),
PRELOAD_FILE("ICONS/UI_TEAMKILLS.XBMP"),
PRELOAD_FILE("ICONS/UI_DEATHS.XBMP"),
PRELOAD_FILE("ICONS/UI_FLAGS.XBMP"),
PRELOAD_FILE("ICONS/UI_VOTES.XBMP"),
PRELOAD_FILE("ICONS/UI_FLAGS.XBMP"),  // DUMMY
PRELOAD_FILE("ICONS/UI_FLAGS.XBMP"),  // DUMMY
PRELOAD_FILE("ICONS/UI_FLAGS.XBMP")   // DUMMY
};

// TexelsPerUIUnit describes asset density, independently of on-screen UI scale.
// A 2x atlas contains two source texels for each logical UI unit.
static const ui_manager::element_desc s_ElementDescs[] =
{
//   Name               Bitmap                                      States  Columns  Rows  TexelsPerUIUnit
    { "frame",          PRELOAD_FILE("WIDGETS/UI_FRAME_PIECES.XBMP"),          2,      3,      3,          2.0f },
    { "frame2",         PRELOAD_FILE("WIDGETS/UI_INNER_FRAME.XBMP"),           1,      3,      3,          2.0f },
    { "glow",           PRELOAD_FILE("WIDGETS/UI_BARGLOW.XBMP"),               1,      1,      1,          2.0f },
    { "highlight",      PRELOAD_FILE("WIDGETS/UI_SELECTION_GLOW.XBMP"),        1,      3,      3,          2.0f },
    { "screenglow",     PRELOAD_FILE("WIDGETS/UI_SCREEN_GLOW.XBMP"),           1,      3,      3,          2.0f },
    { "button",         PRELOAD_FILE("WIDGETS/UI_UIBUTTON.XBMP"),              5,      3,      1,          2.0f },
    { "button_check",   PRELOAD_FILE("WIDGETS/UI_CHECKBOX.XBMP"),              5,      1,      1,          2.0f },
    { "sb_frame",       PRELOAD_FILE("WIDGETS/UI_INNER_FRAME.XBMP"),           1,      3,      3,          2.0f },
    { "sb_arrowdown",   PRELOAD_FILE("WIDGETS/UI_DOWNARROW.XBMP"),             5,      1,      1,          2.0f },
    { "sb_arrowup",     PRELOAD_FILE("WIDGETS/UI_UPARROW.XBMP"),               5,      1,      1,          2.0f },
    { "sb_container",   PRELOAD_FILE("WIDGETS/UI_CONTAINER.XBMP"),             5,      3,      3,          2.0f },
    { "sb_thumb",       PRELOAD_FILE("WIDGETS/UI_THUMB.XBMP"),                 5,      1,      3,          2.0f },
    { "slider_bar",     PRELOAD_FILE("WIDGETS/UI_SLIDER_CONTAINER.XBMP"),      1,      3,      1,          2.0f },
    { "slider_thumb",   PRELOAD_FILE("WIDGETS/UI_SLIDER_THUMB.XBMP"),          5,      1,      1,          2.0f },
    { "button_combo1",  PRELOAD_FILE("WIDGETS/UI_COMBO_BOX.XBMP"),             5,      3,      1,          2.0f },
    { "button_combo2",  PRELOAD_FILE("WIDGETS/UI_COMBO_BOX_128.XBMP"),         1,      3,      1,          2.0f },
    { "button_edit",    PRELOAD_FILE("WIDGETS/UI_TEXT_BOX.XBMP"),              5,      3,      1,          2.0f },
};

static const s32 s_nElementDescs = sizeof(s_ElementDescs) / sizeof(s_ElementDescs[0]);

#define NUM_BUTTON_CODES        58

static const f32 SCREEN_WIPE_TRAIL_LENGTH  = 512.0f;
static const f32 SCREEN_WIPE_HEAD_HEIGHT   = 32.0f;
static const f32 SCREEN_WIPE_SPEED         = 960.0f;
static const f32 SCREEN_WIPE_FADE_DURATION = 16.0f / 30.0f;

namespace
{
    struct ui_nine_slice_layout
    {
        f32 X[4];
        f32 Y[4];
    };

    static xbool ui_CalculateNineSliceLayout( const ui_manager::element& Element,
                                               const irect& Position,
                                               s32 State,
                                               ui_nine_slice_layout& Layout )
    {
        if( (State < 0) || (State >= Element.nStates) )
        {
            return FALSE;
        }

        const s32 BaseIndex = State * Element.cx * Element.cy;

        if( Element.cx == 1 )
        {
            const f32 Width = (f32)Element.LayoutRects[BaseIndex].GetWidth();
            Layout.X[0] = (f32)Position.l + ((f32)Position.GetWidth() - Width) * 0.5f;
            Layout.X[1] = Layout.X[0] + Width;
        }
        else
        {
            ASSERT( Element.cx == 3 );
            const f32 AvailableWidth = (f32)MAX( Position.GetWidth(), 0 );
            f32 LeftWidth  = (f32)Element.LayoutRects[BaseIndex].GetWidth();
            f32 RightWidth = (f32)Element.LayoutRects[BaseIndex + 2].GetWidth();
            const f32 FixedWidth = LeftWidth + RightWidth;
            if( FixedWidth > AvailableWidth )
            {
                const f32 BorderScale = (FixedWidth > 0.0f) ? AvailableWidth / FixedWidth : 0.0f;
                LeftWidth  *= BorderScale;
                RightWidth  = AvailableWidth - LeftWidth;
            }

            Layout.X[0] = (f32)Position.l;
            Layout.X[1] = Layout.X[0] + LeftWidth;
            Layout.X[2] = (f32)Position.r - RightWidth;
            Layout.X[3] = (f32)Position.r;
        }

        if( Element.cy == 1 )
        {
            const f32 Height = (f32)Element.LayoutRects[BaseIndex].GetHeight();
            Layout.Y[0] = (f32)Position.t + ((f32)Position.GetHeight() - Height) * 0.5f;
            Layout.Y[1] = Layout.Y[0] + Height;
        }
        else
        {
            ASSERT( Element.cy == 3 );
            const f32 AvailableHeight = (f32)MAX( Position.GetHeight(), 0 );
            f32 TopHeight    = (f32)Element.LayoutRects[BaseIndex].GetHeight();
            f32 BottomHeight = (f32)Element.LayoutRects[BaseIndex + (Element.cy - 1) * Element.cx].GetHeight();
            const f32 FixedHeight = TopHeight + BottomHeight;
            if( FixedHeight > AvailableHeight )
            {
                const f32 BorderScale = (FixedHeight > 0.0f) ? AvailableHeight / FixedHeight : 0.0f;
                TopHeight    *= BorderScale;
                BottomHeight = AvailableHeight - TopHeight;
            }

            Layout.Y[0] = (f32)Position.t;
            Layout.Y[1] = Layout.Y[0] + TopHeight;
            Layout.Y[2] = (f32)Position.b - BottomHeight;
            Layout.Y[3] = (f32)Position.b;
        }

        return TRUE;
    }
}

//==========================================================================
//  button code table entry
//==========================================================================

struct button_code
{
    xwstring        CodeString;
    s32             ButtonCode;
};

//-------------------------------------------------------------------------

static 
const button_code s_PS2ButtonCodeTable[NUM_BUTTON_CODES] = {
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
    {   "NEWPAGE",      NEW_CREDIT_PAGE             },
    {   "TITLE",        CREDIT_TITLE_LINE           },
    {   "CREDITEND",    CREDIT_END                  },
};

//-------------------------------------------------------------------------

static 
const button_code s_XboxButtonCodeTable[NUM_BUTTON_CODES] = {
    {   "x",            XBOX_BUTTON_A               },
    {   "q",            XBOX_BUTTON_X               },
    {   "a",            XBOX_BUTTON_B               },
    {   "o",            XBOX_BUTTON_Y               },
    {   "d",            XBOX_BUTTON_DPAD_DOWN       },
    {   "l",            XBOX_BUTTON_DPAD_LEFT       },
    {   "u",            XBOX_BUTTON_DPAD_UP         },
    {   "r",            XBOX_BUTTON_DPAD_RIGHT      },
    {   "R",            XBOX_BUTTON_STICK_RIGHT     },
    {   "L",            XBOX_BUTTON_STICK_LEFT      },
    {   "1",            XBOX_BUTTON_TRIGGER_L       },
    {   "2",            XBOX_BUTTON_BLACK           },
    {   "3",            XBOX_BUTTON_TRIGGER_R       },
    {   "4",            XBOX_BUTTON_WHITE           },
    {   "S",            XBOX_BUTTON_START           },
    {   "A",            XBOX_BUTTON_A               },
    {   "B",            XBOX_BUTTON_B               },
    {   "X",            XBOX_BUTTON_X               },
    {   "Y",            XBOX_BUTTON_Y               },
    {   "DOWN",         XBOX_BUTTON_DPAD_DOWN       },
    {   "LEFT",         XBOX_BUTTON_DPAD_LEFT       },
    {   "UP",           XBOX_BUTTON_DPAD_UP         },
    {   "RIGHT",        XBOX_BUTTON_DPAD_RIGHT      },
    {   "UPDOWN",       XBOX_BUTTON_DPAD_UPDOWN     },
    {   "LEFTRIGHT",    XBOX_BUTTON_DPAD_LEFTRIGHT  },
    {   "LTRIG",        XBOX_BUTTON_TRIGGER_L       },
    {   "BLACK",        XBOX_BUTTON_BLACK           },
    {   "STICKL",       XBOX_BUTTON_STICK_LEFT      },
    {   "RTRIG",        XBOX_BUTTON_TRIGGER_R       },
    {   "WHITE",        XBOX_BUTTON_WHITE           },
    {   "STICKR",       XBOX_BUTTON_STICK_RIGHT     },
    {   "PAUSE",        XBOX_BUTTON_START           },
    {   "GRENADE",      XBOX_BUTTON_B               },
    {   "RELOAD",       XBOX_BUTTON_X               },
    {   "USE",          XBOX_BUTTON_X               },
    {   "PREVWEAPON",   XBOX_BUTTON_BLACK           },
    {   "NEXTWEAPON",   XBOX_BUTTON_Y               },
    {   "LEANLEFT",     XBOX_BUTTON_DPAD_LEFT       },
    {   "LEANRIGHT",    XBOX_BUTTON_DPAD_RIGHT      },
    {   "LEAN",         XBOX_BUTTON_DPAD_LEFTRIGHT  },
    {   "MUTATE",       XBOX_BUTTON_DPAD_UP         },
    {   "TRANSFORM",    XBOX_BUTTON_DPAD_UP         },
    {   "JUMP",         XBOX_BUTTON_A               },
    {   "CROUCH",       XBOX_BUTTON_STICK_LEFT      },
    {   "FLASHLIGHT",   XBOX_BUTTON_DPAD_DOWN       },
    {   "FIRE",         XBOX_BUTTON_TRIGGER_R       },
    {   "SECONDARY",    XBOX_BUTTON_TRIGGER_L       },
    {   "MELEE",        XBOX_BUTTON_STICK_RIGHT     },
    {   "PARASITE",     XBOX_BUTTON_TRIGGER_R       },
    {   "CONTAGION",    XBOX_BUTTON_TRIGGER_L       },
    {   "KILLS",        KILL_ICON                   },
    {   "DEATHS",       DEATH_ICON                  },
    {   "TKS",          TEAM_KILL_ICON              },
    {   "FLAGS",        FLAG_ICON                   },
    {   "VOTES",        VOTE_ICON                   },
    {   "NEWPAGE",      NEW_CREDIT_PAGE             },
    {   "TITLE",        CREDIT_TITLE_LINE           },
    {   "CREDITEND",    CREDIT_END                  },
};

//=========================================================================

static
const button_code* GetButtonCodeTable( input_platform Platform )
{
    if( Platform == INPUT_PLATFORM_PS2 )
        return s_PS2ButtonCodeTable;

    if( Platform == INPUT_PLATFORM_XBOX )
        return s_XboxButtonCodeTable;

    // Keyboard/mouse do not have embedded controller glyphs. Use the Xbox
    // glyph set as the established fallback for unknown controller layouts.
    return s_XboxButtonCodeTable;
}

//=========================================================================

static
xbool IsControllerButtonCode( s32 ButtonCode )
{
    return( ((ButtonCode >= XBOX_BUTTON_A)     && (ButtonCode <= XBOX_BUTTON_START)) ||
            ((ButtonCode >= PS2_BUTTON_CROSS) && (ButtonCode <= PS2_BUTTON_START )) );
}

//=========================================================================

static
xbool UseLocalizedXboxButtonTexture( s32 ButtonCode )
{
    if( (ButtonCode != XBOX_BUTTON_TRIGGER_L) && (ButtonCode != XBOX_BUTTON_TRIGGER_R) )
        return FALSE;

    if( x_GetTerritory() == XL_TERRITORY_AMERICA )
        return FALSE;

    switch( x_GetLocale() )
    {
    case XL_LANG_ENGLISH:
    case XL_LANG_GERMAN:
        return FALSE;
    default:
        return TRUE;
    }
}

//=========================================================================

static
void SetButtonTextureName( rhandle<texture>& Texture, s32 ButtonCode )
{
    if( UseLocalizedXboxButtonTexture( ButtonCode ) )
    {
        xstring ButtonName( m_ButtonTexturesNames[ButtonCode] );
        ButtonName.Delete( ButtonName.GetLength()-5, 5 );
        ButtonName += "_";
        ButtonName += x_GetLocaleString();
        ButtonName += ".xbmp";
        Texture.SetName( ButtonName );
    }
    else
    {
        Texture.SetName( m_ButtonTexturesNames[ButtonCode] );
    }
}

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

ui_manager* g_UiMgr    = NULL;
s32         g_UiUserID = -1;

static_assert( ui_manager::MAX_INPUT_CONTROLLERS == frontend_input::MAX_CONTROLLERS,
               "UI and frontend input controller counts must match" );

//=========================================================================
//  Helpers
//=========================================================================

#if !defined(X_RETAIL) || defined(X_QA)
static
void RenderDebugText( const char* pText, s32 X, s32 Y )
{
    if( !g_UiMgr || !g_UIRenderer.IsInitialized() )
        return;

    const s32 Font = g_UiMgr->FindFont( "small" );
    if( Font < 0 )
        return;

    const irect Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();
    const s32 LineHeight = g_UiMgr->GetLineHeight( Font );
    irect Position( Bounds.l + 8 + X * 8, Bounds.t + 8 + Y * LineHeight,
                    Bounds.r - 8, Bounds.t + 8 + ( Y + 1 ) * LineHeight );
    g_UiMgr->RenderText( Font, Position, ui_font::h_left | ui_font::v_top, XCOLOR_WHITE,
                         pText, TRUE, FALSE );
}

//=========================================================================
#endif

static
xbool IsUILogicalDown( s32 ControllerID, frontend_pad::logical_id LogicalID )
{
    ASSERT( ControllerID >= 0 );
    ASSERT( ControllerID < MAX_LOCAL_PLAYERS );

    const frontend_pad::logical& Logical = g_FrontendInput.GetPad( ControllerID ).GetFrameLogical( LogicalID );
    return( (Logical.GetIsValue() > 0.25f) ||
            (Logical.GetWasValue() > 0.25f) );
}

//=========================================================================

static
s32 GetButtonCodeForGadget( input_gadget GadgetID )
{
    switch( GadgetID )
    {
    case INPUT_PS2_BTN_CROSS:       return PS2_BUTTON_CROSS;
    case INPUT_PS2_BTN_SQUARE:      return PS2_BUTTON_SQUARE;
    case INPUT_PS2_BTN_TRIANGLE:    return PS2_BUTTON_TRIANGLE;
    case INPUT_PS2_BTN_CIRCLE:      return PS2_BUTTON_CIRCLE;
    case INPUT_PS2_BTN_L_DOWN:      return PS2_BUTTON_DPAD_DOWN;
    case INPUT_PS2_BTN_L_LEFT:      return PS2_BUTTON_DPAD_LEFT;
    case INPUT_PS2_BTN_L_UP:        return PS2_BUTTON_DPAD_UP;
    case INPUT_PS2_BTN_L_RIGHT:     return PS2_BUTTON_DPAD_RIGHT;
    case INPUT_PS2_BTN_R_STICK:     return PS2_BUTTON_STICK_RIGHT;
    case INPUT_PS2_BTN_L_STICK:     return PS2_BUTTON_STICK_LEFT;
    case INPUT_PS2_BTN_L1:          return PS2_BUTTON_L1;
    case INPUT_PS2_BTN_L2:          return PS2_BUTTON_L2;
    case INPUT_PS2_BTN_R1:          return PS2_BUTTON_R1;
    case INPUT_PS2_BTN_R2:          return PS2_BUTTON_R2;
    case INPUT_PS2_BTN_START:       return PS2_BUTTON_START;

    case INPUT_XBOX_BTN_A:          return XBOX_BUTTON_A;
    case INPUT_XBOX_BTN_X:          return XBOX_BUTTON_X;
    case INPUT_XBOX_BTN_Y:          return XBOX_BUTTON_Y;
    case INPUT_XBOX_BTN_B:          return XBOX_BUTTON_B;
    case INPUT_XBOX_BTN_DOWN:       return XBOX_BUTTON_DPAD_DOWN;
    case INPUT_XBOX_BTN_LEFT:       return XBOX_BUTTON_DPAD_LEFT;
    case INPUT_XBOX_BTN_UP:         return XBOX_BUTTON_DPAD_UP;
    case INPUT_XBOX_BTN_RIGHT:      return XBOX_BUTTON_DPAD_RIGHT;
    case INPUT_XBOX_BTN_R_STICK:    return XBOX_BUTTON_STICK_RIGHT;
    case INPUT_XBOX_BTN_L_STICK:    return XBOX_BUTTON_STICK_LEFT;
    case INPUT_XBOX_BTN_WHITE:      return XBOX_BUTTON_WHITE;
    case INPUT_XBOX_BTN_BLACK:      return XBOX_BUTTON_BLACK;
    case INPUT_XBOX_L_TRIGGER:      return XBOX_BUTTON_TRIGGER_L;
    case INPUT_XBOX_R_TRIGGER:      return XBOX_BUTTON_TRIGGER_R;
    case INPUT_XBOX_BTN_START:      return XBOX_BUTTON_START;
    case INPUT_XBOX_BTN_BACK:       return XBOX_BUTTON_START;
    default:                        return -1;
    }
}

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

//=========================================================================
//  ui_manager
//=========================================================================

ui_manager::ui_manager( void )
{
    m_AlphaTime             = 0.0f;
    m_wipeActive            = FALSE;
    m_wipeFading            = FALSE;
    m_pWipeOwner            = NULL;
    m_wipeBounds.Clear();
    m_wipeRevealY           = 0;
    m_wipeSpeed             = 0.0f;
    m_wipeHeadY             = 0.0f;
    m_wipeFade              = 0.0f;
    m_RefreshStepAccumulator= 0.0f;
    m_GlowStepAccumulator   = 0.0f;
    m_CallbackDepth         = 0;
    m_EnableUserInput       = 0;
    m_WaitForInputRelease   = FALSE;
    m_log                   = 0;
    m_isInitialized          = FALSE;
}

//=========================================================================

ui_manager::~ui_manager( void )
{
    Kill();
}

//=========================================================================

void ui_manager::UpdateViewport( void )
{
    g_UIRenderer.RefreshViewport();
    irect const Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();

    for( s32 i = 0; i < m_Users.GetCount(); i++ )
    {
        user* pUser = m_Users[i];
        pUser->Bounds = Bounds;
        pUser->MouseX = x_clamp( pUser->MouseX, (f32)Bounds.l, (f32)(Bounds.r - 1) );
        pUser->MouseY = x_clamp( pUser->MouseY, (f32)Bounds.t, (f32)(Bounds.b - 1) );
    }
}

//=========================================================================

s32 ui_manager::Init( void )
{
    if( m_isInitialized )
        return 0;

    if( !g_UIRenderer.Init() )
    {
        ASSERTS( FALSE, "Failed to initialize UI renderer" );
        return -1;
    }

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
    RegisterWinClass( "maplist",    &ui_maplist_factory     );


//=--  Register the default dialog classes
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
    VERIFY( g_StringTableMgr.LoadTable( "ui", "ENG_UI_STRINGS.string" ) );

    // load scan strings
    VERIFY( g_StringTableMgr.LoadTable( "scan", "ENG_CHARACTER_SCAN_STRINGS.string" ) );

    // load lore_ingame strings
    VERIFY( g_StringTableMgr.LoadTable( "lore_ingame", "ENG_INGAME_LORE_STRINGS.string" ) );

    //-- Load Elements
    //-- Fonts
    LoadFont        ( "large",          "config.font" );
    LoadFont        ( "small",          "config.font" );
    LoadFont        ( "hudnum",         "config.font" );
    LoadFont        ( "loadscr",        "config.font" );

    for( s32 i=0; i<s_nElementDescs; i++ )
    {
        LoadElement( s_ElementDescs[i] );
    }

    LoadBitmap      ( "a51_logo",               PRELOAD_FILE("VARIANTS/PRELOAD/SCREENS/UI_A51_LOGO.XBMP"                        ) );

    //-- Presence Icons
    LoadBitmap      ( "icon_friend",            PRELOAD_PS2_FILE("ONLINE/UI_PS2_FRIEND.XBMP"                   ) );
    LoadBitmap      ( "icon_voice_on",          PRELOAD_PS2_FILE("ONLINE/UI_PS2_VOICE_ON.XBMP"                 ) );
    LoadBitmap      ( "icon_voice_muted",       PRELOAD_PS2_FILE("ONLINE/UI_PS2_VOICE_MUTED.XBMP"              ) );
    LoadBitmap      ( "icon_voice_thru_tv",     PRELOAD_PS2_FILE("ONLINE/UI_PS2_VOICE_THRU_TV.XBMP"            ) );
    LoadBitmap      ( "icon_voice_speaking",    PRELOAD_PS2_FILE("ONLINE/UI_VOICE_SPEAKING.XBMP"               ) );

    for (s32 i=0;i<NUM_BUTTON_TEXTURES;i++)
    {
        SetButtonTextureName( m_ButtonTextures[i], i );
    }    

    s32 MemoryBudget = MemoryStart - x_MemGetFree();

    //-- Create a user in the resolution-independent UI coordinate space.
    g_UIRenderer.RefreshViewport();
    irect const r = g_UIRenderer.GetViewport().GetLogicalClipBounds();

    g_UiUserID = CreateUser( -1, r );
    EnableUser(g_UiUserID,FALSE);
    ASSERT( g_UiUserID );

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

    m_GlowID = -255;

    m_isInitialized = TRUE;

#if !defined(X_RETAIL) || defined(X_QA)
    x_SetPrintAtHook( RenderDebugText );
#endif

    return( MemoryBudget );
}

//=========================================================================

void ui_manager::Kill( void )
{

    if( !m_isInitialized )
        return;

#if !defined(X_RETAIL) || defined(X_QA)
    x_SetPrintAtHook( NULL );
#endif

    ASSERT( m_CallbackDepth == 0 );
    DestroyDeferredDialogs();
    
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
        m_Elements[0]->Bitmap.Destroy();
        delete m_Elements[0];
        m_Elements.Delete( 0 );
    }

    // Destroy Backgrounds
    while( m_Backgrounds.GetCount() > 0 )
    {
        m_Backgrounds[0]->Bitmap.Destroy();
        delete m_Backgrounds[0];
        m_Backgrounds.Delete( 0 );
    }

    while( m_Bitmaps.GetCount() > 0 )
    {
        m_Bitmaps[0]->Bitmap.Destroy();
        delete m_Bitmaps[0];
        m_Bitmaps.Delete( 0 );
    }

    for( s32 i = 0; i < NUM_BUTTON_TEXTURES; i++ )
        m_ButtonTextures[i].Destroy();

    // Destroy Window Classes
    m_WindowClasses.Delete( 0, m_WindowClasses.GetCount() );

    // Destroy Dialog Classes
    m_DialogClasses.Delete( 0, m_DialogClasses.GetCount() );

    g_UIRenderer.Kill();
    m_isInitialized = FALSE;
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
    rect const& Bounds = g_UIRenderer.GetViewport().GetLogicalBounds();

    // if we're not to show the background, then render a rectangle
    if( m_EnableBackground==FALSE )
    {
        RenderRect( g_UIRenderer.GetViewport().GetLogicalClipBounds(), XCOLOR_BLACK, FALSE );
        return;
    }
    s32 iBackground = FindBackground( pName );
    if( iBackground == -1 )
        return;

    background* pBackground = m_Backgrounds[iBackground];
    ASSERT( pBackground );

    texture* pTexture = pBackground->Bitmap.GetPointer();
    if( pTexture )
    {
        g_UIRenderer.DrawImage( *pTexture,
                                Bounds.Min,
                                Bounds.GetSize(),
                                vector2( 0.0f, 0.0f ),
                                vector2( 1.0f, 1.0f ) );
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
    texture* pTexture = pBitmap->Bitmap.GetPointer();
    const vector2 p( (f32)Position.l, (f32)Position.t );
    vector2     wh( (f32)(Position.r - Position.l), (f32)(Position.b - Position.t));

    // If we have a bitmap, render it. If not, just render a gouraud rect.
    if( pTexture )
    {
        g_UIRenderer.DrawImage( *pTexture,
                                p,
                                wh,
                                vector2( 0.0f, 0.0f ),
                                vector2( 1.0f, 1.0f ),
                                Color,
                                0.0f,
                                UI_BLEND_ALPHA,
                                UI_SAMPLER_LINEAR_CLAMP );
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
    texture* pTexture = pBitmap->Bitmap.GetPointer();
    if( !pTexture )
        return;

    const vector2 p( (f32)Position.l, (f32)Position.t );
    vector2 wh( (f32)(Position.r - Position.l), (f32)(Position.b - Position.t));

    g_UIRenderer.DrawImage( *pTexture,
                            p,
                            wh,
                            UV0,
                            UV1,
                            Color,
                            0.0f,
                            UI_BLEND_ALPHA,
                            UI_SAMPLER_LINEAR_CLAMP );
}

//=========================================================================

s32 ui_manager::LoadElement( const element_desc& Desc )
{
    element*    pElement;
    xarray<s32> x;
    xarray<s32> y;
    s32         i;
    s32         ix;
    s32         iy;
    xcolor      RegColor;
    texture*    pTexture;
    xbitmap*    pBitmap;

    const xbool IsValidDesc = Desc.pName &&
                              Desc.pBitmapName &&
                              (Desc.nStates > 0) &&
                              ((Desc.cx == 1) || (Desc.cx == 3)) &&
                              ((Desc.cy == 1) || (Desc.cy == 3)) &&
                              x_isvalid( Desc.TexelsPerUIUnit ) &&
                              (Desc.TexelsPerUIUnit > 0.0f);
    ASSERT( IsValidDesc );
    if( !IsValidDesc )
        return -1;

    // Check if element already exists
    {
        s32 ID = FindElement( Desc.pName );
        if( ID != -1 )
            return ID;
    }

    // Create new element
    pElement = new element;
    ASSERT( pElement );

    // Set data
    pElement->Name    = Desc.pName;
    pElement->nStates = Desc.nStates;
    pElement->cx      = Desc.cx;
    pElement->cy      = Desc.cy;

    // Load the bitmap
    pElement->Bitmap.SetName( Desc.pBitmapName );
    pTexture = pElement->Bitmap.GetPointer();
    ASSERT( pTexture );
    if( !pTexture )
    {
        delete pElement;
        return -1;
    }
    pBitmap = &pTexture->m_bitmap;

    // Pick out registration mark color
    RegColor = pBitmap->GetPixelColor( 0, 0 );

    // Find the registration markers
    x.SetCapacity( Desc.cx+1 );
    y.SetCapacity( (Desc.cy*Desc.nStates)+1 );
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

    const xbool MarkersAreValid = (x.GetCount() == (Desc.cx+1)) &&
                                  (y.GetCount() == ((Desc.cy*Desc.nStates)+1));
    ASSERT( MarkersAreValid );
    if( !MarkersAreValid )
    {
        delete pElement;
        return -1;
    }

    // Texture rectangles remain in physical texels. Layout rectangles are
    // converted once to resolution-independent UI units.
    pElement->TextureRects.SetCapacity( Desc.cx*Desc.cy*Desc.nStates );
    pElement->LayoutRects.SetCapacity ( Desc.cx*Desc.cy*Desc.nStates );
    for( iy=0 ; iy<(Desc.cy*Desc.nStates) ; iy++ )
    {
        for( ix=0 ; ix<Desc.cx ; ix++ )
        {
            irect& TextureRect = pElement->TextureRects.Append();
            TextureRect.Set( x[ix]+1, y[iy]+1, x[ix+1], y[iy+1] );

            s32 const LogicalWidth  = MAX( 1, (s32)x_floor( (f32)TextureRect.GetWidth()  / Desc.TexelsPerUIUnit + 0.5f ) );
            s32 const LogicalHeight = MAX( 1, (s32)x_floor( (f32)TextureRect.GetHeight() / Desc.TexelsPerUIUnit + 0.5f ) );
            pElement->LayoutRects.Append().Set( 0, 0, LogicalWidth, LogicalHeight );
        }
    }

    // Register the bitmap for VRAM
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
    const element* pElement;
    texture*       pTexture;
    const xbitmap* pBitmap;

    ASSERT( (iElement >= 0) && (iElement < m_Elements.GetCount()) );

    // Get Element pointer
    pElement = m_Elements[iElement];

    // Validate arguments
    ASSERT( (State >= 0) && (State < pElement->nStates) );

    ui_nine_slice_layout Layout;
    if( !ui_CalculateNineSliceLayout( *pElement, Position, State, Layout ) )
    {
        return;
    }

    pTexture = pElement->Bitmap.GetPointer();
    if( !pTexture )
        return;
    pBitmap = &pTexture->m_bitmap;
    const ui_blend_mode Blend = IsAdditive ? UI_BLEND_ADDITIVE : UI_BLEND_ALPHA;

    const s32 BaseIndex = State * pElement->cx * pElement->cy;
    for( s32 YIndex = 0; YIndex < pElement->cy; YIndex++ )
    {
        for( s32 XIndex = 0; XIndex < pElement->cx; XIndex++ )
        {
            const s32 ElementIndex = BaseIndex + XIndex + YIndex * pElement->cx;
            vector2 UV0;
            vector2 UV1;
            ui_GetTextureRectUV( pElement->TextureRects[ElementIndex],
                                 pBitmap->GetWidth(),
                                 pBitmap->GetHeight(),
                                 UV0,
                                 UV1 );

            const vector2 DrawPosition( Layout.X[XIndex], Layout.Y[YIndex] );
            const vector2 DrawSize( Layout.X[XIndex + 1] - Layout.X[XIndex],
                                    Layout.Y[YIndex + 1] - Layout.Y[YIndex] );
            g_UIRenderer.DrawImage( *pTexture,
                                    DrawPosition,
                                    DrawSize,
                                    UV0,
                                    UV1,
                                    Color,
                                    0.0f,
                                    Blend,
                                    UI_SAMPLER_LINEAR_CLAMP_ATLAS );
        }
    }
}

//=========================================================================

void ui_manager::RenderElementUV( s32 iElement, const irect& Position, const irect& UV, const xcolor& Color, xbool IsAdditive ) const
{
    vector2     p(0.0f, 0.0f );
    vector2     wh;
    vector2     uv0;
    vector2     uv1;
    element*    pElement;
    texture*    pTexture;
    const xbitmap* pBitmap;

    ASSERT( (iElement >= 0) && (iElement < m_Elements.GetCount()) );

    // Get Element pointer
    pElement = m_Elements[iElement];

    pTexture = pElement->Bitmap.GetPointer();
    if( !pTexture )
        return;
    pBitmap = &pTexture->m_bitmap;

    // Calculate Position and Size
    p.X      = (f32)Position.l;
    p.Y      = (f32)Position.t;
    wh.X     = (f32)Position.GetWidth();
    wh.Y     = (f32)Position.GetHeight();

    // Calculate UVs
    ui_GetTextureRectUV( UV,
                         pBitmap->GetWidth(),
                         pBitmap->GetHeight(),
                         uv0,
                         uv1 );

    // Draw sprite
    g_UIRenderer.DrawImage( *pTexture,
                            p,
                            wh,
                            uv0,
                            uv1,
                            Color,
                            0.0f,
                            IsAdditive ? UI_BLEND_ADDITIVE : UI_BLEND_ALPHA,
                            UI_SAMPLER_LINEAR_CLAMP_ATLAS );
}

//=========================================================================

void ui_manager::RenderElementUV( s32 iElement, const irect& Position, const vector2& UV0, const vector2& UV1, const xcolor& Color, xbool IsAdditive ) const
{
    vector2     p(0.0f, 0.0f );
    vector2     wh;
    element*    pElement;
    texture*    pTexture;

    ASSERT( (iElement >= 0) && (iElement < m_Elements.GetCount()) );

    // Get Element pointer
    pElement = m_Elements[iElement];

    pTexture = pElement->Bitmap.GetPointer();
    if( !pTexture )
        return;

    // Calculate Position and Size
    p.X      = (f32)Position.l;
    p.Y      = (f32)Position.t;
    wh.X     = (f32)Position.GetWidth();
    wh.Y     = (f32)Position.GetHeight();

    // Draw sprite
    g_UIRenderer.DrawImage( *pTexture,
                            p,
                            wh,
                            UV0,
                            UV1,
                            Color,
                            0.0f,
                            IsAdditive ? UI_BLEND_ADDITIVE : UI_BLEND_ALPHA,
                            UI_SAMPLER_LINEAR_CLAMP_ATLAS );
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
    pFont->PathName = pPathName;

    // Create and load font
    pFont->pFont = new ui_font;
    ASSERT( pFont->pFont );
    VERIFY( pFont->pFont->Load( this, pPathName, pName ) );

    // Add to array of fonts
    m_Fonts.Append() = pFont;

    // Return Font ID
    return m_Fonts.GetCount()-1;
}

//=========================================================================

xbool ui_manager::ReloadFonts( void )
{
    xbool Result = TRUE;

    for( s32 i = 0; i < m_Fonts.GetCount(); i++ )
    {
        font* pFont = m_Fonts[i];

        if( !pFont->pFont->Load( this, pFont->PathName, pFont->Name ) )
        {
            Result = FALSE;
        }
    }

    return Result;
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

void ui_manager::RenderInputText( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color, const xwchar* pString, input_platform Platform ) const
{
    ASSERT( (iFont >= 0) && (iFont < m_Fonts.GetCount()) );

    ui_font* pFont = m_Fonts[iFont]->pFont;
    pFont->RenderInputText( Position, Flags, Color, pString, Platform );
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
    g_UIRenderer.DrawRect( r, Color, IsWire );
}

//=========================================================================

void ui_manager::RenderGouraudRect( const irect& r, const xcolor& c1, const xcolor& c2, const xcolor& c3, const xcolor& c4, xbool IsWire, xbool IsAdditive ) const
{
    g_UIRenderer.DrawGradientRect( r,
                                   c1,
                                   c4,
                                   c3,
                                   c2,
                                   IsWire,
                                   IsAdditive ? UI_BLEND_ADDITIVE : UI_BLEND_ALPHA );
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
        ASSERT( (ControllerID >= -1) && (ControllerID < MAX_INPUT_CONTROLLERS) );
        pUser->ControllerID             = ((ControllerID >= 0) && (ControllerID < MAX_INPUT_CONTROLLERS))
                                        ? ControllerID
                                        : -1;
        pUser->Bounds                   = Bounds;
        pUser->Data                     = Data;
        pUser->Height                   = 0;
        pUser->pCaptureWindow           = 0;
        pUser->pPressedWindow           = 0;
        pUser->pFocusedWindow           = 0;
        pUser->pHoveredWindow           = 0;
        pUser->InputDevice              = ui_input_device::None;
        pUser->InputPlatform            = INPUT_PLATFORM_NONE;

        pUser->MouseVisible             = FALSE;
        pUser->MouseX                   = Bounds.GetWidth()/2  + Bounds.l;
        pUser->MouseY                   = Bounds.GetHeight()/2 + Bounds.t;
        pUser->LastMouseX               = Bounds.GetWidth()/2  + Bounds.l;
        pUser->LastMouseY               = Bounds.GetHeight()/2 + Bounds.t;

        // Set Analog Scalers
        for( i=0 ; i<MAX_INPUT_CONTROLLERS ; i++ )
        {
            pUser->NavigateUp[i]   .SetupRepeat( 0.200f, 0.060f );
            pUser->NavigateDown[i] .SetupRepeat( 0.200f, 0.060f );
            pUser->NavigateLeft[i] .SetupRepeat( 0.200f, 0.060f );
            pUser->NavigateRight[i].SetupRepeat( 0.200f, 0.060f );
            pUser->Delete[i]       .SetupRepeat( 0.400f, 0.050f );
        }
        static const s32 MAX_DIALOGS_EVER = 20;
        pUser->DialogStack.SetCapacity( MAX_DIALOGS_EVER ); // Plenty of room
        pUser->DialogRevision = 0;

        // Assign a stable id (handle) and add to the users list
        static s32 s_NextUserId = 0;
        pUser->Id = ++s_NextUserId;
        m_Users.Append() = pUser;
    }

    return pUser->Id;
}

//=========================================================================

void ui_manager::DeleteAllUsers( void )
{
    while( m_Users.GetCount() > 0 )
    {
        DeleteUser( m_Users[0]->Id );
    }
}

//=========================================================================

void ui_manager::DeleteUser( s32 UserID )
{
    s32     Index;

    ASSERT( m_CallbackDepth == 0 );

    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    // Find the users index
    Index = m_Users.Find( GetUserById( UserID ) );
    ASSERT( Index != -1 );

    // Close all the dialog that may be open
    while( m_Users[Index]->DialogStack.GetCount() > 0 )
    {
        s32 i = m_Users[Index]->DialogStack.GetCount()-1;
        delete m_Users[Index]->DialogStack[i];
        m_Users[Index]->DialogStack.Delete( i );
    }
    m_Users[Index]->DialogStack.FreeExtra();

    // Delete the user
    delete m_Users[Index];
    m_Users.Delete( Index );
}

//=========================================================================

ui_manager::user* ui_manager::GetUserById( s32 UserID ) const
{
    for( s32 i=0; i<m_Users.GetCount(); i++ )
        if( m_Users[i]->Id == UserID )
            return m_Users[i];
    return NULL;
}

//=========================================================================

ui_manager::user* ui_manager::GetUser( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    return pUser;
}

//=========================================================================

void ui_manager::SetUserController( s32 UserID, s32 ControllerID )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    ASSERT( (ControllerID >= -1) && (ControllerID < MAX_INPUT_CONTROLLERS) );

    user* pUser = GetUserById( UserID );
    pUser->ControllerID = ((ControllerID >= 0) && (ControllerID < MAX_INPUT_CONTROLLERS))
                        ? ControllerID
                        : -1;
}

//=========================================================================

s32 ui_manager::GetUserData( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    return pUser->Data;
}

//=========================================================================

ui_win* ui_manager::GetFocusedWindow( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    return pUser->pFocusedWindow;
}

//=========================================================================

ui_input_device ui_manager::GetInputDevice( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );
    return GetUserById( UserID )->InputDevice;
}

//=========================================================================

input_platform ui_manager::GetInputPlatform( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );
    return GetUserById( UserID )->InputPlatform;
}

//=========================================================================

xbool ui_manager::DispatchInput( ui_win* pTarget, ui_input_event& Event )
{
    if( !pTarget )
    {
        return FALSE;
    }

    Event.m_pTarget = pTarget;
    m_CallbackDepth++;
    xbool const Handled = pTarget->OnInput( Event );
    m_CallbackDepth--;

    if( m_CallbackDepth == 0 )
    {
        DestroyDeferredDialogs();
    }

    return Handled;
}

//=========================================================================

void ui_manager::DestroyDeferredDialogs( void )
{
    while( m_DeferredDialogs.GetCount() > 0 )
    {
        ui_dialog* pDialog = m_DeferredDialogs[0];
        m_DeferredDialogs.Delete( 0 );
        delete pDialog;
    }
}

//=========================================================================

void ui_manager::SetHoveredWindow( user* pUser, ui_win* pWin )
{
    ASSERT( pUser );

    if( pUser->pHoveredWindow == pWin )
    {
        return;
    }

    if( pUser->pHoveredWindow )
    {
        pUser->pHoveredWindow->OnPointerLeave( pUser->pHoveredWindow );
        pUser->pHoveredWindow->m_IsHovered = FALSE;
    }

    pUser->pHoveredWindow = pWin;

    if( pUser->pHoveredWindow )
    {
        pUser->pHoveredWindow->m_IsHovered = TRUE;
    }
}

//=========================================================================

void ui_manager::UpdateMousePosition( user* pUser, s32 ScreenDeltaX, s32 ScreenDeltaY )
{
    vector2 const LogicalDelta = g_UIRenderer.GetViewport().ScreenDeltaToLogical(
        vector2( (f32)ScreenDeltaX, (f32)ScreenDeltaY ) );
    pUser->MouseX += LogicalDelta.X;
    pUser->MouseY += LogicalDelta.Y;

    // Clamp to screen bounds
    pUser->MouseX = MAX( pUser->MouseX, (f32)pUser->Bounds.l );
    pUser->MouseX = MIN( pUser->MouseX, (f32)(pUser->Bounds.r - 1) );
    pUser->MouseY = MAX( pUser->MouseY, (f32)pUser->Bounds.t );
    pUser->MouseY = MIN( pUser->MouseY, (f32)(pUser->Bounds.b - 1) );
}

//=========================================================================

void ui_manager::SetInputMode( user* pUser, ui_input_device Device, input_platform Platform )
{
    ASSERT( pUser );

    pUser->InputDevice   = Device;
    pUser->InputPlatform = Platform;

    if( Device == ui_input_device::Mouse )
    {
        pUser->MouseVisible = TRUE;
    }
    else if( (Device == ui_input_device::Keyboard) ||
             (Device == ui_input_device::Gamepad) )
    {
        pUser->MouseVisible = FALSE;
        SetHoveredWindow( pUser, NULL );
    }
}

//=========================================================================

void ui_manager::SetMouseVisible( s32 UserID, xbool State )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    pUser->MouseVisible = State;
    if( !State )
    {
        SetHoveredWindow( pUser, NULL );
    }
}

//=========================================================================

xbool ui_manager::GetMouseVisible( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    return pUser->MouseVisible;
}

//=========================================================================

void ui_manager::SetMousePos( s32 UserID, s32 x, s32 y )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    pUser->MouseX = (f32)x;
    pUser->MouseY = (f32)y;
}

//=========================================================================

void ui_manager::GetMousePos( s32 UserID, s32& x, s32& y ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    x = (s32)x_floor( pUser->MouseX + 0.5f );
    y = (s32)x_floor( pUser->MouseY + 0.5f );
}

//=========================================================================

void ui_manager::SetFocusWindow( s32 UserID, ui_win* pWin )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );

    // Only fire events when the focused window actually changes
    if( pWin == pUser->pFocusedWindow )
        return;

    if( pUser->pFocusedWindow )
        pUser->pFocusedWindow->OnFocusLost( pUser->pFocusedWindow );

    pUser->pFocusedWindow = pWin;

    if( pUser->pFocusedWindow )
    {
        pUser->pFocusedWindow->OnFocusGained( pUser->pFocusedWindow );

        for( ui_win* pAncestor = pUser->pFocusedWindow->GetParent();
             pAncestor;
             pAncestor = pAncestor->GetParent() )
        {
            pAncestor->OnFocusWithin( pUser->pFocusedWindow );
        }
    }
}

//=========================================================================

ui_win* ui_manager::SetCapture( s32 UserID, ui_win* pWin )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    ui_win* pOldCaptureWin = pUser->pCaptureWindow;
    pUser->pCaptureWindow = pWin;

    return pOldCaptureWin;
}

//=========================================================================

void ui_manager::ReleaseCapture( s32 UserID )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    pUser->pCaptureWindow = NULL;
}

//=========================================================================

void ui_manager::SetUserBackground( s32 UserID, const char* pName )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    m_EnableBackground = TRUE;
    user*   pUser = GetUserById( UserID );
    pUser->Background = pName;
}

//=========================================================================

const irect& ui_manager::GetUserBounds( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    return pUser->Bounds;
}

//=========================================================================

void ui_manager::SetUserScale( f32 Scale )
{
    g_UIRenderer.SetUserScale( Scale );
    UpdateViewport();
}

//=========================================================================

f32 ui_manager::GetUserScale( void ) const
{
    return g_UIRenderer.GetViewport().GetUserScale();
}

//=========================================================================

void ui_manager::EnableUser( s32 UserID, xbool State )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    pUser->Enabled = State;
}

//=========================================================================

xbool ui_manager::IsUserEnabled( s32 UserID ) const
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );

    user*   pUser = GetUserById( UserID );
    return pUser->Enabled;
}

//=========================================================================

xbool ui_manager::ProcessInput( f32 DeltaTime )
{
    s32     i;
    xbool   Continue = TRUE;

    if( m_WaitForInputRelease )
    {
        const input_snapshot& Snapshot = g_Input.GetFrameSnapshot();
        for( s32 i = 0; i < INPUT_GADGET_COUNT; i++ )
        {
            const input_gadget Gadget = static_cast<input_gadget>( i );
            const input_control_kind Kind = input_system::GetGadgetControlKind( Gadget );
            if( (Kind != INPUT_CONTROL_KEY) && (Kind != INPUT_CONTROL_BUTTON) )
                continue;

            for( s32 j = 0; j < INPUT_MAX_DEVICES; j++ )
            {
                if( Snapshot.IsPressed( Gadget, j ) || Snapshot.WasPressed( Gadget, j ) )
                    return TRUE;
            }
        }

        for( s32 i = 0; i < MAX_INPUT_CONTROLLERS; i++ )
        {
            for( s32 j = frontend_pad::UI_UP; j <= frontend_pad::UI_HELP; j++ )
            {
                if( IsUILogicalDown( i, static_cast<frontend_pad::logical_id>( j ) ) )
                    return TRUE;
            }
        }

        // Consume the neutral frame before accepting new input.
        m_WaitForInputRelease = FALSE;
        return TRUE;
    }

    // Keep the cursor moving during transitions without dispatching UI actions.
    if( m_isScaling )
    {
        UpdateViewport();
        s32 const ScreenDeltaX = g_Input.GetFrameMouseDeltaX();
        s32 const ScreenDeltaY = g_Input.GetFrameMouseDeltaY();
        if( ScreenDeltaX || ScreenDeltaY )
        {
            for( i=0 ; i<m_Users.GetCount() ; i++ )
            {
                user* pUser = m_Users[i];
                ASSERT( pUser );
                if( pUser->Enabled )
                {
                    UpdateMousePosition( pUser, ScreenDeltaX, ScreenDeltaY );
                    SetInputMode( pUser, ui_input_device::Mouse, INPUT_PLATFORM_DESKTOP );
                }
            }
        }
    }
    else
    {
        // Loop through each user
        for( i=0 ; i<m_Users.GetCount() ; i++ )
        {
            user* pUser = m_Users[i];
            ASSERT( pUser );

            // Only process input for enabled users
            if( pUser->Enabled )
            {
                Continue &= ProcessInput( DeltaTime, pUser->Id );
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
            s32 LocalX = x;
            s32 LocalY = y;
            pDialog->ScreenToLocal( LocalX, LocalY );
            pWindow = pDialog->GetWindowAtXY( LocalX, LocalY );

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

xbool ui_manager::ProcessInput( f32 DeltaTime, s32 UserID )
{
    xbool   Iterate             = FALSE;
    xbool   MouseWheelProcessed = FALSE;
    s32     IterateCount        = 0;

    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );
    user*   pUser = GetUserById( UserID );
    m_CallbackDepth++;
    u32     DialogRevision = pUser->DialogRevision;
    s32 const StartController = (pUser->ControllerID >= 0) ? pUser->ControllerID : 0;
    s32 const EndController   = (pUser->ControllerID >= 0) ? pUser->ControllerID : MAX_INPUT_CONTROLLERS - 1;
    ASSERT( (StartController >= 0) && (EndController < MAX_INPUT_CONTROLLERS) );
    UpdateViewport();

    do
    {
        xbool HoverTargetChanged = FALSE;

        // Don't iterate unless set later
        if( Iterate )
        {
            IterateCount++;
            Iterate = FALSE;
        }

        s32 const MouseWheelDelta = MouseWheelProcessed
                                  ? 0
                                  : static_cast<s32>( g_Input.GetFrameSnapshot().GetValue( INPUT_MOUSE_WHEEL_REL ) );

        // Semantic navigation is routed through focus. Pointer capture is a
        // separate concern and must not redirect keyboard/gamepad actions.
        ui_win* pWin = pUser->pFocusedWindow;

        // Character keys are not semantic UI actions, but they still switch
        // the active input mode and hide the mouse cursor.
        if( g_Input.WasFrameDeviceButtonPressed( INPUT_DEVICE_KEYBOARD ) )
        {
            SetInputMode( pUser, ui_input_device::Keyboard, INPUT_PLATFORM_DESKTOP );
        }

        // Update mouse position and switch to mouse mode if it moved
        {
            s32 const ScreenDeltaX = g_Input.GetFrameMouseDeltaX();
            s32 const ScreenDeltaY = g_Input.GetFrameMouseDeltaY();

            if( ScreenDeltaX || ScreenDeltaY )
            {
                UpdateMousePosition( pUser, ScreenDeltaX, ScreenDeltaY );
            }

            const xbool HasMouseInput =
                   ScreenDeltaX
                || ScreenDeltaY
                || MouseWheelDelta
                || g_Input.GetFrameSnapshot().WasPressed( INPUT_MOUSE_BTN_L );

            if( HasMouseInput )
            {
                SetInputMode( pUser, ui_input_device::Mouse, INPUT_PLATFORM_DESKTOP );
            }

            // Hover and keyboard/gamepad focus are independent. A pointer move
            // must never silently change the focused control.
            if( pUser->InputDevice == ui_input_device::Mouse )
            {
                s32 const MouseX = (s32)x_floor( pUser->MouseX + 0.5f );
                s32 const MouseY = (s32)x_floor( pUser->MouseY + 0.5f );
                ui_win* const pPreviousHover = pUser->pHoveredWindow;
                SetHoveredWindow( pUser, GetWindowAtXY( pUser, MouseX, MouseY ) );
                HoverTargetChanged = (pPreviousHover != pUser->pHoveredWindow);
            }
        }

        // Determine which window receives input this frame
        if( pWin == NULL )
        {
            // If nothing has focus (e.g. mouse is over background), fall back to
            // the topmost dialog so pad navigation always has somewhere to go
            if( pUser->DialogStack.GetCount() > 0 )
                pWin = pUser->DialogStack[pUser->DialogStack.GetCount()-1];
        }

        auto RefreshInputTarget = [&]() -> xbool
        {
            if( DialogRevision == pUser->DialogRevision )
            {
                return FALSE;
            }

            DialogRevision = pUser->DialogRevision;
            pWin = pUser->pFocusedWindow;
            if( !pWin && (pUser->DialogStack.GetCount() > 0) )
            {
                pWin = pUser->DialogStack[pUser->DialogStack.GetCount()-1];
            }
            return TRUE;
        };

        for( s32 i=StartController ; i<=EndController ; i++ )
        {
            {
                UpdateButton( pUser->NavigateUp[i],   IsUILogicalDown( i, frontend_pad::UI_UP ),          DeltaTime );
                UpdateButton( pUser->NavigateDown[i], IsUILogicalDown( i, frontend_pad::UI_DOWN ),        DeltaTime );
                UpdateButton( pUser->NavigateLeft[i], IsUILogicalDown( i, frontend_pad::UI_LEFT ),        DeltaTime );
                UpdateButton( pUser->NavigateRight[i],IsUILogicalDown( i, frontend_pad::UI_RIGHT ),       DeltaTime );
                UpdateButton( pUser->Accept[i],       IsUILogicalDown( i, frontend_pad::UI_SELECT ),      DeltaTime );
                UpdateButton( pUser->Cancel[i],       IsUILogicalDown( i, frontend_pad::UI_BACK ),        DeltaTime );
                UpdateButton( pUser->Delete[i],       IsUILogicalDown( i, frontend_pad::UI_DELETE ),      DeltaTime );
                UpdateButton( pUser->Alternate[i],    IsUILogicalDown( i, frontend_pad::UI_ACTIVATE ),    DeltaTime );
                UpdateButton( pUser->PagePrevious[i], IsUILogicalDown( i, frontend_pad::UI_SHOULDER_L ),  DeltaTime );
                UpdateButton( pUser->PageNext[i],     IsUILogicalDown( i, frontend_pad::UI_SHOULDER_R ),  DeltaTime );
                UpdateButton( pUser->First[i],        IsUILogicalDown( i, frontend_pad::UI_SHOULDER_L2 ), DeltaTime );
                UpdateButton( pUser->Last[i],         IsUILogicalDown( i, frontend_pad::UI_SHOULDER_R2 ), DeltaTime );
                UpdateButton( pUser->Help[i],         IsUILogicalDown( i, frontend_pad::UI_HELP ),        DeltaTime );
            }
        }

        // Update mouse buttons
        UpdateButton( pUser->PointerPrimary,
                      g_Input.GetFrameSnapshot().IsPressed( INPUT_MOUSE_BTN_L ) ||
                      g_Input.GetFrameSnapshot().WasPressed( INPUT_MOUSE_BTN_L ),
                      DeltaTime );

        // Route pointer input to the captured control, or to the control under
        // the cursor. Semantic input continues to use the focused control.
        ui_win* pPointerWin = pUser->pCaptureWindow
                            ? pUser->pCaptureWindow
                            : pUser->pHoveredWindow;
        s32 const MouseX = (s32)x_floor( pUser->MouseX + 0.5f );
        s32 const MouseY = (s32)x_floor( pUser->MouseY + 0.5f );

        if( pPointerWin )
        {
            if( HoverTargetChanged ||
                (pUser->LastMouseX != pUser->MouseX) ||
                (pUser->LastMouseY != pUser->MouseY) )
            {
                ui_input_event Event;
                Event.m_Type   = ui_input_event_type::PointerMove;
                Event.m_Device = ui_input_device::Mouse;
                Event.m_X      = MouseX;
                Event.m_Y      = MouseY;
                DispatchInput( pPointerWin, Event );

                if( RefreshInputTarget() )
                {
                    pPointerWin = NULL;
                }
            }

            if( pPointerWin && pUser->PointerPrimary.nPresses )
            {
                ui_win* pFocusWin = pPointerWin;
                while( pFocusWin && !pFocusWin->CanFocus() )
                {
                    pFocusWin = pFocusWin->GetParent();
                }

                if( pFocusWin )
                {
                    SetFocusWindow( pUser->Id, pFocusWin );
                }

                pPointerWin->m_IsPressed = TRUE;
                pUser->pPressedWindow = pPointerWin;

                ui_input_event Event;
                Event.m_Type          = ui_input_event_type::PointerDown;
                Event.m_Device        = ui_input_device::Mouse;
                Event.m_PointerButton = ui_pointer_button::Primary;
                Event.m_X             = MouseX;
                Event.m_Y             = MouseY;
                Event.m_Presses       = pUser->PointerPrimary.nPresses;
                DispatchInput( pPointerWin, Event );

                if( RefreshInputTarget() )
                {
                    pPointerWin = NULL;
                }
            }

            if( pPointerWin && MouseWheelDelta )
            {
                ui_input_event Event;
                Event.m_Type   = ui_input_event_type::PointerWheel;
                Event.m_Device = ui_input_device::Mouse;
                Event.m_X      = MouseX;
                Event.m_Y      = MouseY;
                Event.m_Delta  = MouseWheelDelta;
                DispatchInput( pPointerWin, Event );
                MouseWheelProcessed = TRUE;
                if( RefreshInputTarget() )
                {
                    pPointerWin = NULL;
                }
            }
        }

        if( pUser->PointerPrimary.nReleases )
        {
            ui_win* pReleaseWin = pUser->pCaptureWindow
                                ? pUser->pCaptureWindow
                                : pUser->pPressedWindow;
            pUser->pPressedWindow = NULL;

            if( pReleaseWin )
            {
                pReleaseWin->m_IsPressed = FALSE;

                ui_input_event Event;
                Event.m_Type          = ui_input_event_type::PointerUp;
                Event.m_Device        = ui_input_device::Mouse;
                Event.m_PointerButton = ui_pointer_button::Primary;
                Event.m_X             = MouseX;
                Event.m_Y             = MouseY;
                DispatchInput( pReleaseWin, Event );
                RefreshInputTarget();
            }
        }

        // Only do this if there is a semantic input target.
        if( pWin )
        {
            // Sum up button presses
            s32 UpPresses       = 0;
            s32 DownPresses     = 0;
            s32 LeftPresses     = 0;
            s32 RightPresses    = 0;
            s32 UpRepeats       = 0;
            s32 DownRepeats     = 0;
            s32 LeftRepeats     = 0;
            s32 RightRepeats    = 0;
            s32 UpCount         = 0;
            s32 DownCount       = 0;
            s32 LeftCount       = 0;
            s32 RightCount      = 0;
            s32 Accept          = 0;
            s32 Cancel          = 0;
            s32 Delete          = 0;
            s32 Alternate       = 0;
            s32 PagePrevious    = 0;
            s32 PageNext        = 0;
            s32 First           = 0;
            s32 Last            = 0;
            s32 Help            = 0;
            {
                s32 i;
                for( i=StartController ; i<=EndController ; i++ )
                {
                    // check input for each controller
                    UpPresses       = pUser->NavigateUp[i].nPresses;
                    DownPresses     = pUser->NavigateDown[i].nPresses;
                    LeftPresses     = pUser->NavigateLeft[i].nPresses;
                    RightPresses    = pUser->NavigateRight[i].nPresses;
                    UpRepeats       = pUser->NavigateUp[i].nRepeats;
                    DownRepeats     = pUser->NavigateDown[i].nRepeats;
                    LeftRepeats     = pUser->NavigateLeft[i].nRepeats;
                    RightRepeats    = pUser->NavigateRight[i].nRepeats;
                    UpCount         = UpPresses + UpRepeats;
                    DownCount       = DownPresses + DownRepeats;
                    LeftCount       = LeftPresses + LeftRepeats;
                    RightCount      = RightPresses + RightRepeats;
                    Accept          = pUser->Accept[i].nPresses;
                    Cancel          = pUser->Cancel[i].nPresses;
                    Delete          = pUser->Delete[i].nPresses + pUser->Delete[i].nRepeats;
                    Alternate       = pUser->Alternate[i].nPresses;
                    PagePrevious    = pUser->PagePrevious[i].nPresses + pUser->PagePrevious[i].nRepeats;
                    PageNext        = pUser->PageNext[i].nPresses + pUser->PageNext[i].nRepeats;
                    First           = pUser->First[i].nPresses + pUser->First[i].nRepeats;
                    Last            = pUser->Last[i].nPresses + pUser->Last[i].nRepeats;
                    Help            = pUser->Help[i].nPresses;
                    
                    xbool const HasSemanticInput =
                           UpCount || DownCount || LeftCount || RightCount
                        || Accept || Cancel || Delete || Alternate || Help
                        || PagePrevious || PageNext || First || Last;

                    ui_input_device const SemanticDevice =
                        (g_Input.GetCurrentInputDevice() == INPUT_DEVICE_GAMEPAD)
                        ? ui_input_device::Gamepad
                        : ui_input_device::Keyboard;

                    if( HasSemanticInput )
                    {
                        m_ActiveController = i;
                        const input_platform SemanticPlatform =
                            (SemanticDevice == ui_input_device::Gamepad)
                            ? g_Input.GetCurrentInputPlatform()
                            : INPUT_PLATFORM_DESKTOP;
                        SetInputMode( pUser, SemanticDevice, SemanticPlatform );
                    }

                    auto SendNavigation = [&]( ui_navigation Navigation,
                                               s32 Presses,
                                               s32 Repeats,
                                               xbool WrapX,
                                               xbool WrapY )
                    {
                        ui_input_event Event;
                        Event.m_Type       = ui_input_event_type::Navigate;
                        Event.m_Device     = SemanticDevice;
                        Event.m_Navigation = Navigation;
                        Event.m_Presses    = Presses;
                        Event.m_Repeats    = Repeats;
                        Event.m_WrapX      = WrapX;
                        Event.m_WrapY      = WrapY;
                        DispatchInput( pWin, Event );
                        RefreshInputTarget();
                    };

                    auto SendAction = [&]( ui_input_event_type Type )
                    {
                        ui_input_event Event;
                        Event.m_Type   = Type;
                        Event.m_Device = SemanticDevice;
                        DispatchInput( pWin, Event );
                        RefreshInputTarget();
                    };

                    u32 const EventDialogRevision = DialogRevision;

                    if( UpCount && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendNavigation( ui_navigation::Up, UpPresses, UpRepeats, FALSE, TRUE );
                    }

                    if( DownCount && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendNavigation( ui_navigation::Down, DownPresses, DownRepeats, FALSE, TRUE );
                    }

                    if( LeftCount && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendNavigation( ui_navigation::Left, LeftPresses, LeftRepeats, FALSE, FALSE );
                    }

                    if( RightCount && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendNavigation( ui_navigation::Right, RightPresses, RightRepeats, FALSE, FALSE );
                    }

                    if( !Iterate && Accept && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendAction( ui_input_event_type::Accept );
                    }

                    if( !Iterate && Cancel && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendAction( ui_input_event_type::Cancel );
                    }

                    if( !Iterate && Delete && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendAction( ui_input_event_type::Delete );
                    }

                    if( !Iterate && Alternate && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendAction( ui_input_event_type::Alternate );
                    }

                    if( !Iterate && Help && (EventDialogRevision == DialogRevision) )
                    {
                        Iterate = TRUE;
                        SendAction( ui_input_event_type::Help );
                    }

                    if( PagePrevious && (EventDialogRevision == DialogRevision) )
                    {
                        SendNavigation( ui_navigation::PagePrevious, PagePrevious, 0, FALSE, FALSE );
                    }
                    else if( PageNext && (EventDialogRevision == DialogRevision) )
                    {
                        SendNavigation( ui_navigation::PageNext, PageNext, 0, FALSE, FALSE );
                    }

                    if( First && (EventDialogRevision == DialogRevision) )
                    {
                        SendNavigation( ui_navigation::First, First, 0, FALSE, FALSE );
                    }
                    else if( Last && (EventDialogRevision == DialogRevision) )
                    {
                        SendNavigation( ui_navigation::Last, Last, 0, FALSE, FALSE );
                    }

                    if( EventDialogRevision != DialogRevision )
                    {
                        break;
                    }
                }
            }
        }

        // Save last mouse position for next frame
        pUser->LastMouseX = pUser->MouseX;
        pUser->LastMouseY = pUser->MouseY;

        // Clear DeltaTime in case of next iteration
        DeltaTime = 0.0f;

    } while( Iterate && !IterateCount );

    xbool const Continue = !g_Input.GetFrameSnapshot().IsPressed( INPUT_MSG_EXIT );

    m_CallbackDepth--;
    if( m_CallbackDepth == 0 )
    {
        DestroyDeferredDialogs();
    }

    return Continue;
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
    ClearUserInput();
}

//=========================================================================

void ui_manager::SuppressInputUntilRelease( void )
{
    m_WaitForInputRelease = TRUE;
    ClearUserInput();
}

//=========================================================================

void ui_manager::ClearUserInput( void )
{
    for( s32 i=0 ; i<m_Users.GetCount() ; i++ )
    {
        user* pUser = m_Users[i];
        ASSERT( pUser );

        for( s32 j=0 ; j<MAX_INPUT_CONTROLLERS ; j++ )
        {
            pUser->NavigateUp[j]   .Clear();
            pUser->NavigateDown[j] .Clear();
            pUser->NavigateLeft[j] .Clear();
            pUser->NavigateRight[j].Clear();
            pUser->Accept[j]       .Clear();
            pUser->Cancel[j]       .Clear();
            pUser->Delete[j]       .Clear();
            pUser->Alternate[j]    .Clear();
            pUser->Help[j]         .Clear();
            pUser->PagePrevious[j] .Clear();
            pUser->PageNext[j]     .Clear();
            pUser->First[j]        .Clear();
            pUser->Last[j]         .Clear();
        }
    }
}

//=========================================================================

void ui_manager::Update( f32 DeltaTime )
{
    const f32 HighlightFadeRate = 30.0f;

    // Update AlphaTime
    m_AlphaTime += DeltaTime;
    m_AlphaTime = x_fmod( m_AlphaTime, 1.0f );

    // Update highlight alpha
    if( m_HighlightFadeUp )
    {
        m_HighlightAlpha += (DeltaTime * HighlightFadeRate);
        if( m_HighlightAlpha >= 32.0f )
        {
            m_HighlightAlpha = 32.0f;
            m_HighlightFadeUp = FALSE;
        }
    }
    else
    {
        m_HighlightAlpha -= (DeltaTime * HighlightFadeRate);
        if( m_HighlightAlpha <= 0.0f )
        {
            m_HighlightAlpha = 0.0f;
            m_HighlightFadeUp = TRUE;
        }
    }

    // update the screen wipe
    UpdateScreenWipe(DeltaTime);

    // update the refresh bar
    UpdateRefreshBar(DeltaTime);

    m_CallbackDepth++;

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
                pUser->DialogStack[j]->UpdateTree( DeltaTime );
            }
        }
    }

    m_CallbackDepth--;
    if( m_CallbackDepth == 0 )
    {
        DestroyDeferredDialogs();
    }
}

//=========================================================================

void ui_manager::RenderNavText( const user* pUser ) const
{
    ASSERT( pUser );

    if( !pUser || (pUser->InputDevice != ui_input_device::Gamepad) )
    {
        return;
    }

    if( pUser->DialogStack.GetCount() == 0 )
    {
        return;
    }

    const ui_dialog* pDialog = pUser->DialogStack[pUser->DialogStack.GetCount() - 1];
    if( !pDialog->IsNavTextVisible() || (pDialog->GetNavText().GetLength() == 0) )
    {
        return;
    }

    irect Position = pUser->Bounds;
    Position.t = Position.b - 48;
    Position.b -= 8;

    const s32 FontID = FindFont( "small" );
    RenderInputText( FontID,
                     Position,
                     ui_font::h_center | ui_font::v_top,
                     XCOLOR_WHITE,
                     pDialog->GetNavText(),
                     pUser->InputPlatform );
}

//=========================================================================

void ui_manager::Render( void )
{
    s32 i;
    UpdateViewport();

    if( eng_Begin( "UI" ) )
    {
        xbool RenderCursor = FALSE;

        // Loop through each user to render
        for( i=0 ; i<m_Users.GetCount() ; i++ )
        {
            user* pUser = m_Users[i];
            ASSERT( pUser );

            // Only render enabled users
            if( pUser->Enabled )
            {
                // If there are visible dialogs, render the stack
                if (pUser->DialogStack.GetCount())
                    RenderCursor = TRUE;
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
                    pUser->DialogStack[j]->Render( 0, 0 );
                }

                RenderNavText( pUser );
            }
        }

        RenderRefreshBar();

        // Only render the mouse cursor in mouse mode when dialogs are visible.
        if( RenderCursor )
        {
            // Get the last user.
            user* pUser = m_Users[m_Users.GetCount()-1];

            if( pUser->MouseVisible )
            {
                const f32 CursorX = (f32)pUser->MouseX;
                const f32 CursorY = (f32)pUser->MouseY;
                g_UIRenderer.DrawLine( vector2( CursorX - 3.0f, CursorY - 3.0f ),
                                       vector2( CursorX + 4.0f, CursorY + 4.0f ),
                                       XCOLOR_WHITE );
                g_UIRenderer.DrawLine( vector2( CursorX - 3.0f, CursorY + 4.0f ),
                                       vector2( CursorX + 4.0f, CursorY - 3.0f ),
                                       XCOLOR_WHITE );
            }
        }

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

    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );
    user* pUser = GetUserById( UserID );

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
        // Dialog positions are always expressed in logical UI coordinates.
        if( Flags & ui_win::WF_DLG_CENTER )
        {
            irect b = GetUserBounds( UserID );
            if( (b.GetWidth()  >= Position.GetWidth()) &&
                (b.GetHeight() >= Position.GetHeight()) )
            {
                Position.Translate( b.l + (b.GetWidth ()-Position.GetWidth ())/2 - Position.l,
                                    b.t + (b.GetHeight()-Position.GetHeight())/2 - Position.t );
            }
        }

        // Create the Dialog Window
        pDialog = (ui_dialog*)pFactory( UserID, this, pDialogTem, Position, pParent, Flags, pUserData );
        ASSERT( pDialog );

        LOG_MESSAGE( "ui_manager::OpenDialog", "New dialog opened. ID:0x%08x, Name:%s, Position:(%d,%d,%d,%d)", pDialog, ClassName, Position.l, Position.t, Position.r, Position.b );

        // If this is not a TAB dialog page
        if( !(pDialog->GetFlags() & ui_win::WF_TAB) )
        {
            // Add to the Dialog Stack
            if( pParent == NULL )
            {
                pUser->DialogStack.Append() = pDialog;
                pUser->DialogRevision++;
            }

            // Activate the dialog if it has controls
            if( !(Flags & ui_win::WF_NO_ACTIVATE) && (pDialog->m_Children.GetCount() > 0) )
            {
                ui_control* pFocusedControl = NULL;
                if( pDialogTem &&
                    (pDialogTem->FocusControl >= 0) &&
                    (pDialogTem->FocusControl < pDialog->GetNumControls()) )
                {
                    pFocusedControl = pDialog->GotoControl( pDialogTem->FocusControl );
                }

                for( s32 i = 0; !pFocusedControl && (i < pDialog->GetNumControls()); i++ )
                {
                    pFocusedControl = pDialog->GotoControl( i );
                }

                if( !pFocusedControl && (Flags & ui_win::WF_INPUTMODAL) )
                {
                    SetFocusWindow( UserID, pDialog );
                }
            }
        }
    }

    // Return pointer to new dialog
    return pDialog;
}

//=========================================================================

void ui_manager::EndDialog( s32 UserID, xbool ResetCursor )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );
    user*   pUser = GetUserById( UserID );
    s32     Count = pUser->DialogStack.GetCount();

    // Check if there are any dialogs to end
    if( Count > 0 )
    {
        // Get dialog pointer
        ui_dialog* pDialog = pUser->DialogStack[Count-1];

        (void)ResetCursor;

        // Clear focus if it was owned by this dialog
        if( pUser->pFocusedWindow )
        {
            if( (pUser->pFocusedWindow == (ui_win*)pDialog) ||
                (pUser->pFocusedWindow->IsChildOf( pDialog )) )
            {
                SetFocusWindow( UserID, NULL );
            }
        }

        // Clear hover if it was over this dialog
        if( pUser->pHoveredWindow )
        {
            if( (pUser->pHoveredWindow == (ui_win*)pDialog) ||
                (pUser->pHoveredWindow->IsChildOf( pDialog )) )
            {
                SetHoveredWindow( pUser, NULL );
            }
        }

        // Clear capture if it was owned by this dialog
        if( pUser->pCaptureWindow )
        {
            if( (pUser->pCaptureWindow == static_cast<ui_win*>( pDialog )) ||
                (pUser->pCaptureWindow->IsChildOf( pDialog )) )
            {
                pUser->pCaptureWindow->m_IsPressed = FALSE;
                ReleaseCapture( UserID );
            }
        }

        if( pUser->pPressedWindow )
        {
            if( (pUser->pPressedWindow == static_cast<ui_win*>( pDialog )) ||
                (pUser->pPressedWindow->IsChildOf( pDialog )) )
            {
                pUser->pPressedWindow->m_IsPressed = FALSE;
                pUser->pPressedWindow = NULL;
            }
        }

        // End the dialog
        pUser->DialogStack.Delete( Count-1 );
        pUser->DialogRevision++;

        if( pUser->DialogStack.GetCount() > 0 )
        {
            ui_dialog* pPreviousDialog = pUser->DialogStack[pUser->DialogStack.GetCount()-1];
            ui_control* pFocusedControl = NULL;
            if( (pPreviousDialog->GetControl() >= 0) &&
                (pPreviousDialog->GetControl() < pPreviousDialog->GetNumControls()) )
            {
                pFocusedControl = pPreviousDialog->GotoControl( pPreviousDialog->GetControl() );
            }

            for( s32 i = 0; !pFocusedControl && (i < pPreviousDialog->GetNumControls()); i++ )
            {
                pFocusedControl = pPreviousDialog->GotoControl( i );
            }

            if( !pFocusedControl )
            {
                SetFocusWindow( UserID, pPreviousDialog );
            }
        }

        LOG_MESSAGE( "ui_manager::EndDialog", "Dialog closed. ID:0x%08x", pDialog );
        if( m_CallbackDepth > 0 )
        {
            m_DeferredDialogs.Append() = pDialog;
        }
        else
        {
            delete pDialog;
        }
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
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );
    user*   pUser = GetUserById( UserID );

    // Return the number of stacked dialogs
    return pUser->DialogStack.GetCount();
}

//=========================================================================

ui_dialog* ui_manager::GetTopmostDialog( s32 UserID )
{
    ASSERT( (m_Users.Find( GetUserById( UserID ) )) != -1 );
    user*   pUser = GetUserById( UserID );

    if( pUser->DialogStack.GetCount() > 0 )
        return pUser->DialogStack.GetAt( pUser->DialogStack.GetCount()-1 );
    else
        return NULL;
}

//=========================================================================

void ui_manager::PushClipWindow( const irect &r )
{
    g_UIRenderer.PushClipRect( r );
}

//=========================================================================

void ui_manager::PopClipWindow( void )
{
    g_UIRenderer.PopClipRect();
}

//=========================================================================

void ui_manager::WordWrapString( s32 iFont, const irect& Rect, const char* pString, xwstring& Result )
{
    xwstring Text;

    if( !DecodeFontText( pString, Text ) )
    {
        x_throw( "Invalid UTF-8 text" );
        return;
    }

    m_Fonts[iFont]->pFont->TextWrap( Text, Rect, Result );
}

//=========================================================================

void ui_manager::WordWrapString( s32 iFont, const irect& Rect, const xwstring& Text, xwstring& Result )
{
    m_Fonts[iFont]->pFont->TextWrap( Text, Rect, Result );
}

//=========================================================================

texture* ui_manager::GetButtonTexture( s32 buttonCode )
{
    return (m_ButtonTextures[buttonCode].GetPointer());
}

//=========================================================================

//=========================================================================


s32 ui_manager::LookUpButtonCode( const xwchar* pString, s32 iStart, input_device Device, input_platform Platform ) const
{
    xwstring codeString;
    const xbool IsGamepad = (Device == INPUT_DEVICE_GAMEPAD);

    if( !pString )
        return -1;

 
    if( IsGamepad )
    {
        input_gadget GadgetID = input_GetPromptGadget( pString + iStart, Platform );
        if( GadgetID != INPUT_UNDEFINED )
        {
            s32 ButtonCode = GetButtonCodeForGadget( GadgetID );
            if( ButtonCode != -1 )
                return ButtonCode;
        }
    }
 
    codeString = pString + iStart;
    if( codeString.GetLength() == 0 ) return -1;
 
    const button_code* pButtonCodeTable = GetButtonCodeTable( Platform );
 
    for( s32 i = 0; i < NUM_BUTTON_CODES; i++ )
    {
        if( x_wstrcmp( codeString, pButtonCodeTable[i].CodeString ) == 0 )
        {
            s32 ButtonCode = pButtonCodeTable[i].ButtonCode;

            if( !IsGamepad && IsControllerButtonCode( ButtonCode ) )
                return -1;

            return ButtonCode;
        }
    }
 
    return -1;
}

//=========================================================================

void ui_manager::InitScreenWipe( ui_dialog* pOwner )
{
    m_wipeActive = TRUE;
    m_wipeFading = FALSE;
    m_wipeBounds.l = m_CurrScreenSize.l + 21;
    m_wipeBounds.t = m_CurrScreenSize.t + 8;
    m_wipeBounds.r = m_CurrScreenSize.r - 23;
    m_wipeBounds.b = m_CurrScreenSize.b - 8;
    m_pWipeOwner = pOwner;
    m_wipeRevealY = m_wipeBounds.t;
    m_wipeSpeed  = SCREEN_WIPE_SPEED;
    m_wipeHeadY  = MIN( (f32)m_wipeBounds.t + SCREEN_WIPE_HEAD_HEIGHT,
                        (f32)m_wipeBounds.b );
    m_wipeFade   = 1.0f;

    // play wipe sound effect
    g_AudioMgr.Play( "ScreenWipe" );
}

//=========================================================================

void ui_manager::RenderScreenWipe( const ui_dialog* pOwner )
{
    if( !IsWipeActiveFor( pOwner ) )
        return;

    const f32 Fade        = x_clamp( m_wipeFade, 0.0f, 1.0f );
    const f32 TrailLength = SCREEN_WIPE_TRAIL_LENGTH * Fade;
    if( TrailLength <= 0.0f )
        return;

    const f32 GradientTop = m_wipeHeadY - TrailLength;
    irect Gradient = m_wipeBounds;
    Gradient.t = MAX( Gradient.t, (s32)x_floor( GradientTop ) );
    Gradient.b = MIN( Gradient.b, (s32)x_floor( m_wipeHeadY + 0.5f ) );
    if( Gradient.b <= Gradient.t )
        return;

    // Keep the geometry inside the frame, as the original segmented wipe did.
    // Recalculate the endpoint colors so clipping does not alter the gradient.
    const f32 TopFactor = x_clamp( ((f32)Gradient.t - GradientTop) / TrailLength, 0.0f, 1.0f );
    const f32 BottomFactor = x_clamp( ((f32)Gradient.b - GradientTop) / TrailLength, 0.0f, 1.0f );
    const u8 TopIntensity = (u8)(255.0f * Fade * TopFactor + 0.5f);
    const u8 BottomIntensity = (u8)(255.0f * Fade * BottomFactor + 0.5f);
    const xcolor TopColor( (u8)((146 * TopIntensity) / 255),
                           (u8)((226 * TopIntensity) / 255),
                           (u8)((100 * TopIntensity) / 255),
                           TopIntensity );
    const xcolor BottomColor( (u8)((146 * BottomIntensity) / 255),
                              (u8)((226 * BottomIntensity) / 255),
                              (u8)((100 * BottomIntensity) / 255),
                              BottomIntensity );

    RenderGouraudRect( Gradient,
                       TopColor,
                       BottomColor,
                       BottomColor,
                       TopColor,
                       FALSE,
                       TRUE );
}

//=========================================================================

void ui_manager::UpdateScreenWipe( f32 DeltaTime )
{
    if( !m_wipeActive || (DeltaTime <= 0.0f) )
        return;

    f32 RemainingTime = DeltaTime;
    if( !m_wipeFading )
    {
        const f32 Distance = (f32)m_wipeBounds.b - m_wipeHeadY;
        const f32 SweepTime = (m_wipeSpeed > 0.0f) ? MAX( 0.0f, Distance ) / m_wipeSpeed : 0.0f;
        if( RemainingTime < SweepTime )
        {
            m_wipeHeadY += m_wipeSpeed * RemainingTime;
            RemainingTime = 0.0f;
        }
        else
        {
            m_wipeHeadY  = (f32)m_wipeBounds.b;
            m_wipeFading = TRUE;
            RemainingTime -= SweepTime;
        }
    }

    if( m_wipeFading && (RemainingTime > 0.0f) )
    {
        m_wipeFade -= RemainingTime / SCREEN_WIPE_FADE_DURATION;
        if( m_wipeFade <= 0.0f )
        {
            ResetScreenWipe();
            return;
        }
    }

    if( m_wipeFading )
    {
        m_wipeRevealY = m_wipeBounds.b;
    }
    else
    {
        m_wipeRevealY = MIN( m_wipeBounds.b,
                             MAX( m_wipeBounds.t,
                                  (s32)(m_wipeHeadY - SCREEN_WIPE_HEAD_HEIGHT + 0.5f) ) );
    }
}

//=========================================================================

xbool ui_manager::IsWipeActiveFor( const ui_dialog* pOwner ) const
{
    return m_wipeActive && (m_pWipeOwner == pOwner);
}

//=========================================================================
void ui_manager::ResetScreenWipe( void )
{
    m_wipeActive = FALSE;
    m_wipeFading = FALSE;
    m_pWipeOwner = NULL;
    m_wipeSpeed  = 0.0f;
    m_wipeHeadY  = 0.0f;
    m_wipeFade   = 0.0f;
    m_wipeBounds.Clear();
    m_wipeRevealY = 0;
}

//=============================================================================

void ui_manager::InitRefreshBar( void )
{
    m_RefreshSpeed  = 80;
    m_RefreshWidth  = 5;
    m_RefreshStepAccumulator = 0.0f;
    m_RefreshPos.l  = m_CurrScreenSize.l + 22;      
    m_RefreshPos.t  = m_CurrScreenSize.b - m_RefreshWidth;
    m_RefreshPos.r  = m_CurrScreenSize.r - 23;
    m_RefreshPos.b  = m_CurrScreenSize.b;
}

//=============================================================================

void ui_manager::RenderRefreshBar( void )
{

}

//=============================================================================

void ui_manager::UpdateRefreshBar( f32 deltaTime )
{
    const f32 VisualStep = 1.0f / 30.0f;

    m_RefreshStepAccumulator += deltaTime;

    while( m_RefreshStepAccumulator >= VisualStep )
    {
        m_RefreshStepAccumulator -= VisualStep;
        m_RefreshPos.t -= (s32)( ( m_RefreshSpeed * VisualStep ) + 0.5f );

        if( m_RefreshPos.t < m_CurrScreenSize.t )
            m_RefreshPos.t = m_CurrScreenSize.b - m_RefreshWidth;
    }
    
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
    m_ScreenHighlightID      = FindElement( "highlight" );
    m_ScreenGlowID           = FindElement( "screenglow" );
    m_HighlightAlpha         = 0.0f;
    m_ScreenHighlightEnabled = FALSE;
    m_HighlightFadeUp        = TRUE;
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
    u32 val = 64 + GetHighlightAlpha();
    RenderElement( m_ScreenHighlightID, m_ScreenHighlightPos, 0, xcolor(val,val,val,val), TRUE );
}

//=========================================================================

s32 ui_manager::GetHighlightAlpha( s32 cycle )
{
    s32 HighlightAlpha = (s32)(m_HighlightAlpha + 0.5f);
    s32 Phase;
    s32 Wave;

    if ( m_HighlightFadeUp )
    {
        Phase = HighlightAlpha;
    }
    else
    {
        Phase = (32 - HighlightAlpha);
    }

    Wave = Phase % (cycle * 2);

    if ( Wave > cycle )
    {
        Wave = (cycle * 2) - Wave;
    }

    return( Wave );
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
    u32 val = 128 + (GetHighlightAlpha() * 2);
    RenderElement( m_ScreenGlowID, pos, 0, xcolor(val,val,val,val), TRUE );
}

//=========================================================================

void ui_manager::InitGlowBar ( void )
{
    // Set up the glow bar data
    m_GlowID = FindElement( "glow" );

    m_GlowStartX    = m_CurrScreenSize.l + 46;
    m_GlowEndX      = m_CurrScreenSize.r - 46 - 16;

    m_GlowPos.l     = m_GlowStartX;
    m_GlowPos.t     = m_CurrScreenSize.t;
    m_GlowPos.r     = m_GlowStartX + 16;
    m_GlowPos.b     = m_CurrScreenSize.t + 7;

    m_GlowSpeed     = 120;
    m_GlowOnTop     = TRUE;
    m_GlowStepAccumulator = 0.0f;

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
            RenderElement(m_GlowID, m_GlowTrail[i], 0, xcolor(val,val,val,val), TRUE );
        }
    }
}

//=========================================================================

void ui_manager::UpdateGlowBar( f32 deltaTime )
{
    const f32 VisualStep = 1.0f / 30.0f;

    if (!m_ScreenIsOn)
        return;

    m_GlowStepAccumulator += deltaTime;

    while( m_GlowStepAccumulator >= VisualStep )
    {
        const s32 deltaPos = (s32)((m_GlowSpeed * VisualStep) + 0.5f);
        m_GlowStepAccumulator -= VisualStep;

        if (m_GlowOnTop)
        {
            m_GlowPos.l += deltaPos;
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
            m_GlowPos.l -= deltaPos;
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

    if( !eng_BeginFrame() )
    {
        return;
    }
    render::BeginFrame();

    if( eng_Begin("Progress Bar") )
    {
        // render the background image.
        RenderBackground( "loadscreen" );

        // render text
        irect rb( 196, 308, 316, 338 );
        RenderText( FindFont( "large" ), rb, ui_font::h_center, TextColor, g_StringTableMgr( "ui", "IDS_LOADING_MSG" ));

        // render the inner bar
        rb.l = 58;
        rb.t = 270;
        rb.b = 278;
        rb.r = rb.l + (s32)(s_ProgressBarScale * m_PercentLoaded);

        RenderRect(rb, xcolor(199,236,249,255), FALSE);

        // render the alien shell
        rb.l = 53;
        rb.r = 463;
        rb.t = 262;
        rb.b = 286;


        rb.l = 151;
        rb.t = 360;
        rb.r = 361;
        rb.b = 400;
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
        xwstring LevelString  = g_ActiveConfig.GetLevelName();
        RenderText( FindFont( "small" ), rb, ui_font::h_right, TextColor, LevelString );
#endif

        eng_End();
    }

    xbool const renderReady = render::EndFrame();
    xbool const engineReady = eng_EndFrame();
    if ( renderReady && !engineReady )
        x_throw( "UI: failed to submit progress frame" );
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

//=========================================================================

void ui_manager::BuildTextData( const xwchar* pString,
                                ui_text_data& Result,
                                xarray<s32>* pSourceOffsets ) const
{
    Result.Text.Clear();
    Result.Runs.Clear();

    if( pSourceOffsets )
    {
        pSourceOffsets->Clear();
    }

    const f32 InputHintSpacing = 6.0f;
    xwstring  Literal;
    xbool     HasInputButton = FALSE;

    for( s32 i = 0; pString && pString[i]; )
    {
        static const xwchar Prefix[] = { '{', 'I', 'n', 'p', 'u', 't', ':', 0 };

        if( x_wstrncmp( pString + i, Prefix, 7 ) == 0 )
        {
            s32 End = i + 7;

            while( pString[End] && pString[End] != '}' )
            {
                End++;
            }

            if( !pString[End] )
            {
                x_throw( "Unterminated input placeholder" );
                return;
            }

            xwstring Token;

            for( s32 j = i + 7; j < End; j++ )
            {
                Token += pString[j];
            }

            s32 Code = LookUpButtonCode( Token,
                                         0,
                                         g_Input.GetCurrentInputDevice(),
                                         g_Input.GetCurrentInputPlatform() );

            xbool HasSeparator = FALSE;
            if( Literal.GetLength() )
            {
                xwchar Last = Literal[Literal.GetLength() - 1];
                HasSeparator = ( Last == ' ' || Last == '\t' || Last == '\n' || Last == '\r' );
            }

            f32 Spacing = HasInputButton && !HasSeparator ? InputHintSpacing : 0.0f;

            Result.Append( Literal );
            Literal.Clear();

            if( Code >= 0 && Code < NUM_BUTTON_TEXTURES )
            {
                f32 Width = Code == INPUT_KBD_RETURN ? 24.0f : (f32)BUTTON_SPRITE_WIDTH;
                Result.AppendButton( Code, vector2( Width, (f32)BUTTON_SPRITE_WIDTH ), Spacing );
                HasInputButton = TRUE;

                if( pSourceOffsets )
                {
                    pSourceOffsets->Append() = i;
                }
            }

            i = End + 1;
        }
        else
        {
            if( pSourceOffsets )
            {
                pSourceOffsets->Append() = i;
            }

            Literal += pString[i++];
        }
    }

    Result.Append( Literal );

    if( pSourceOffsets )
    {
        pSourceOffsets->Append() = pString ? x_wstrlen( pString ) : 0;
    }
}

//=========================================================================

ui_font* ui_manager::GetFont( s32 Index ) const
{
    ASSERT( Index >= 0 && Index < m_Fonts.GetCount() );
    return m_Fonts[Index]->pFont;
}
