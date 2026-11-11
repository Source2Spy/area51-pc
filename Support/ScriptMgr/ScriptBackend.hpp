//==============================================================================
//
//  ScriptBackend.hpp
//
//==============================================================================

#ifndef SCRIPT_BACKEND_HPP
#define SCRIPT_BACKEND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "ScriptContext.hpp"

//==============================================================================
//  SCRIPT BACKEND
//==============================================================================

class ScriptBackend
{
public:
    virtual                ~ScriptBackend   ( void ) {}

    //--------------------------------------------------------------------------
    //  Lifecycle
    //--------------------------------------------------------------------------

    virtual void            Init            ( void ) = 0;
    virtual void            Kill            ( void ) = 0;

    //--------------------------------------------------------------------------
    //  Execution
    //--------------------------------------------------------------------------

    // Run a script file from disk.
    virtual xbool           RunFile         ( const char* pPath ) = 0;

    // Run a string of script code directly.
    virtual xbool           RunString       ( const char* pCode ) = 0;

    // Call a named global function with no arguments.
    virtual xbool           CallFunction    ( const char* pFuncName ) = 0;

    // Call OnUpdate( dt ) if it is defined.
    virtual void            Update          ( f32 DeltaTime ) = 0;

    //--------------------------------------------------------------------------
    //  Binding registration
    //
    //  Maps a script-visible name to a backend-agnostic ScriptFn.
    //  The backend is responsible for bridging ScriptFn <-> its call convention.
    //--------------------------------------------------------------------------

    virtual void            RegisterFunction( const char* pName, ScriptFn Fn ) = 0;

    //--------------------------------------------------------------------------
    //  Info
    //--------------------------------------------------------------------------

    virtual const char*     GetBackendName  ( void ) const = 0;
};

//==============================================================================
#endif // SCRIPT_BACKEND_HPP
//==============================================================================
