//==============================================================================
//  
//  d3deng_input.cpp
//  
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
// INCLUDES 
//==============================================================================

#include "..\e_Engine.hpp"

#define DIRECTINPUT_VERSION  0x0800
#include "dinput.h"
#include "xinput.h"
#pragma comment( lib, "xinput.lib" )

//==============================================================================
// DEFINES
//==============================================================================

#define MAX_DEVICES      8          // Maximum number of devices per type
#define REFRESH_RATE    15          // Times per second
#define MAX_STATES      64          // ( REFRESH_RATE - MAX_STATES ) is how long we can go without losing info
#define MAX_EVENTS      64          // How long DirectX can go before losing input

// Intensity lost per second during rumble decay.
#ifdef X_RETAIL
#   define RUMBLE_DECAY_RATE 2.4f
#else
    static f32 RUMBLE_DECAY_RATE = 2.4f;
#endif

//==============================================================================
// TYPES
//==============================================================================

enum digital_type
{
    DIGITAL_ON       = (1<<0),
    DIGITAL_DEBAUNCE = (1<<1),
};

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
    DIGITAL_COUNT_MOUSE    = INPUT_MOUSE__ANALOG           - INPUT_MOUSE__DIGITAL,
    DIGITAL_COUNT_KBD      = INPUT_KBD__END                - INPUT_KBD__DIGITAL,

    ANALOG_COUNT_MOUSE     = INPUT_MOUSE__END              - INPUT_MOUSE__ANALOG,

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
    f32     Anolog [ ANALOG_COUNT_MOUSE  ];
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
    s64             TimeStamp;
    input_keyboard  Keyboard[ MAX_DEVICES    ];
    input_mouse     Mouse   [ MAX_DEVICES    ];
    input_xbox_pad  XboxPad [ XUSER_MAX_COUNT];
};

//==============================================================================
// VARIABLES
//==============================================================================

static struct
{
    HWND            Window;
    IDirectInput8*  pDInput;
    MSG             Msg;
    xbool           bProcessEvents;

    s32             nMouses;
    s32             nKeyboards;

    device          Mouse   [ MAX_DEVICES ];
    device          Keyboard[ MAX_DEVICES ];

    s32             KeybdDevice[ MAX_DEVICES ];
    s32             MouseDevice[ MAX_DEVICES ];

    xbool           bExclusive;
    xbool           bForeground;
    xbool           bImmediate;
    xbool           bDisableWindowsKey;

    state           State[ MAX_STATES ];
    s32             nStates;
    s32             iState;
    s32             iStateNext;

    s64             CurrentTimeFrame;
    s64             LastTimeFrame;
    s64             TicksPerRefresh;

    xbool           ExitApp;

    xbool           bXboxConnected[ XUSER_MAX_COUNT ];

} s_Input = {0};

//-------------------------------------------------------------------------

static struct
{
    rumble_controller Controller[ XUSER_MAX_COUNT ];
    xbool             Suppress;
} s_Rumble;

//==============================================================================
// FUNCTIONS
//==============================================================================

static dxerr CreateMouse   ( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize );
static dxerr CreateKeyboard( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize );
void   d3deng_KillInput    ( void );

static xbool s_DoNotProcessWindowsMessages = FALSE;

//=========================================================================

static
void ClearDebounce( state& State )
{
    s32 i;

    // Clear all the debounce for the XInput
    for( i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        for( s32 d = 0; d < XBOX_DIGITAL_COUNT; d++ )
            State.XboxPad[i].Digital  [d] &= ~DIGITAL_DEBAUNCE;
        for( s32 d = 0; d < XBOX_ANALOG_BTN_COUNT; d++ )
            State.XboxPad[i].AnalogBtn[d] &= ~DIGITAL_DEBAUNCE;
    }

    // Clear all the debounce for the Mouse
    for( i = 0; i < s_Input.nMouses; i++ )
    {
        for( s32 d = 0; d < DIGITAL_COUNT_MOUSE; d++ )
            State.Mouse[i].Digital[d] &= ~DIGITAL_DEBAUNCE;

        for( s32 a = 0; a < ANALOG_COUNT_MOUSE; a++ )
            State.Mouse[i].Anolog[a] = 0;
    }

    // Clear all the debounce for the Keyboard
    for( i = 0; i < s_Input.nKeyboards; i++ )
        for( s32 d = 0; d < DIGITAL_COUNT_KBD; d++ )
            State.Keyboard[i].Digital[d] &= ~DIGITAL_DEBAUNCE;
}

//=========================================================================

