//==============================================================================
//
//  BindSystem.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "BindSystem.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"

//==============================================================================
//  BINDING FUNCTIONS
//==============================================================================

static
s32 BindPrint( ScriptContext& Context )
{
    const char* pMsg = Context.ArgString( 0 );
    x_printf( "[ESCRIPT] %s\n", pMsg );
    return 0;
}

//==============================================================================

static
s32 BindLog( ScriptContext& Context )
{
    const char* pMsg = Context.ArgString( 0 );
    x_DebugMsg( "[ESCRIPT] %s\n", pMsg );
    return 0;
}

//==============================================================================
//  REGISTER
//==============================================================================

void BindSystem::Register( ScriptBackend* pBackend )
{
    pBackend->RegisterFunction( "print", BindPrint );
    pBackend->RegisterFunction( "log",   BindLog   );
}
