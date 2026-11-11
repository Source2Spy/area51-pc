//==============================================================================
//  
//  input_dinput.cpp
//  
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_WINDOWS
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
// INCLUDES 
//==============================================================================

#include "x_files.hpp"
#include "e_Input.hpp"
#include "Input/backend/input_backend.hpp"
#include "SDLEngine/sdleng_window.hpp"

#include <windows.h>

#define DIRECTINPUT_VERSION  0x0800
#include "dinput.h"
#include "xinput.h"
#pragma comment( lib, "xinput.lib" )

typedef HRESULT dxerr;

//==============================================================================
//  CONSTANTS AND DEFINES
//==============================================================================

#define MAX_DEVICES                  8
#define DIRECT_INPUT_BUFFER_CAPACITY 1024
#define DIRECT_INPUT_READ_BATCH_SIZE 256

// Intensity lost per second during rumble decay.
#ifdef X_RETAIL
#   define RUMBLE_DECAY_RATE 2.4f
#else
    static f32 RUMBLE_DECAY_RATE = 2.4f;
#endif

//==============================================================================
// TYPES
//==============================================================================

static byte const DIGITAL_ON = (1 << 0);

//-------------------------------------------------------------------------

enum feedback_type
{
    RT_NO_RUMBLE,
    RT_INTENSITY,
    RT_DECAY,
};

//-------------------------------------------------------------------------

struct rumble_controller
{
    feedback_type  Type;
    f32            Intensity;
    f32            DurationSec;
    xbool          Enabled;
};

//-------------------------------------------------------------------------

enum
{
    DIGITAL_COUNT_MOUSE    = INPUT_MOUSE_BTN_4             - INPUT_MOUSE_BTN_L + 1,
    DIGITAL_COUNT_KBD      = 256,

    RELATIVE_COUNT_MOUSE   = 3,

    XBOX_DIGITAL_COUNT     = INPUT_XBOX__DIGITAL_BUTTONS_END - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1,
    XBOX_ANALOG_BTN_COUNT  = INPUT_XBOX__ANALOG_BUTTONS_END  - INPUT_XBOX__ANALOG_BUTTONS_BEGIN  - 1,
    XBOX_STICK_COUNT       = INPUT_XBOX__STICKS_END          - INPUT_XBOX__STICKS_BEGIN           - 1,
};

//-------------------------------------------------------------------------

struct device
{
    IDirectInputDevice8*    pDevice;
};

//-------------------------------------------------------------------------

struct input_mouse
{
    byte    Digital[ DIGITAL_COUNT_MOUSE ];
    f32     Relative[ RELATIVE_COUNT_MOUSE ];
};

//-------------------------------------------------------------------------

struct input_keyboard
{
    byte    Digital[ DIGITAL_COUNT_KBD ];
};

//-------------------------------------------------------------------------

struct input_xbox_pad
{
    byte    Digital  [ XBOX_DIGITAL_COUNT    ];  // START, BACK, DPAD, L/R_STICK
    byte    AnalogBtn[ XBOX_ANALOG_BTN_COUNT ];  // LB, RB, A, B, X, Y, LT, RT (digital state)
    f32     Trigger  [ 2 ];                      // LT, RT analog (0..1)
    f32     Stick    [ XBOX_STICK_COUNT      ];  // LS_X, LS_Y, RS_X, RS_Y (-1..1)
};

//-------------------------------------------------------------------------

struct state
{
    input_keyboard  Keyboard[ MAX_DEVICES    ];
    input_mouse     Mouse   [ MAX_DEVICES    ];
    input_xbox_pad  XboxPad [ XUSER_MAX_COUNT];
};

//-------------------------------------------------------------------------

class dinput_input_backend : public input_backend
{
public:
    virtual xbool   Init                ( const input_init_desc& Desc );
    virtual void    Kill                ( void );

    virtual xbool   CaptureFrameInput   ( input_event_buffer& Events );
    virtual xbool   IsGadgetDown        ( input_gadget GadgetID, s32 DeviceID ) const;
    virtual f32     GetGadgetValue      ( input_gadget GadgetID, s32 DeviceID ) const;
    virtual xbool   IsGadgetPresent     ( input_gadget GadgetID, s32 DeviceID ) const;
    virtual xbool   IsDevicePresent     ( input_device Device, s32 DeviceID ) const;
    virtual s32     GetPadCount         ( void ) const;

    virtual void    Feedback            ( f32 Duration, f32 Intensity, s32 DeviceID );
    virtual void    Feedback            ( s32 Count, feedback_envelope* pEnvelope, s32 DeviceID );
    virtual void    EnableFeedback      ( xbool State, s32 DeviceID );
    virtual void    SuppressFeedback    ( xbool Suppress );
    virtual void    ClearFeedback       ( void );
};

//==============================================================================
//  STORAGE
//==============================================================================

static struct
{
    HWND            Window;
    IDirectInput8*  pDInput;
    s32             nMouses;
    s32             nKeyboards;

    device          Mouse   [ MAX_DEVICES ];
    device          Keyboard[ MAX_DEVICES ];

    s32             KeybdDevice[ MAX_DEVICES ];
    s32             MouseDevice[ MAX_DEVICES ];

    xbool           bExclusive;
    xbool           bForeground;
    xbool           bDisableWindowsKey;

    state           CurrentState;

    s64             CurrentTimeFrame;
    s64             LastTimeFrame;

    xbool           ExitApp;

    xbool           bXboxConnected[ XUSER_MAX_COUNT ];

} s_Input = {0};

//-------------------------------------------------------------------------

static struct
{
    rumble_controller Controller[ XUSER_MAX_COUNT ];
    xbool             Suppress;
} s_Rumble;

//-------------------------------------------------------------------------

static
xbool IsGamepadFeedbackAllowed( void )
{
    return( g_Input.GetCurrentInputDevice() == INPUT_DEVICE_GAMEPAD );
}

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

static dxerr CreateMouse       ( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize );
static dxerr CreateKeyboard    ( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize );
static void  input_KillBackend ( void );

static xbool s_DoNotProcessWindowsMessages = FALSE;