static
void PageFlipQueue( void )
{
    //  Clear all the time stamps
    for( s32 i = 0; i < MAX_STATES; i++ )
        s_Input.State[ i ].TimeStamp = 0;

    // Prepare the first event in the queue
    if( s_Input.bImmediate )
    {
        s_Input.State[ 0 ].TimeStamp = s_Input.CurrentTimeFrame;
    }
    else
    {
        // Copy the previous old state		
        ASSERT( s_Input.nStates > 0 );
        // Update the time to our old one to make sure that we don't loose precision		
        s_Input.State[ 0 ]            = s_Input.State[ s_Input.nStates - 1 ];
        s_Input.State[ 0 ].TimeStamp  = s_Input.LastTimeFrame;
    }

    s_Input.iState     = 0;
    s_Input.nStates     = 1;        // We always have at least one state
    s_Input.iStateNext = 0;

    // Make sure that all the debounces are clear
    ClearDebounce( s_Input.State[0] );
}

//=========================================================================

static
state& GetState( s64 TimeStamp )
{
    s32 i = 0;

    // Try to find the right state
    while( TimeStamp > ( s_Input.State[ i ].TimeStamp + s_Input.TicksPerRefresh ) )
    {
        // Make sure that the next time interval is initialize		
        i++;
        ASSERT( i < MAX_STATES );
        // Copy the previous time frame		
        if( s_Input.State[ i ].TimeStamp == 0 )
        {
            s_Input.State[ i ]            = s_Input.State[ i-1 ];
            s_Input.State[ i ].TimeStamp += s_Input.TicksPerRefresh;

            // Clear the debounce for the new state
            ClearDebounce( s_Input.State[ i ] );

            // Update the queue count
            s_Input.nStates++;
            ASSERT( s_Input.nStates <= MAX_STATES );
        }
    }

    ASSERT( i < MAX_STATES );
    return s_Input.State[ i ];
}

//=========================================================================

static
BOOL CALLBACK EnumKeyboardCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext )
{
    // Is the main keyboard? If so then do some quick nothing.
    //if( GUID_SysKeyboard == pdidInstance ) {}	
    dxerr Error = CreateKeyboard( s_Input.Keyboard[ s_Input.nKeyboards ], pdidInstance, MAX_EVENTS );
	
    // If it failed, then we can't use this Keyboard. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)	
    if( !FAILED( Error ) )
        s_Input.nKeyboards++;

    // If it failed, then we can't use this keyboard. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    return DIENUM_CONTINUE;
}

//=========================================================================

static
BOOL CALLBACK EnumMouseCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext )
{
    // Is the main mouse If so then do some quick nothing.
    //if( GUID_SysMouse == pdidInstance ) {}	
    dxerr Error = CreateMouse( s_Input.Mouse[ s_Input.nMouses ], pdidInstance, MAX_EVENTS );
	
    // If it failed, then we can't use this Mouse. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)	
    if( !FAILED( Error ) )
        s_Input.nMouses++;

    // If it failed, then we can't use this keyboard. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    return DIENUM_CONTINUE;
}

//=========================================================================

static
dxerr CreateMouse( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize )
{
    dxerr Error;
    DWORD dwCoopFlags;

    // Detrimine where the buffer would like to be allocated 
    dwCoopFlags  = s_Input.bExclusive  ? DISCL_EXCLUSIVE    : DISCL_NONEXCLUSIVE;
    dwCoopFlags |= s_Input.bForeground ? DISCL_FOREGROUND   : DISCL_BACKGROUND;

    // Obtain an interface to the system mouse device.
    Error = s_Input.pDInput->CreateDevice( pInstance->guidInstance, &Device.pDevice, NULL );
    if( FAILED( Error ) )
        return Error;

    // Set the data format to "mouse format" - a predefined data format 
    //
    // A data format specifies which controls on a device we
    // are interested in, and how they should be reported.
    //
    // This tells DirectInput that we will be passing a
    // DIMOUSESTATE2 structure to IDirectInputDevice::GetDeviceState.
    Error = Device.pDevice->SetDataFormat( &c_dfDIMouse2 );
    if( FAILED( Error ) )
        return Error;

    // Set the cooperativity level to let DirectInput know how
    // this device should interact with the system and with other
    // DirectInput applications_Input.
    Error = Device.pDevice->SetCooperativeLevel( s_Input.Window, dwCoopFlags );
    if( Error == DIERR_UNSUPPORTED && !s_Input.bForeground && s_Input.bExclusive )
    {
        d3deng_KillInput();
        MessageBox( s_Input.Window, "SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n"
                                    "For security reasons, background exclusive Mouse\n"
                                    "access is not allowed.", "Mouse", MB_OK );
        return Error;
    }
    if( FAILED( Error ) )
        return Error;

    if( !s_Input.bImmediate )
    {
        // IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
        //
        // DirectInput uses unbuffered I/O (buffer size = 0) by default.
        // If you want to read buffered data, you need to set a nonzero
        // buffer size.
        //
        // Set the buffer size to SAMPLE_BUFFER_SIZE (defined above) elements_Input.
        //
        // The buffer size is a DWORD property associated with the device.
        DIPROPDWORD dipdw;
        dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj        = 0;
        dipdw.diph.dwHow        = DIPH_DEVICE;
        dipdw.dwData            = SampleBufferSize; // Arbitary buffer size

        Error = Device.pDevice->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph );
        if( FAILED( Error ) )
            return Error;
    }

    // Acquire the newly created device
    Device.pDevice->Acquire();

    // Set the device for this specific mouse
    s_Input.MouseDevice[ s_Input.nMouses ] = s_Input.nMouses;

    return Error;
}

