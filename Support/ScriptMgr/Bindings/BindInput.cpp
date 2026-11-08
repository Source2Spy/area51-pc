//==============================================================================
//
//  BindInput.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "BindInput.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"
#include "Entropy.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
input_gadget ArgGadget( ScriptContext& Context, s32 Index )
{
    input_gadget G = input_system::LookupGadget( Context.ArgString(Index) );
    if( G == INPUT_UNDEFINED )
    {
        x_DebugMsg( "***\n*** SCRIPT WARNING [input] unknown gadget '%s'\n***\n", // Spam simulator
                    Context.ArgString(Index) );
        ASSERT( G != INPUT_UNDEFINED );
    }
    return G;
}

//==============================================================================
//  BINDING FUNCTIONS
//==============================================================================

static
s32 BindInputWasPressed( ScriptContext& Context )
{
    input_gadget G = ArgGadget( Context, 0 );
    Context.PushBool( G != INPUT_UNDEFINED ? g_Input.GetFrameSnapshot().WasPressed( G ) : FALSE );
    return 1;
}

//==============================================================================

static
s32 BindInputIsPressed( ScriptContext& Context )
{
    input_gadget G = ArgGadget( Context, 0 );
    Context.PushBool( G != INPUT_UNDEFINED ? g_Input.GetFrameSnapshot().IsPressed( G ) : FALSE );
    return 1;
}

//==============================================================================

static
s32 BindInputGetValue( ScriptContext& Context )
{
    input_gadget G = ArgGadget( Context, 0 );
    Context.PushFloat( G != INPUT_UNDEFINED ? g_Input.GetFrameSnapshot().GetValue( G ) : 0.0f );
    return 1;
}

//==============================================================================

static
s32 BindInputIsPresent( ScriptContext& Context )
{
    input_gadget G = ArgGadget( Context, 0 );
    Context.PushBool( G != INPUT_UNDEFINED ? g_Input.GetFrameSnapshot().IsPresent( G ) : FALSE );
    return 1;
}

//==============================================================================
//  REGISTER
//==============================================================================

void BindInput::Register( ScriptBackend* pBackend )
{
    pBackend->RegisterFunction( "input_waspressed", BindInputWasPressed );
    pBackend->RegisterFunction( "input_ispressed",  BindInputIsPressed  );
    pBackend->RegisterFunction( "input_getvalue",   BindInputGetValue   );
    pBackend->RegisterFunction( "input_ispresent",  BindInputIsPresent  );
}