//=========================================================================
//  DEVICE STATE
//=========================================================================

static
void ClearRelativeState( state& State )
{
    for( s32 i = 0; i < s_Input.nMouses; i++ )
    {
        for( s32 a = 0; a < RELATIVE_COUNT_MOUSE; a++ )
        {
            State.Mouse[i].Relative[a] = 0;
        }
    }
}

//=========================================================================

static
void AppendDigitalEvent( input_event_buffer& Events,
                         input_gadget       GadgetID,
                         s32                DeviceID,
                         byte&              State,
                         xbool              IsDown,
                         f32                Value,
                         u32                TimeStamp )
{
    xbool const WasDown = (State & DIGITAL_ON) != 0;
    if( IsDown == WasDown )
    {
        return;
    }

    if( IsDown )
    {
        if( Events.Append( GadgetID, DeviceID, INPUT_EVENT_PRESSED, Value, TimeStamp ) )
        {
            State = DIGITAL_ON;
        }
    }
    else
    {
        if( Events.Append( GadgetID, DeviceID, INPUT_EVENT_RELEASED, 0.0f, TimeStamp ) )
        {
            State = 0;
        }
    }
}

//=========================================================================
//  DEVICE ENUMERATION
//=========================================================================

static
BOOL CALLBACK EnumKeyboardCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext )
{
    if( s_Input.nKeyboards >= MAX_DEVICES )
    {
        return DIENUM_STOP;
    }

    // Is the main keyboard? If so then do some quick nothing.
    //if( GUID_SysKeyboard == pdidInstance ) {}    
    dxerr Error = CreateKeyboard( s_Input.Keyboard[ s_Input.nKeyboards ],
                                  pdidInstance,
                                  DIRECT_INPUT_BUFFER_CAPACITY );
    
    // If it failed, then we can't use this Keyboard. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)    
    if( !FAILED( Error ) )
    {
        s_Input.nKeyboards++;
    }

    // If it failed, then we can't use this keyboard. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    return DIENUM_CONTINUE;
}

//=========================================================================

static
BOOL CALLBACK EnumMouseCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext )
{
    if( s_Input.nMouses >= MAX_DEVICES )
    {
        return DIENUM_STOP;
    }

    // Is the main mouse If so then do some quick nothing.
    //if( GUID_SysMouse == pdidInstance ) {}    
    dxerr Error = CreateMouse( s_Input.Mouse[ s_Input.nMouses ],
                               pdidInstance,
                               DIRECT_INPUT_BUFFER_CAPACITY );
    
    // If it failed, then we can't use this Mouse. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)    
    if( !FAILED( Error ) )
    {
        s_Input.nMouses++;
    }

    // If it failed, then we can't use this keyboard. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    return DIENUM_CONTINUE;
}

//=========================================================================
//  DEVICE CREATION
//=========================================================================

static
dxerr CreateMouse( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize )
{
    dxerr Error;
    DWORD dwCoopFlags;

    // Select the cooperative access mode for this device.
    dwCoopFlags  = s_Input.bExclusive  ? DISCL_EXCLUSIVE    : DISCL_NONEXCLUSIVE;
    dwCoopFlags |= s_Input.bForeground ? DISCL_FOREGROUND   : DISCL_BACKGROUND;

    // Obtain an interface to the system mouse device.
    Error = s_Input.pDInput->CreateDevice( pInstance->guidInstance, &Device.pDevice, NULL );
    if( FAILED( Error ) )
    {
        return Error;
    }

    // Set the data format to "mouse format" - a predefined data format 
    //
    // A data format specifies which controls on a device we
    // are interested in, and how they should be reported.
    //
    // This tells DirectInput that we will be passing a
    // DIMOUSESTATE2 structure to IDirectInputDevice::GetDeviceState.
    Error = Device.pDevice->SetDataFormat( &c_dfDIMouse2 );
    if( FAILED( Error ) )
    {
        return Error;
    }

    // Set the cooperativity level to let DirectInput know how
    // this device should interact with the system and with other
    // DirectInput applications_Input.
    Error = Device.pDevice->SetCooperativeLevel( s_Input.Window, dwCoopFlags );
    if( Error == DIERR_UNSUPPORTED && !s_Input.bForeground && s_Input.bExclusive )
    {
        input_KillBackend();
        MessageBox( s_Input.Window, "SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n"
                                    "For security reasons, background exclusive Mouse\n"
                                    "access is not allowed.", "Mouse", MB_OK );
        return Error;
    }
    if( FAILED( Error ) )
    {
        return Error;
    }

    DIPROPDWORD BufferSize;
    BufferSize.diph.dwSize       = sizeof(DIPROPDWORD);
    BufferSize.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    BufferSize.diph.dwObj        = 0;
    BufferSize.diph.dwHow        = DIPH_DEVICE;
    BufferSize.dwData            = SampleBufferSize;

    Error = Device.pDevice->SetProperty( DIPROP_BUFFERSIZE, &BufferSize.diph );
    if( FAILED( Error ) )
    {
        return Error;
    }

    // Acquire the newly created device
    Device.pDevice->Acquire();

    // Set the source slot for this mouse.
    s_Input.MouseDevice[ s_Input.nMouses ] = s_Input.nMouses;

    return Error;
}

//=========================================================================