//=========================================================================

static
dxerr CreateKeyboard( device& Device, const DIDEVICEINSTANCE* pInstance, s32 SampleBufferSize )
{
    dxerr Error;
    DWORD dwCoopFlags;

    // Detrimine where the buffer would like to be allocated 
    dwCoopFlags  = s_Input.bExclusive  ? DISCL_EXCLUSIVE    : DISCL_NONEXCLUSIVE;
    dwCoopFlags |= s_Input.bForeground ? DISCL_FOREGROUND   : DISCL_BACKGROUND;

    // Disabling the windows key is only allowed only if we are in foreground nonexclusive
    if( s_Input.bDisableWindowsKey && !s_Input.bExclusive && s_Input.bForeground )
        dwCoopFlags |= DISCL_NOWINKEY;

    // Obtain an interface to the keyboard device.
    Error = s_Input.pDInput->CreateDevice( pInstance->guidInstance, &Device.pDevice, NULL );
    if( FAILED( Error ) )
        return Error;

    // Set the data format to "keyboard format" - a predefined data format 
    //
    // A data format specifies which controls on a device we
    // are interested in, and how they should be reported.
    //
    // This tells DirectInput that we will be passing an array
    // of 256 bytes to IDirectInputDevice::GetDeviceState.
    Error = Device.pDevice->SetDataFormat( &c_dfDIKeyboard );
    if( FAILED( Error ) )
        return Error;

    // Set the cooperativity level to let DirectInput know how
    // this device should interact with the system and with other
    // DirectInput applications_Input.
    Error = Device.pDevice->SetCooperativeLevel( s_Input.Window, dwCoopFlags );
    if( Error == DIERR_UNSUPPORTED && !s_Input.bForeground && s_Input.bExclusive )
    {
        d3deng_KillInput();
        MessageBox( s_Input.Window, "SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n"
                                    "For security reasons, background exclusive keyboard\n"
                                    "access is not allowed.", "Keyboard", MB_OK );
        return Error;
    }
    if( FAILED( Error ) )
        return Error;

    if( !s_Input.bImmediate )
    {
        // IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
        //
        // DirectInput uses unbuffered I/O (buffer size = 0) by default.
        // If you want to read buffered data, you need to set a nonzero
        // buffer size.
        //
        // Set the buffer size to DINPUT_BUFFERSIZE (defined above) elements_Input.
        //
        // The buffer size is a DWORD property associated with the device.

        DIPROPDWORD dipdw;
        dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj        = 0;
        dipdw.diph.dwHow        = DIPH_DEVICE;
        dipdw.dwData            = SampleBufferSize; // Arbitary buffer size

        Error = Device.pDevice->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph );
        if( FAILED( Error ) )
            return Error;
    }

    // Acquire the newly created device
    Device.pDevice->Acquire();

    // Set the device for this specific keyboard
    s_Input.KeybdDevice[ s_Input.nKeyboards ] = s_Input.nKeyboards;

    return Error;
}

//=========================================================================

