//==============================================================================
//
//  bind_input.cpp
//
//==============================================================================

#include "bind_input.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"
#include "Entropy.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
input_gadget ArgGadget( script_context& ctx, s32 Index )
{
    input_gadget G = input_LookupGadget( ctx.ArgString(Index) );
    if( G == INPUT_UNDEFINED )
	{
        x_DebugMsg( "***\n*** SCRIPT WARNING [input] unknown gadget '%s'\n***\n", // Spam simulator
                    ctx.ArgString(Index) );
        ASSERT( G != INPUT_UNDEFINED );
	}
    return G;
}

//==============================================================================
//  BINDING FUNCTIONS
//==============================================================================

static 
s32 bind_input_waspressed( script_context& ctx )
{
    input_gadget G = ArgGadget( ctx, 0 );
    ctx.PushBool( G != INPUT_UNDEFINED ? input_WasPressed(G) : FALSE );
    return 1;
}

//==============================================================================

static 
s32 bind_input_ispressed( script_context& ctx )
{
    input_gadget G = ArgGadget( ctx, 0 );
    ctx.PushBool( G != INPUT_UNDEFINED ? input_IsPressed(G) : FALSE );
    return 1;
}

//==============================================================================

static 
s32 bind_input_getvalue( script_context& ctx )
{
    input_gadget G = ArgGadget( ctx, 0 );
    ctx.PushFloat( G != INPUT_UNDEFINED ? input_GetValue(G) : 0.0f );
    return 1;
}

//==============================================================================

static 
s32 bind_input_ispresent( script_context& ctx )
{
    input_gadget G = ArgGadget( ctx, 0 );
    ctx.PushBool( G != INPUT_UNDEFINED ? input_IsPresent(G) : FALSE );
    return 1;
}

//==============================================================================
//  REGISTER
//==============================================================================

void bind_input::Register( script_backend* pBackend )
{
    pBackend->RegisterFunction( "input_waspressed", bind_input_waspressed );
    pBackend->RegisterFunction( "input_ispressed",  bind_input_ispressed  );
    pBackend->RegisterFunction( "input_getvalue",   bind_input_getvalue   );
    pBackend->RegisterFunction( "input_ispresent",  bind_input_ispresent  );
}
