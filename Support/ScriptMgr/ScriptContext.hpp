//==============================================================================
//
//  ScriptContext.hpp
//
//==============================================================================

//==============================================================================
//
//  Abstract call context, the only interface binding functions use.
//  Binding code never includes any backend headers (lua.h, etc.).
//
//  Usage in a binding function:
//
//    static s32 BindMyFn( ScriptContext& Context )
//    {
//        f32 X = Context.ArgFloat( 0 );   // first argument
//        f32 Y = Context.ArgFloat( 1 );   // second argument
//        Context.PushFloat( X + Y );      // push return value
//        return 1;                        // number of return values pushed
//    }
//
//  Arguments are 0-indexed regardless of backend.
//
//==============================================================================

#ifndef SCRIPT_CONTEXT_HPP
#define SCRIPT_CONTEXT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  SCRIPT CONTEXT
//==============================================================================

class ScriptContext
{
public:
    virtual                ~ScriptContext   ( void ) {}

    //--------------------------------------------------------------------------
    //  Arguments  (0-indexed)
    //--------------------------------------------------------------------------

    virtual s32             ArgCount        ( void )        const = 0;

    virtual xbool           ArgBool         ( s32 Index )   const = 0;
    virtual s32             ArgInt          ( s32 Index )   const = 0;
    virtual u64             ArgU64          ( s32 Index )   const = 0;  // For guids
    virtual f32             ArgFloat        ( s32 Index )   const = 0;
    virtual const char*     ArgString       ( s32 Index )   const = 0;

    //--------------------------------------------------------------------------
    //  Return values (push each value, return the total count from your fn)
    //--------------------------------------------------------------------------

    virtual void            PushBool        ( xbool         Value ) = 0;
    virtual void            PushInt         ( s32           Value ) = 0;
    virtual void            PushU64         ( u64           Value ) = 0;  // For guids
    virtual void            PushFloat       ( f32           Value ) = 0;
    virtual void            PushString      ( const char*   Value ) = 0;
    virtual void            PushNil         ( void )                = 0;
};

//==============================================================================

typedef s32 (*ScriptFn)( ScriptContext& Context );

//==============================================================================
#endif // SCRIPT_CONTEXT_HPP
//==============================================================================