static
dxerr CreateKeyboard( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize )
{
    dxerr Error;
    DWORD dwCoopFlags;

    // Select the cooperative access mode for this device.
    dwCoopFlags  = s_Input.bExclusive  ? DISCL_EXCLUSIVE    : DISCL_NONEXCLUSIVE;
    dwCoopFlags |= s_Input.bForeground ? DISCL_FOREGROUND   : DISCL_BACKGROUND;

    // Disabling the windows key is only allowed only if we are in foreground nonexclusive
    if( s_Input.bDisableWindowsKey && !s_Input.bExclusive && s_Input.bForeground )
    {
        dwCoopFlags |= DISCL_NOWINKEY;
    }

    // Obtain an interface to the keyboard device.
    Error = s_Input.pDInput->CreateDevice( pInstance->guidInstance, &Device.pDevice, NULL );
    if( FAILED( Error ) )
    {
        return Error;
    }

    // Set the data format to "keyboard format" - a predefined data format 
    //
    // A data format specifies which controls on a device we
    // are interested in, and how they should be reported.
    //
    // This tells DirectInput that we will be passing an array
    // of 256 bytes to IDirectInputDevice::GetDeviceState.
    Error = Device.pDevice->SetDataFormat( &c_dfDIKeyboard );
    if( FAILED( Error ) )
    {
        return Error;
    }

    // Set the cooperativity level to let DirectInput know how
    // this device should interact with the system and with other
    // DirectInput applications_Input.
    Error = Device.pDevice->SetCooperativeLevel( s_Input.Window, dwCoopFlags );
    if( Error == DIERR_UNSUPPORTED && !s_Input.bForeground && s_Input.bExclusive )
    {
        input_KillBackend();
        MessageBox( s_Input.Window, "SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n"
                                    "For security reasons, background exclusive keyboard\n"
                                    "access is not allowed.", "Keyboard", MB_OK );
        return Error;
    }
    if( FAILED( Error ) )
    {
        return Error;
    }

    DIPROPDWORD BufferSize;
    BufferSize.diph.dwSize       = sizeof(DIPROPDWORD);
    BufferSize.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    BufferSize.diph.dwObj        = 0;
    BufferSize.diph.dwHow        = DIPH_DEVICE;
    BufferSize.dwData            = SampleBufferSize;

    Error = Device.pDevice->SetProperty( DIPROP_BUFFERSIZE, &BufferSize.diph );
    if( FAILED( Error ) )
    {
        return Error;
    }

    // Acquire the newly created device
    Device.pDevice->Acquire();

    // Set the source slot for this keyboard.
    s_Input.KeybdDevice[ s_Input.nKeyboards ] = s_Input.nKeyboards;

    return Error;
}

//=========================================================================
//  BUFFERED DEVICE READS
//=========================================================================

static
input_gadget GetMouseButtonGadget( s32 ButtonIndex )
{
    static input_gadget const s_MouseButtons[DIGITAL_COUNT_MOUSE] =
    {
        INPUT_MOUSE_BTN_L,
        INPUT_MOUSE_BTN_R,
        INPUT_MOUSE_BTN_C,
        INPUT_MOUSE_BTN_0,
        INPUT_MOUSE_BTN_1,
        INPUT_MOUSE_BTN_2,
        INPUT_MOUSE_BTN_3,
        INPUT_MOUSE_BTN_4
    };

    ASSERT( (ButtonIndex >= 0) && (ButtonIndex < DIGITAL_COUNT_MOUSE) );
    return ((ButtonIndex >= 0) && (ButtonIndex < DIGITAL_COUNT_MOUSE))
         ? s_MouseButtons[ButtonIndex]
         : INPUT_UNDEFINED;
}

//=========================================================================

static
dxerr ReadKeyboardBufferedData( device& Device, s32 DeviceID, input_event_buffer& Events )
{
    dxerr Result = DI_OK;

    for( ;; )
    {
        DIDEVICEOBJECTDATA Data[DIRECT_INPUT_READ_BATCH_SIZE];
        DWORD ElementCount = DIRECT_INPUT_READ_BATCH_SIZE;
        dxerr const Error = Device.pDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
                                                           Data,
                                                           &ElementCount,
                                                           0 );

        if( Error == DI_BUFFEROVERFLOW )
        {
            Events.MarkOverflow();
        }
        else if( FAILED( Error ) )
        {
            Events.MarkOverflow();
            Result = Device.pDevice->Acquire();
            return Result;
        }

        for( u32 i = 0; i < ElementCount; i++ )
        {
            s32 const ScanCode = static_cast<s32>( Data[i].dwOfs );
            if( (ScanCode < 0) || (ScanCode >= DIGITAL_COUNT_KBD) )
            {
                continue;
            }

            input_gadget const GadgetID = static_cast<input_gadget>( INPUT_KBD__BEGIN + ScanCode );
            AppendDigitalEvent( Events,
                                GadgetID,
                                DeviceID,
                                s_Input.CurrentState.Keyboard[DeviceID].Digital[ScanCode],
                                (Data[i].dwData & 0x80) != 0,
                                1.0f,
                                Data[i].dwTimeStamp );
        }

        if( ElementCount < DIRECT_INPUT_READ_BATCH_SIZE )
        {
            return Error;
        }
    }
}

//=========================================================================

static
dxerr ReadMouseBufferedData( device& Device, s32 DeviceID, input_event_buffer& Events )
{
    dxerr Result = DI_OK;

    for( ;; )
    {
        DIDEVICEOBJECTDATA Data[DIRECT_INPUT_READ_BATCH_SIZE];
        DWORD ElementCount = DIRECT_INPUT_READ_BATCH_SIZE;
        dxerr const Error = Device.pDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
                                                           Data,
                                                           &ElementCount,
                                                           0 );

        if( Error == DI_BUFFEROVERFLOW )
        {
            Events.MarkOverflow();
        }
        else if( FAILED( Error ) )
        {
            Events.MarkOverflow();
            Result = Device.pDevice->Acquire();
            return Result;
        }

        for( u32 i = 0; i < ElementCount; i++ )
        {
            if( (Data[i].dwOfs >= DIMOFS_BUTTON0) && (Data[i].dwOfs <= DIMOFS_BUTTON7) )
            {
                s32 const ButtonIndex = static_cast<s32>( Data[i].dwOfs - DIMOFS_BUTTON0 );
                AppendDigitalEvent( Events,
                                    GetMouseButtonGadget( ButtonIndex ),
                                    DeviceID,
                                    s_Input.CurrentState.Mouse[DeviceID].Digital[ButtonIndex],
                                    (Data[i].dwData & 0x80) != 0,
                                    1.0f,
                                    Data[i].dwTimeStamp );
            }
            else if( (Data[i].dwOfs >= DIMOFS_X) && (Data[i].dwOfs <= DIMOFS_Z) )
            {
                s32 const AxisIndex = static_cast<s32>( (Data[i].dwOfs - DIMOFS_X) >> 2 );
                f32 const Value = static_cast<f32>( static_cast<s32>( Data[i].dwData ) );
                input_gadget const GadgetID = (AxisIndex == 0) ? INPUT_MOUSE_X_REL
                                             : (AxisIndex == 1) ? INPUT_MOUSE_Y_REL
                                                                : INPUT_MOUSE_WHEEL_REL;

                s_Input.CurrentState.Mouse[DeviceID].Relative[AxisIndex] += Value;
                Events.Append( GadgetID,
                               DeviceID,
                               INPUT_EVENT_RELATIVE,
                               Value,
                               Data[i].dwTimeStamp );
            }
        }

        if( ElementCount < DIRECT_INPUT_READ_BATCH_SIZE )
        {
            return Error;
        }
    }
}