static
dxerr ReadKeyboadBufferedData( device& Device, s32 ID )
{
    DIDEVICEOBJECTDATA  didod[ MAX_EVENTS ];
    DWORD               dwElements = MAX_EVENTS;
    dxerr               Error;

    Error = Device.pDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), didod, &dwElements, 0 );
    if( Error != DI_OK )
    {
        Error = Device.pDevice->Acquire();
        while( Error == DIERR_INPUTLOST )
            Error = Device.pDevice->Acquire();

        return Error;
    }

    if( FAILED( Error ) )
        return Error;

    for( u32 i = 0; i < dwElements; i++ )
    {
        state&       State  = GetState( didod[ i ].dwTimeStamp );
        input_gadget Gadget = (input_gadget)( didod[ i ].dwOfs );

        ASSERT( Gadget >= 0 );
        ASSERT( Gadget < DIGITAL_COUNT_KBD );

        if( didod[ i ].dwData & 0x80 )
            State.Keyboard[ ID ].Digital[ Gadget ] |=  DIGITAL_ON | DIGITAL_DEBAUNCE;
        else
            State.Keyboard[ ID ].Digital[ Gadget ] &= ~DIGITAL_ON;
    }

    return Error;
}

//=========================================================================

static
dxerr ReadMouseBufferedData( device& Device, s32 ID )
{
    DIDEVICEOBJECTDATA  didod[ MAX_EVENTS ];
    DWORD               dwElements = MAX_EVENTS;
    dxerr               Error;

    Error = Device.pDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), didod, &dwElements, 0 );
    if( Error != DI_OK )
    {
        Error = Device.pDevice->Acquire();
        while( Error == DIERR_INPUTLOST )
            Error = Device.pDevice->Acquire();

        return Error;
    }

    if( FAILED( Error ) )
        return Error;

    for( u32 i = 0; i < dwElements; i++ )
    {
        state& State = GetState( didod[ i ].dwTimeStamp );

        if( didod[ i ].dwOfs >= DIMOFS_BUTTON0 && didod[ i ].dwOfs <= DIMOFS_BUTTON7 )
        {
            s32 Index = didod[ i ].dwOfs - DIMOFS_BUTTON0;
            ASSERT( Index >= 0 );
            ASSERT( Index < DIGITAL_COUNT_MOUSE );

            if( didod[ i ].dwData & 0x80 )
                State.Mouse[ ID ].Digital[ Index ] |=  DIGITAL_ON | DIGITAL_DEBAUNCE;
            else
                State.Mouse[ ID ].Digital[ Index ] &= ~DIGITAL_ON;
        }
        else if( didod[ i ].dwOfs >= DIMOFS_X && didod[ i ].dwOfs <= DIMOFS_Z )
        {
            s32 Index = (didod[ i ].dwOfs - DIMOFS_X) >> 2;
            ASSERT( Index >= 0 );
            ASSERT( Index < ANALOG_COUNT_MOUSE );
            State.Mouse[ ID ].Anolog[ Index ] = (f32)((s32)didod[ i ].dwData);
        }
    }

    return Error;
}

//=========================================================================

static
dxerr ReadMouseImmediateData( device& Device, s32 ID )
{
    dxerr         Error;
    DIMOUSESTATE2 dims2;

    ZeroMemory( &dims2, sizeof(dims2) );
    Error = Device.pDevice->GetDeviceState( sizeof(DIMOUSESTATE2), &dims2 );
    if( FAILED( Error ) )
    {
        Error = Device.pDevice->Acquire();
        while( Error == DIERR_INPUTLOST )
            Error = Device.pDevice->Acquire();

        return Error;
    }

    for( s32 i = 0; i < 8; i++ )
        s_Input.State[0].Mouse[ ID ].Digital[ i ] = (dims2.rgbButtons[ i ] & 0x80) != 0;

    s_Input.State[0].Mouse[ ID ].Anolog[0] = (f32)dims2.lX;
    s_Input.State[0].Mouse[ ID ].Anolog[1] = (f32)dims2.lY;
    s_Input.State[0].Mouse[ ID ].Anolog[2] = (f32)dims2.lZ;

    return Error;
}

//=========================================================================

static
dxerr ReadKeyboardImmediateData( device& Device, s32 ID )
{
    dxerr Error;
    byte  diks[256];

    ZeroMemory( &diks, sizeof(diks) );
    Error = Device.pDevice->GetDeviceState( sizeof(diks), &diks );
    if( FAILED( Error ) )
    {
        Error = Device.pDevice->Acquire();
        while( Error == DIERR_INPUTLOST )
            Error = Device.pDevice->Acquire();

        return Error;
    }

    for( s32 i = 0; i < ( INPUT_KBD__END - INPUT_KBD__BEGIN ); i++ )
    {
        xbool Pressed = ( diks[ i ] & 0x80 ) != 0;

        if( Pressed )
        {
            if( s_Input.State[0].Keyboard[ ID ].Digital[ i ] & DIGITAL_ON )
                s_Input.State[0].Keyboard[ ID ].Digital[ i ] &= ~DIGITAL_DEBAUNCE;
            else
                s_Input.State[0].Keyboard[ ID ].Digital[ i ] |= DIGITAL_ON | DIGITAL_DEBAUNCE;
        }
        else
        {
            s_Input.State[0].Keyboard[ ID ].Digital[ i ] &= ~(DIGITAL_ON | DIGITAL_DEBAUNCE);
        }
    }

    return Error;
}

