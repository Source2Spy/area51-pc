//==============================================================================
//
//  bind_system.cpp
//
//==============================================================================

#include "bind_system.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

static s32 bind_print( script_context& ctx )
{
    const char* pMsg = ctx.ArgString( 0 );
    x_printf( "[ESCRIPT] %s\n", pMsg );
    return 0;
}

//==============================================================================

static s32 bind_log( script_context& ctx )
{
    const char* pMsg = ctx.ArgString( 0 );
    x_DebugMsg( "[ESCRIPT] %s\n", pMsg );
    return 0;
}

//==============================================================================
//  REGISTER
//==============================================================================

void bind_system::Register( script_backend* pBackend )
{
    pBackend->RegisterFunction( "print", bind_print );
    pBackend->RegisterFunction( "log",   bind_log   );
}

//==============================================================================