//=========================================================================
//  FINAL-STATE RECONCILIATION
//=========================================================================

static
void ReconcileMouseButtons( device& Device,
                            s32 DeviceID,
                            input_event_buffer& Events,
                            u32 TimeStamp )
{
    DIMOUSESTATE2 State;
    ZeroMemory( &State, sizeof(State) );

    dxerr Error = Device.pDevice->GetDeviceState( sizeof(State), &State );
    if( FAILED( Error ) )
    {
        Error = Device.pDevice->Acquire();
        if( SUCCEEDED( Error ) )
        {
            Error = Device.pDevice->GetDeviceState( sizeof(State), &State );
        }
    }

    for( s32 i = 0; i < DIGITAL_COUNT_MOUSE; i++ )
    {
        const xbool IsDown = SUCCEEDED( Error ) && ((State.rgbButtons[i] & 0x80) != 0);
        AppendDigitalEvent( Events,
                            GetMouseButtonGadget( i ),
                            DeviceID,
                            s_Input.CurrentState.Mouse[DeviceID].Digital[i],
                            IsDown,
                            1.0f,
                            TimeStamp );
    }
}

//=========================================================================

static
void ReconcileKeyboardButtons( device& Device,
                               s32 DeviceID,
                               input_event_buffer& Events,
                               u32 TimeStamp )
{
    byte State[DIGITAL_COUNT_KBD];
    ZeroMemory( State, sizeof(State) );

    dxerr Error = Device.pDevice->GetDeviceState( sizeof(State), State );
    if( FAILED( Error ) )
    {
        Error = Device.pDevice->Acquire();
        if( SUCCEEDED( Error ) )
        {
            Error = Device.pDevice->GetDeviceState( sizeof(State), State );
        }
    }

    for( s32 i = 0; i < DIGITAL_COUNT_KBD; i++ )
    {
        const xbool IsDown = SUCCEEDED( Error ) && ((State[i] & 0x80) != 0);
        AppendDigitalEvent( Events,
                            static_cast<input_gadget>( INPUT_KBD__BEGIN + i ),
                            DeviceID,
                            s_Input.CurrentState.Keyboard[DeviceID].Digital[i],
                            IsDown,
                            1.0f,
                            TimeStamp );
    }
}

//=========================================================================
//  XINPUT READS
//=========================================================================