//=========================================================================

static
void ReadXboxPad( s32 ID )
{
    XINPUT_STATE xState;
    s_Input.bXboxConnected[ ID ] = ( XInputGetState( ID, &xState ) == ERROR_SUCCESS );
    if( !s_Input.bXboxConnected[ ID ] )
        return;

    const XINPUT_GAMEPAD& Pad  = xState.Gamepad;
    input_xbox_pad&       XPad = s_Input.State[0].XboxPad[ ID ];

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
        xbool Pressed = ( Pad.wButtons & s_DigMap[i].Mask ) != 0;
        byte& Slot    = XPad.Digital[ s_DigMap[i].Idx ];
        if( Pressed )
        {
            if( Slot & DIGITAL_ON ) Slot &= ~DIGITAL_DEBAUNCE;
            else                    Slot |= DIGITAL_ON | DIGITAL_DEBAUNCE;
        }
        else
        {
            Slot &= ~(DIGITAL_ON | DIGITAL_DEBAUNCE);
        }
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
        xbool Pressed = ( Pad.wButtons & s_BtnMap[i].Mask ) != 0;
        byte& Slot    = XPad.AnalogBtn[ s_BtnMap[i].Idx ];
        if( Pressed )
        {
            if( Slot & DIGITAL_ON ) Slot &= ~DIGITAL_DEBAUNCE;
            else                    Slot |= DIGITAL_ON | DIGITAL_DEBAUNCE;
        }
        else
        {
            Slot &= ~(DIGITAL_ON | DIGITAL_DEBAUNCE);
        }
    }

    // Triggers — digital state + float analog value
    const s32 LTIdx = INPUT_XBOX_L_TRIGGER - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1;
    const s32 RTIdx = INPUT_XBOX_R_TRIGGER - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1;

    auto UpdateTrigger = [&]( byte& Slot, f32& Value, BYTE Raw )
    {
        Value = Raw / 255.0f;
        xbool Pressed = Raw > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        if( Pressed )
        {
            if( Slot & DIGITAL_ON ) Slot &= ~DIGITAL_DEBAUNCE;
            else                    Slot |= DIGITAL_ON | DIGITAL_DEBAUNCE;
        }
        else
        {
            Slot &= ~(DIGITAL_ON | DIGITAL_DEBAUNCE);
        }
    };

    UpdateTrigger( XPad.AnalogBtn[ LTIdx ], XPad.Trigger[0], Pad.bLeftTrigger  );
    UpdateTrigger( XPad.AnalogBtn[ RTIdx ], XPad.Trigger[1], Pad.bRightTrigger );

    // Sticks — normalize to [-1..1] then apply radial deadzone.
    // Use 32768 as divisor so -32768 maps exactly to -1.0.
    {
        const f32 StickInv = 1.0f / 32768.0f;

        f32 LX = (f32)Pad.sThumbLX * StickInv;
        f32 LY = (f32)Pad.sThumbLY * StickInv;
        f32 RX = (f32)Pad.sThumbRX * StickInv;
        f32 RY = (f32)Pad.sThumbRY * StickInv;

        // Radial deadzone: if the stick magnitude is inside the dead zone we zero
        // both axes so they don't drift; outside we rescale to fill the full range.
        const f32 LDZ = (f32)XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  * StickInv;
        const f32 RDZ = (f32)XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE * StickInv;

        f32 LMag = sqrtf( LX*LX + LY*LY );
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
xbool ProcessEvents( void )
{
    // Check whether we have more events to porcess
    if( s_Input.iStateNext >= s_Input.nStates )
        return FALSE;

    // Advance the process queue
    s_Input.iState = s_Input.iStateNext;
    ASSERT( s_Input.iState < MAX_STATES );

    // Check whether we have more events to porcess
    s_Input.iStateNext++;

    return TRUE;
}

//========================================================================

static
s64 TIME_GetClock( void )
{
    return GetTickCount();
}

//========================================================================

static
s64 TIME_GetTicksPerSecond( void )
{
    return 1000;
}

//=========================================================================

xbool input_UpdateState2( s32 Depth = 0 )
{
    // check whether we are getting events from the queue or we are collecting more events	
    if( s_Input.bProcessEvents )
    {
        // Process the next set of events		
        if( ProcessEvents() )
            return TRUE;

        // We are done processing events for this time interval
        s_Input.bProcessEvents = FALSE;
        return FALSE;
    }

    // Read all messages for the system
    if( !s_DoNotProcessWindowsMessages )
    {
        while( PeekMessage( &s_Input.Msg, NULL, 0, 0, PM_NOREMOVE ) )
        {
            if( !GetMessage( &s_Input.Msg, NULL, 0, 0 ) )
            {
                s_Input.ExitApp = TRUE;
                return FALSE;
            }

            TranslateMessage( &s_Input.Msg );
            DispatchMessage ( &s_Input.Msg );
        }
    }

    // Update the timer as well as the type of input query
    s_Input.LastTimeFrame    = s_Input.CurrentTimeFrame;
    s_Input.CurrentTimeFrame = TIME_GetClock();

    if( s_Input.TicksPerRefresh &&
        (( s_Input.CurrentTimeFrame - s_Input.LastTimeFrame ) / s_Input.TicksPerRefresh) > MAX_STATES )
    {
        s_Input.bImmediate = TRUE;
    }

    // Tell the queue to prepare since we are about to start collecting events
    if( Depth == 0 )
        PageFlipQueue();

    if( !s_DoNotProcessWindowsMessages )
    {
        // Read input for all the mouses	
        for( s32 m = 0; m < s_Input.nMouses; m++ )
        {
            if( s_Input.bImmediate )
                ReadMouseImmediateData( s_Input.Mouse[ m ], m );
            else
                ReadMouseBufferedData ( s_Input.Mouse[ m ], m );
        }

        // Read input for all the keyboards
        for( s32 k = 0; k < s_Input.nKeyboards; k++ )
        {
            if( s_Input.bImmediate )
                ReadKeyboardImmediateData( s_Input.Keyboard[ k ], k );
            else
                ReadKeyboadBufferedData  ( s_Input.Keyboard[ k ], k );
        }
    }

    // Read input for all the joysticks
    for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
        ReadXboxPad( x );

    {
        f32 DeltaSec = (f32)( s_Input.CurrentTimeFrame - s_Input.LastTimeFrame )
                     / (f32)TIME_GetTicksPerSecond();

        for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
        {
            if( !s_Input.bXboxConnected[x] )
                continue;

            rumble_controller& C   = s_Rumble.Controller[x];
            XINPUT_VIBRATION   vib = { 0, 0 };

            if( C.Enabled && !s_Rumble.Suppress && C.Type != RT_NO_RUMBLE )
            {
                C.Intensity   -= RUMBLE_DECAY_RATE * DeltaSec;
                C.DurationSec -= DeltaSec;

                if( C.Intensity <= 0.0f || C.DurationSec <= 0.0f )
                {
                    C.Type      = RT_NO_RUMBLE;
                    C.Intensity = 0.0f;
                }
                else
                {
                    WORD Speed = (WORD)( MIN( MAX( C.Intensity, 0.0f ), 1.0f ) * 65535.0f );
                    vib.wLeftMotorSpeed  = Speed;
                    vib.wRightMotorSpeed = Speed;
                }
            }

            XInputSetState( x, &vib );
        }
    }

    // Set the next stage of the input system
    s_Input.bProcessEvents = TRUE;

    return input_UpdateState2( Depth + 1 );
}

//=========================================================================

xbool input_UpdateState( void )
{
    return input_UpdateState2( 0 );
}

//=========================================================================

void d3deng_KillInput( void )
{
    for( s32 i = 0; i < s_Input.nMouses; i++ )
        if( s_Input.Mouse[i].pDevice )
        {
            s_Input.Mouse[i].pDevice->Unacquire();
            s_Input.Mouse[i].pDevice->Release();
            s_Input.Mouse[i].pDevice = NULL;
        }

    for( s32 i = 0; i < s_Input.nKeyboards; i++ )
        if( s_Input.Keyboard[i].pDevice )
        {
            s_Input.Keyboard[i].pDevice->Unacquire();
            s_Input.Keyboard[i].pDevice->Release();
            s_Input.Keyboard[i].pDevice = NULL;
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

void d3deng_DoNotProcessWindowsMessages()
{
    s_DoNotProcessWindowsMessages = TRUE;
}

//=========================================================================

xbool d3deng_InitInput( HWND Window )
{
    dxerr Error;

    // Set our input in contex of a window
    s_Input.Window             = Window;
    // Set our defauls
    s_Input.bExclusive         = FALSE;
    s_Input.bForeground        = TRUE;
    s_Input.bImmediate         = FALSE;
    s_Input.bDisableWindowsKey = TRUE;

#ifndef X_EDITOR
    s_Input.bExclusive = TRUE;
    s_Input.bImmediate = TRUE;
#endif

    s_Input.TicksPerRefresh = TIME_GetTicksPerSecond() / REFRESH_RATE;

    // Initialize all the devices indirections to -1
    for( s32 i = 0; i < MAX_DEVICES; i++ )
    {
        s_Input.KeybdDevice[i]  = -1;
        s_Input.MouseDevice[i]  = -1;
    }

    for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
        s_Rumble.Controller[x].Enabled = TRUE;

    // Create a DInput object
    Error = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                                IID_IDirectInput8, (VOID**)&s_Input.pDInput, NULL );
    if( FAILED( Error ) )
        return FALSE;

    // Create all the keyboards
    Error = s_Input.pDInput->EnumDevices( DI8DEVCLASS_KEYBOARD,
                                          EnumKeyboardCallback,
                                          NULL, DIEDFL_ATTACHEDONLY );
    if( FAILED( Error ) )
    {
        d3deng_KillInput();
        return FALSE;
    }

    // Create all the Mouses
    Error = s_Input.pDInput->EnumDevices( DI8DEVCLASS_POINTER,
                                          EnumMouseCallback,
                                          NULL, DIEDFL_ATTACHEDONLY );
    if( FAILED( Error ) )
    {
        d3deng_KillInput();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

static
f32 GetValue( s32 ControllerID, input_gadget GadgetID, digital_type DigitalType )
{
    ASSERT( ControllerID >= 0 );
    ASSERT( ControllerID < MAX_DEVICES );

    if( GadgetID < INPUT_KBD__END && GadgetID > INPUT_KBD__BEGIN )
    {
        s32 DeviceID = s_Input.KeybdDevice[ ControllerID ];
        if( DeviceID >= 0 )
        {
            s32 Index = GadgetID - INPUT_KBD__DIGITAL + 1;
            return (f32)( s_Input.State[ s_Input.iState ].Keyboard[ ControllerID ].Digital[ Index ] & DigitalType );
        }
    }

    if( GadgetID < INPUT_MOUSE__END && GadgetID > INPUT_MOUSE__BEGIN )
    {
        switch( GadgetID )
        {
        case INPUT_MOUSE_X_REL:     return s_Input.State[0].Mouse[ ControllerID ].Anolog[0];
        case INPUT_MOUSE_Y_REL:     return s_Input.State[0].Mouse[ ControllerID ].Anolog[1];
        case INPUT_MOUSE_WHEEL_REL: return s_Input.State[0].Mouse[ ControllerID ].Anolog[2];
        case INPUT_MOUSE_BTN_L:     return s_Input.State[0].Mouse[ ControllerID ].Digital[0];
        case INPUT_MOUSE_BTN_R:     return s_Input.State[0].Mouse[ ControllerID ].Digital[1];
        case INPUT_MOUSE_BTN_C:     return s_Input.State[0].Mouse[ ControllerID ].Digital[2];
        }
    }

    if( GadgetID < INPUT_XBOX__END && GadgetID > INPUT_XBOX__BEGIN )
    {
        if( ControllerID >= XUSER_MAX_COUNT || !s_Input.bXboxConnected[ ControllerID ] )
            return 0;

        const input_xbox_pad& XPad = s_Input.State[ s_Input.iState ].XboxPad[ ControllerID ];

        if( GadgetID > INPUT_XBOX__DIGITAL_BUTTONS_BEGIN && GadgetID < INPUT_XBOX__DIGITAL_BUTTONS_END )
        {
            s32 Index = GadgetID - INPUT_XBOX__DIGITAL_BUTTONS_BEGIN - 1;
            return (f32)( XPad.Digital[ Index ] & DigitalType );
        }

        if( GadgetID > INPUT_XBOX__ANALOG_BUTTONS_BEGIN && GadgetID < INPUT_XBOX__ANALOG_BUTTONS_END )
        {
            s32 Index = GadgetID - INPUT_XBOX__ANALOG_BUTTONS_BEGIN - 1;
            if( GadgetID == INPUT_XBOX_L_TRIGGER || GadgetID == INPUT_XBOX_R_TRIGGER )
            {
                // Debounce queries (input_WasPressed) use the digital AnalogBtn state,
                // everything else returns the normalized analog float (0..1).
                if( DigitalType == DIGITAL_DEBAUNCE )
                    return (f32)( XPad.AnalogBtn[ Index ] & DIGITAL_DEBAUNCE );
                return (GadgetID == INPUT_XBOX_L_TRIGGER) ? XPad.Trigger[0] : XPad.Trigger[1];
            }
            return (f32)( XPad.AnalogBtn[ Index ] & DigitalType );
        }

        if( GadgetID > INPUT_XBOX__STICKS_BEGIN && GadgetID < INPUT_XBOX__STICKS_END )
        {
            s32 Index = GadgetID - INPUT_XBOX__STICKS_BEGIN - 1;
            return XPad.Stick[ Index ];
        }
    }

    if( GadgetID == INPUT_MSG_EXIT )
        return (f32)( s_Input.ExitApp );

    return 0;
}

//=========================================================================

xbool input_WasPressed( input_gadget GadgetID, s32 ControllerID )
{
    return GetValue( ControllerID, GadgetID, DIGITAL_DEBAUNCE ) != 0;
}

//=========================================================================

xbool input_IsPressed( input_gadget GadgetID, s32 ControllerID )
{
    return GetValue( ControllerID, GadgetID, DIGITAL_ON ) != 0;
}

//=========================================================================

f32 input_GetValue( input_gadget GadgetID, s32 ControllerID )
{
    return GetValue( ControllerID, GadgetID, DIGITAL_ON );
}

//==============================================================================

void input_Feedback( f32 Duration, f32 Intensity, s32 ControllerID )
{
    if( ControllerID < 0 || ControllerID >= XUSER_MAX_COUNT )
        return;
    if( !s_Input.bXboxConnected[ ControllerID ] )
        return;

    rumble_controller& C = s_Rumble.Controller[ ControllerID ];
    C.Type        = RT_INTENSITY;
    C.Intensity  += Intensity * 2.0f;
    C.DurationSec = Duration;
}

//==============================================================================

void input_Feedback( s32 Count, feedback_envelope* pEnvelope, s32 ControllerID )
{
    if( Count <= 0 || !pEnvelope )
        return;
    if( ControllerID < 0 || ControllerID >= XUSER_MAX_COUNT )
        return;
    if( !s_Input.bXboxConnected[ ControllerID ] )
        return;

    // Use the first envelope entry for intensity and duration.
    rumble_controller& C = s_Rumble.Controller[ ControllerID ];
    C.Type        = RT_INTENSITY;
    C.Intensity   = pEnvelope[0].Intensity;
    C.DurationSec = pEnvelope[0].Duration;
}

//=============================================================================

void input_EnableFeedback( xbool state, s32 ControllerID )
{
    if( ControllerID < 0 || ControllerID >= XUSER_MAX_COUNT )
        return;

    s_Rumble.Controller[ ControllerID ].Enabled = state;

    if( !state )
    {
        s_Rumble.Controller[ ControllerID ].Type      = RT_NO_RUMBLE;
        s_Rumble.Controller[ ControllerID ].Intensity = 0.0f;
        XINPUT_VIBRATION silence = { 0, 0 };
        if( s_Input.bXboxConnected[ ControllerID ] )
            XInputSetState( ControllerID, &silence );
    }
}

//==============================================================================

void input_SuppressFeedback( xbool Suppress )
{
    s_Rumble.Suppress = Suppress;

    if( Suppress )
    {
        XINPUT_VIBRATION silence = { 0, 0 };
        for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
            if( s_Input.bXboxConnected[x] )
                XInputSetState( x, &silence );
    }
}

//==============================================================================

void input_Kill( void )
{
    input_ClearFeedback();
    d3deng_KillInput();
}

//==============================================================================

xbool input_IsPresent( input_gadget GadgetID, s32 ControllerID )
{
    if( GadgetID > INPUT_XBOX__BEGIN && GadgetID < INPUT_XBOX__END )
    {
        if( ControllerID < 0 || ControllerID >= XUSER_MAX_COUNT )
            return FALSE;
        return s_Input.bXboxConnected[ ControllerID ];
    }
    return TRUE;
}

//==============================================================================

s32 input_GetPadCount( void )
{
    s32 nXbox = 0;
    for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
        if( s_Input.bXboxConnected[x] )
            nXbox++;
    return nXbox;
}

//==============================================================================

void input_ClearFeedback( void )
{
    XINPUT_VIBRATION silence = { 0, 0 };
    for( s32 x = 0; x < XUSER_MAX_COUNT; x++ )
    {
        s_Rumble.Controller[x].Type      = RT_NO_RUMBLE;
        s_Rumble.Controller[x].Intensity = 0.0f;
        s_Rumble.Controller[x].DurationSec = 0.0f;
        if( s_Input.bXboxConnected[x] )
            XInputSetState( x, &silence );
    }
}

//==============================================================================