static
void ReadXboxPad( s32 DeviceID )
{
    XINPUT_STATE xState;
    s_Input.bXboxConnected[ DeviceID ] = ( XInputGetState( DeviceID, &xState ) == ERROR_SUCCESS );
    if( !s_Input.bXboxConnected[ DeviceID ] )
    {
        return;
    }

    const XINPUT_GAMEPAD& Pad  = xState.Gamepad;
    input_xbox_pad&       XPad = s_Input.CurrentState.XboxPad[ DeviceID ];

    // Digital buttons (START, BACK, DPAD, thumb clicks)
    static const struct { WORD Mask; s32 Idx; } s_DigMap[] =
    {
        { XINPUT_GAMEPAD_START,        INPUT_XBOX_BTN_START   - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_BACK,         INPUT_XBOX_BTN_BACK    - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_DPAD_LEFT,    INPUT_XBOX_BTN_LEFT    - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_DPAD_RIGHT,   INPUT_XBOX_BTN_RIGHT   - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_DPAD_UP,      INPUT_XBOX_BTN_UP      - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_DPAD_DOWN,    INPUT_XBOX_BTN_DOWN    - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_LEFT_THUMB,   INPUT_XBOX_BTN_L_STICK - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_RIGHT_THUMB,  INPUT_XBOX_BTN_R_STICK - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1 },
    };

    for( s32 i = 0; i < (s32)(sizeof(s_DigMap)/sizeof(s_DigMap[0])); i++ )
    {
        const xbool Pressed = (Pad.wButtons & s_DigMap[i].Mask) != 0;
        XPad.Digital[s_DigMap[i].Idx] = Pressed ? DIGITAL_ON : 0;
    }

    // Face buttons and bumpers (LB=WHITE, RB=BLACK, A, B, X, Y)
    static const struct { WORD Mask; s32 Idx; } s_BtnMap[] =
    {
        { XINPUT_GAMEPAD_LEFT_SHOULDER,  INPUT_XBOX_BTN_WHITE - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_RIGHT_SHOULDER, INPUT_XBOX_BTN_BLACK - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_A,              INPUT_XBOX_BTN_A     - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_B,              INPUT_XBOX_BTN_B     - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_X,              INPUT_XBOX_BTN_X     - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1 },
        { XINPUT_GAMEPAD_Y,              INPUT_XBOX_BTN_Y     - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1 },
    };

    for( s32 i = 0; i < (s32)(sizeof(s_BtnMap)/sizeof(s_BtnMap[0])); i++ )
    {
        const xbool Pressed = (Pad.wButtons & s_BtnMap[i].Mask) != 0;
        XPad.AnalogBtn[s_BtnMap[i].Idx] = Pressed ? DIGITAL_ON : 0;
    }

    // Triggers: digital state plus a normalized analog value.
    const s32 LTIdx = INPUT_XBOX_L_TRIGGER - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1;
    const s32 RTIdx = INPUT_XBOX_R_TRIGGER - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1;

    auto UpdateTrigger = []( byte& Slot, f32& Value, BYTE Raw )
    {
        Value = Raw / 255.0f;
        Slot = (Raw > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? DIGITAL_ON : 0;
    };

    UpdateTrigger( XPad.AnalogBtn[ LTIdx ], XPad.Trigger[0], Pad.bLeftTrigger  );
    UpdateTrigger( XPad.AnalogBtn[ RTIdx ], XPad.Trigger[1], Pad.bRightTrigger );

    // Normalize sticks to [-1..1], then apply a radial dead zone.
    // Use 32768 as divisor so -32768 maps exactly to -1.0.
    {
        const f32 StickInv = 1.0f / 32768.0f;

        f32 LX = (f32)Pad.sThumbLX * StickInv;
        f32 LY = (f32)Pad.sThumbLY * StickInv;
        f32 RX = (f32)Pad.sThumbRX * StickInv;
        f32 RY = (f32)Pad.sThumbRY * StickInv;

        // Radial deadzone: if the stick magnitude is inside the dead zone we zero
        // both axes so they don't drift; outside we rescale to fill the full range.
        const f32 LDZ = 0.30f;
        const f32 RDZ = 0.30f;

        f32 LMag = sqrtf( LX*LX + LY*LY );
        if( LMag > 1.0f )
        {
            LX /= LMag;
            LY /= LMag;
            LMag = 1.0f;
        }
        if( LMag < LDZ )
        {
            LX = 0.0f;
            LY = 0.0f;
        }
        else
        {
            f32 Scale = (LMag - LDZ) / (LMag * (1.0f - LDZ));
            LX *= Scale;
            LY *= Scale;
        }

        f32 RMag = sqrtf( RX*RX + RY*RY );
        if( RMag > 1.0f )
        {
            RX /= RMag;
            RY /= RMag;
            RMag = 1.0f;
        }
        if( RMag < RDZ )
        {
            RX = 0.0f;
            RY = 0.0f;
        }
        else
        {
            f32 Scale = (RMag - RDZ) / (RMag * (1.0f - RDZ));
            RX *= Scale;
            RY *= Scale;
        }

        XPad.Stick[ INPUT_XBOX_STICK_LEFT_X  - INPUT_XBOX__STICKS_BEGIN - 1 ] = LX;
        XPad.Stick[ INPUT_XBOX_STICK_LEFT_Y  - INPUT_XBOX__STICKS_BEGIN - 1 ] = LY;
        XPad.Stick[ INPUT_XBOX_STICK_RIGHT_X - INPUT_XBOX__STICKS_BEGIN - 1 ] = RX;
        XPad.Stick[ INPUT_XBOX_STICK_RIGHT_Y - INPUT_XBOX__STICKS_BEGIN - 1 ] = RY;
    }
}

//=========================================================================

static
s64 GetInputClock( void )
{
    return GetTickCount();
}

//=========================================================================

static
s64 GetInputTicksPerSecond( void )
{
    return 1000;
}

//=========================================================================

static
void CaptureXboxPadEvents( s32 DeviceID, input_event_buffer& Events, u32 TimeStamp )
{
    input_xbox_pad const Previous = s_Input.CurrentState.XboxPad[DeviceID];
    xbool const WasConnected = s_Input.bXboxConnected[DeviceID];

    ReadXboxPad( DeviceID );

    if( WasConnected && !s_Input.bXboxConnected[DeviceID] )
    {
        x_memset( &s_Input.CurrentState.XboxPad[DeviceID],
                  0,
                  sizeof( s_Input.CurrentState.XboxPad[DeviceID] ) );
    }

    input_xbox_pad& Current = s_Input.CurrentState.XboxPad[DeviceID];

    input_gadget const DigitalGadgets[XBOX_DIGITAL_COUNT] =
    {
        INPUT_XBOX_BTN_START,
        INPUT_XBOX_BTN_BACK,
        INPUT_XBOX_BTN_LEFT,
        INPUT_XBOX_BTN_RIGHT,
        INPUT_XBOX_BTN_UP,
        INPUT_XBOX_BTN_DOWN,
        INPUT_XBOX_BTN_L_STICK,
        INPUT_XBOX_BTN_R_STICK
    };

    for( s32 i = 0; i < XBOX_DIGITAL_COUNT; i++ )
    {
        xbool const WasDown = (Previous.Digital[i] & DIGITAL_ON) != 0;
        xbool const IsDown  = (Current.Digital[i]  & DIGITAL_ON) != 0;
        if( WasDown != IsDown )
        {
            Events.Append( DigitalGadgets[i],
                           DeviceID,
                           IsDown ? INPUT_EVENT_PRESSED : INPUT_EVENT_RELEASED,
                           IsDown ? 1.0f : 0.0f,
                           TimeStamp );
        }
    }

    input_gadget const AnalogButtonGadgets[XBOX_ANALOG_BTN_COUNT] =
    {
        INPUT_XBOX_BTN_WHITE,
        INPUT_XBOX_BTN_BLACK,
        INPUT_XBOX_BTN_A,
        INPUT_XBOX_BTN_B,
        INPUT_XBOX_BTN_X,
        INPUT_XBOX_BTN_Y,
        INPUT_XBOX_L_TRIGGER,
        INPUT_XBOX_R_TRIGGER
    };

    for( s32 i = 0; i < XBOX_ANALOG_BTN_COUNT; i++ )
    {
        xbool const WasDown = (Previous.AnalogBtn[i] & DIGITAL_ON) != 0;
        xbool const IsDown  = (Current.AnalogBtn[i]  & DIGITAL_ON) != 0;
        if( WasDown != IsDown )
        {
            f32 const Value = (i >= (XBOX_ANALOG_BTN_COUNT - 2))
                            ? Current.Trigger[i - (XBOX_ANALOG_BTN_COUNT - 2)]
                            : (IsDown ? 1.0f : 0.0f);
            Events.Append( AnalogButtonGadgets[i],
                           DeviceID,
                           IsDown ? INPUT_EVENT_PRESSED : INPUT_EVENT_RELEASED,
                           Value,
                           TimeStamp );
        }
    }

    input_gadget const TriggerGadgets[2] = { INPUT_XBOX_L_TRIGGER, INPUT_XBOX_R_TRIGGER };
    for( s32 i = 0; i < 2; i++ )
    {
        if( Current.Trigger[i] != Previous.Trigger[i] )
        {
            Events.Append( TriggerGadgets[i],
                           DeviceID,
                           INPUT_EVENT_ABSOLUTE,
                           Current.Trigger[i],
                           TimeStamp );
        }
    }

    input_gadget const StickGadgets[XBOX_STICK_COUNT] =
    {
        INPUT_XBOX_STICK_LEFT_X,
        INPUT_XBOX_STICK_LEFT_Y,
        INPUT_XBOX_STICK_RIGHT_X,
        INPUT_XBOX_STICK_RIGHT_Y
    };

    for( s32 i = 0; i < XBOX_STICK_COUNT; i++ )
    {
        if( Current.Stick[i] != Previous.Stick[i] )
        {
            Events.Append( StickGadgets[i],
                           DeviceID,
                           INPUT_EVENT_ABSOLUTE,
                           Current.Stick[i],
                           TimeStamp );
        }
    }
}

//=========================================================================

xbool dinput_input_backend::CaptureFrameInput( input_event_buffer& Events )
{
    s_Input.LastTimeFrame = s_Input.CurrentTimeFrame;
    if( s_Input.LastTimeFrame == 0 )
    {
        s_Input.LastTimeFrame = GetInputClock();
    }

    Events.BeginCapture( static_cast<u32>( s_Input.LastTimeFrame ) );
    ClearRelativeState( s_Input.CurrentState );

    if( !s_DoNotProcessWindowsMessages && !sdleng_WindowPumpMessages() )
    {
        s_Input.ExitApp = TRUE;
    }

    s_Input.CurrentTimeFrame = GetInputClock();
    u32 const TimeStamp = static_cast<u32>( s_Input.CurrentTimeFrame );

    if( s_Input.ExitApp )
    {
        Events.Append( INPUT_MSG_EXIT, 0, INPUT_EVENT_EXIT, 1.0f, TimeStamp );
    }

    if( !s_DoNotProcessWindowsMessages )
    {
        for( s32 i = 0; i < s_Input.nMouses; i++ )
        {
            ReadMouseBufferedData( s_Input.Mouse[i], i, Events );
            ReconcileMouseButtons( s_Input.Mouse[i], i, Events, TimeStamp );
        }

        for( s32 i = 0; i < s_Input.nKeyboards; i++ )
        {
            ReadKeyboardBufferedData( s_Input.Keyboard[i], i, Events );
            ReconcileKeyboardButtons( s_Input.Keyboard[i], i, Events, TimeStamp );
        }
    }

    for( s32 i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        CaptureXboxPadEvents( i, Events, TimeStamp );
    }

    f32 const DeltaSeconds = static_cast<f32>( s_Input.CurrentTimeFrame - s_Input.LastTimeFrame ) /
                             static_cast<f32>( GetInputTicksPerSecond() );
    xbool const AllowFeedback = IsGamepadFeedbackAllowed();

    for( s32 i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        if( !s_Input.bXboxConnected[i] )
        {
            continue;
        }

        rumble_controller& Controller = s_Rumble.Controller[i];
        XINPUT_VIBRATION Vibration = { 0, 0 };

        if( !AllowFeedback )
        {
            Controller.Type        = RT_NO_RUMBLE;
            Controller.Intensity   = 0.0f;
            Controller.DurationSec = 0.0f;
        }
        else if( Controller.Enabled && !s_Rumble.Suppress && (Controller.Type != RT_NO_RUMBLE) )
        {
            Controller.Intensity   -= RUMBLE_DECAY_RATE * DeltaSeconds;
            Controller.DurationSec -= DeltaSeconds;

            if( (Controller.Intensity > 0.0f) && (Controller.DurationSec > 0.0f) )
            {
                WORD const Speed = static_cast<WORD>( MIN( MAX( Controller.Intensity, 0.0f ), 1.0f ) * 65535.0f );
                Vibration.wLeftMotorSpeed  = Speed;
                Vibration.wRightMotorSpeed = Speed;
            }
            else
            {
                Controller.Type      = RT_NO_RUMBLE;
                Controller.Intensity = 0.0f;
            }
        }

        XInputSetState( i, &Vibration );
    }

    Events.EndCapture( TimeStamp );
    return s_Input.ExitApp;
}

//=========================================================================
//  BACKEND LIFETIME
//=========================================================================

static
void input_KillBackend( void )
{
    for( s32 i = 0; i < s_Input.nMouses; i++ )
    {
        if( s_Input.Mouse[i].pDevice )
        {
            s_Input.Mouse[i].pDevice->Unacquire();
            s_Input.Mouse[i].pDevice->Release();
            s_Input.Mouse[i].pDevice = NULL;
        }
    }

    for( s32 i = 0; i < s_Input.nKeyboards; i++ )
    {
        if( s_Input.Keyboard[i].pDevice )
        {
            s_Input.Keyboard[i].pDevice->Unacquire();
            s_Input.Keyboard[i].pDevice->Release();
            s_Input.Keyboard[i].pDevice = NULL;
        }
    }

    if( s_Input.pDInput )
    {
        s_Input.pDInput->Release();
        s_Input.pDInput = NULL;
    }

    s_Input.nMouses    = 0;
    s_Input.nKeyboards = 0;
}

//=========================================================================

static
void input_DoNotProcessWindowsMessages( void )
{
    s_DoNotProcessWindowsMessages = TRUE;
}

//=========================================================================

static
xbool input_InitBackend( HWND Window )
{
    dxerr Error;

    // Associate input with the application window.
    s_Input.Window             = Window;
    // Set backend defaults.
    s_Input.bExclusive         = FALSE;
    s_Input.bForeground        = TRUE;
    s_Input.bDisableWindowsKey = TRUE;

#ifndef X_EDITOR
    s_Input.bExclusive = TRUE;
#endif

    // Initialize all the devices indirections to -1
    for( s32 i = 0; i < MAX_DEVICES; i++ )
    {
        s_Input.KeybdDevice[i]  = -1;
        s_Input.MouseDevice[i]  = -1;
    }

    for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
    {
        s_Rumble.Controller[x].Enabled = TRUE;
    }

    // Create a DInput object
    Error = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                                IID_IDirectInput8, (VOID**)&s_Input.pDInput, NULL );
    if( FAILED( Error ) )
    {
        return FALSE;
    }

    // Create all the keyboards
    Error = s_Input.pDInput->EnumDevices( DI8DEVCLASS_KEYBOARD,
                                          EnumKeyboardCallback,
                                          NULL, DIEDFL_ATTACHEDONLY );
    if( FAILED( Error ) )
    {
        input_KillBackend();
        return FALSE;
    }

    // Create all the Mouses
    Error = s_Input.pDInput->EnumDevices( DI8DEVCLASS_POINTER,
                                          EnumMouseCallback,
                                          NULL, DIEDFL_ATTACHEDONLY );
    if( FAILED( Error ) )
    {
        input_KillBackend();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  RAW GADGET BACKEND
//=========================================================================

static
f32 GetRawBackendValue( s32 DeviceID, input_gadget GadgetID, xbool ReadAnalogTrigger )
{
    ASSERT( DeviceID >= 0 );
    ASSERT( DeviceID < MAX_DEVICES );

    if( (DeviceID < 0) || (DeviceID >= MAX_DEVICES) )
    {
        return 0.0f;
    }

    if( GadgetID < INPUT_KBD__END && GadgetID > INPUT_KBD__BEGIN )
    {
        s32 KeybdDevice = s_Input.KeybdDevice[ DeviceID ];
        ASSERT( KeybdDevice < MAX_DEVICES );

        if( (KeybdDevice >= 0) && (KeybdDevice < MAX_DEVICES) )
        {
            s32 Index = GadgetID - INPUT_KBD__DIGITAL + 1;
            ASSERT( Index >= 0 );
            ASSERT( Index < DIGITAL_COUNT_KBD );

            if( (Index >= 0) && (Index < DIGITAL_COUNT_KBD) )
            {
                return (f32)( s_Input.CurrentState.Keyboard[ KeybdDevice ].Digital[ Index ] & DIGITAL_ON );
            }
        }
    }

    if( GadgetID < INPUT_MOUSE__END && GadgetID > INPUT_MOUSE__BEGIN )
    {
        s32 MouseDevice = s_Input.MouseDevice[ DeviceID ];
        ASSERT( MouseDevice < MAX_DEVICES );

        if( (MouseDevice < 0) || (MouseDevice >= MAX_DEVICES) )
        {
            return 0.0f;
        }

        const input_mouse& Mouse = s_Input.CurrentState.Mouse[ MouseDevice ];

        for( s32 i = 0; i < DIGITAL_COUNT_MOUSE; i++ )
        {
            if( GadgetID == GetMouseButtonGadget( i ) )
            {
                return (f32)( Mouse.Digital[i] & DIGITAL_ON );
            }
        }

        switch( GadgetID )
        {
        case INPUT_MOUSE_X_REL:     return Mouse.Relative[0]; break;
        case INPUT_MOUSE_Y_REL:     return Mouse.Relative[1]; break;
        case INPUT_MOUSE_WHEEL_REL: return Mouse.Relative[2]; break;
        }
    }

    if( GadgetID < INPUT_XBOX__END && GadgetID > INPUT_XBOX__BEGIN )
    {
        if( DeviceID >= XUSER_MAX_COUNT || !s_Input.bXboxConnected[ DeviceID ] )
        {
            return 0;
        }

        const input_xbox_pad& XPad = s_Input.CurrentState.XboxPad[ DeviceID ];

        if( GadgetID > INPUT_XBOX__DIGITAL_BUTTONS_BEGIN && GadgetID < INPUT_XBOX__DIGITAL_BUTTONS_END )
        {
            s32 Index = GadgetID - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1;
            return (f32)( XPad.Digital[ Index ] & DIGITAL_ON );
        }

        if( GadgetID > INPUT_XBOX__ANALOG_BUTTONS_BEGIN && GadgetID < INPUT_XBOX__ANALOG_BUTTONS_END )
        {
            s32 Index = GadgetID - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1;
            if( GadgetID == INPUT_XBOX_L_TRIGGER || GadgetID == INPUT_XBOX_R_TRIGGER )
            {
                if( ReadAnalogTrigger )
                {
                    return (GadgetID == INPUT_XBOX_L_TRIGGER) ? XPad.Trigger[0] : XPad.Trigger[1];
                }

                return (f32)( XPad.AnalogBtn[ Index ] & DIGITAL_ON );
            }
            return (f32)( XPad.AnalogBtn[ Index ] & DIGITAL_ON );
        }

        if( GadgetID > INPUT_XBOX__STICKS_BEGIN && GadgetID < INPUT_XBOX__STICKS_END )
        {
            s32 Index = GadgetID - INPUT_XBOX__STICKS_BEGIN - 1;
            return XPad.Stick[ Index ];
        }
    }

    if( GadgetID == INPUT_MSG_EXIT )
    {
        return (f32)( s_Input.ExitApp );
    }

    return 0;
}

//=========================================================================

xbool dinput_input_backend::IsGadgetDown( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetRawBackendValue( DeviceID, GadgetID, FALSE ) != 0;
}

//=========================================================================

f32 dinput_input_backend::GetGadgetValue( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetRawBackendValue( DeviceID, GadgetID, TRUE );
}

//==============================================================================

xbool dinput_input_backend::IsGadgetPresent( input_gadget GadgetID, s32 DeviceID ) const
{
    ASSERT( DeviceID >= 0 );
    ASSERT( DeviceID < MAX_DEVICES );

    if( (DeviceID < 0) || (DeviceID >= MAX_DEVICES) )
    {
        return FALSE;
    }

    if( GadgetID == INPUT_MSG_EXIT )
    {
        return TRUE;
    }

    if( GadgetID > INPUT_KBD__BEGIN && GadgetID < INPUT_KBD__END )
    {
        s32 KeybdDevice = s_Input.KeybdDevice[ DeviceID ];
        return( (KeybdDevice >= 0) && (KeybdDevice < MAX_DEVICES) );
    }

    if( GadgetID > INPUT_MOUSE__BEGIN && GadgetID < INPUT_MOUSE__END )
    {
        s32 MouseDevice = s_Input.MouseDevice[ DeviceID ];
        return( (MouseDevice >= 0) && (MouseDevice < MAX_DEVICES) );
    }

    if( GadgetID > INPUT_XBOX__BEGIN && GadgetID < INPUT_XBOX__END )
    {
        if( DeviceID < 0 || DeviceID >= XUSER_MAX_COUNT )
        {
            return FALSE;
        }
        return s_Input.bXboxConnected[ DeviceID ];
    }

    return FALSE;
}

//==============================================================================

xbool dinput_input_backend::IsDevicePresent( input_device Device, s32 DeviceID ) const
{
    if( (DeviceID < 0) || (DeviceID >= MAX_DEVICES) )
    {
        return FALSE;
    }

    switch( Device )
    {
    case INPUT_DEVICE_KEYBOARD:
    {
        s32 const KeybdDevice = s_Input.KeybdDevice[ DeviceID ];
        return( (KeybdDevice >= 0) && (KeybdDevice < MAX_DEVICES) );
    }

    case INPUT_DEVICE_MOUSE:
    {
        s32 const MouseDevice = s_Input.MouseDevice[ DeviceID ];
        return( (MouseDevice >= 0) && (MouseDevice < MAX_DEVICES) );
    }

    case INPUT_DEVICE_GAMEPAD:
    {
        return (DeviceID < XUSER_MAX_COUNT) && s_Input.bXboxConnected[ DeviceID ];
    }

    default:
    {
        return FALSE;
    }
    }
}

//==============================================================================

s32 dinput_input_backend::GetPadCount( void ) const
{
    s32 nXbox = 0;
    for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
    {
        if( s_Input.bXboxConnected[x] )
        {
            nXbox++;
        }
    }
    return nXbox;
}

//==============================================================================
//  FEEDBACK
//==============================================================================

void dinput_input_backend::Feedback( f32 Duration, f32 Intensity, s32 DeviceID )
{
    if( DeviceID < 0 || DeviceID >= XUSER_MAX_COUNT )
    {
        return;
    }
    if( !s_Input.bXboxConnected[ DeviceID ] )
    {
        return;
    }
    if( !IsGamepadFeedbackAllowed() )
    {
        return;
    }

    rumble_controller& C = s_Rumble.Controller[ DeviceID ];
    C.Type        = RT_INTENSITY;
    C.Intensity  += Intensity * 2.0f;
    C.DurationSec = Duration;
}

//==============================================================================

void dinput_input_backend::Feedback( s32 Count, feedback_envelope* pEnvelope, s32 DeviceID )
{
    if( Count <= 0 || !pEnvelope )
    {
        return;
    }
    if( DeviceID < 0 || DeviceID >= XUSER_MAX_COUNT )
    {
        return;
    }
    if( !s_Input.bXboxConnected[ DeviceID ] )
    {
        return;
    }
    if( !IsGamepadFeedbackAllowed() )
    {
        return;
    }

    // Use the first envelope entry for intensity and duration.
    rumble_controller& C = s_Rumble.Controller[ DeviceID ];
    C.Type        = RT_INTENSITY;
    C.Intensity   = pEnvelope[0].Intensity;
    C.DurationSec = pEnvelope[0].Duration;
}

//=============================================================================

void dinput_input_backend::EnableFeedback( xbool state, s32 DeviceID )
{
    if( DeviceID < 0 || DeviceID >= XUSER_MAX_COUNT )
    {
        return;
    }

    s_Rumble.Controller[ DeviceID ].Enabled = state;

    if( !state )
    {
        s_Rumble.Controller[ DeviceID ].Type      = RT_NO_RUMBLE;
        s_Rumble.Controller[ DeviceID ].Intensity = 0.0f;
        XINPUT_VIBRATION silence = { 0, 0 };
        if( s_Input.bXboxConnected[ DeviceID ] )
        {
            XInputSetState( DeviceID, &silence );
        }
    }
}

//==============================================================================

void dinput_input_backend::SuppressFeedback( xbool Suppress )
{
    s_Rumble.Suppress = Suppress;

    if( Suppress )
    {
        XINPUT_VIBRATION silence = { 0, 0 };
        for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
        {
            if( s_Input.bXboxConnected[x] )
            {
                XInputSetState( x, &silence );
            }
        }
    }
}

//==============================================================================

void dinput_input_backend::ClearFeedback( void )
{
    XINPUT_VIBRATION silence = { 0, 0 };
    for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
    {
        s_Rumble.Controller[x].Type        = RT_NO_RUMBLE;
        s_Rumble.Controller[x].Intensity   = 0.0f;
        s_Rumble.Controller[x].DurationSec = 0.0f;
        if( s_Input.bXboxConnected[x] )
        {
            XInputSetState( x, &silence );
        }
    }
}

//==============================================================================
//  INPUT SYSTEM LIFETIME
//==============================================================================

xbool dinput_input_backend::Init( const input_init_desc& Desc )
{
    if( Desc.pWindow )
    {
        s_Input.Window = (HWND)Desc.pWindow;
    }

    if( s_Input.pDInput )
    {
        return TRUE;
    }

    if( !s_Input.Window )
    {
        return FALSE;
    }

    return input_InitBackend( s_Input.Window );
}

//==============================================================================

void dinput_input_backend::Kill( void )
{
    ClearFeedback();
    input_KillBackend();
}

//==============================================================================

input_backend* input_CreateDefaultBackend( void )
{
    return new dinput_input_backend;
}

//==============================================================================

void input_DestroyDefaultBackend( input_backend* pBackend )
{
    delete pBackend;
}

//==============================================================================
