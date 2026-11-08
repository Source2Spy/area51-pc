//==============================================================================
//
//  ScriptBindings.hpp
//
//==============================================================================

//==============================================================================
//
//  Base class for a binding module, a self-contained group of script functions.
//
//  To add a new module:
//    1. Subclass ScriptBindings in Bindings/BindMyModule.hpp
//    2. Override GetModuleName() and Register()
//    3. Add it to ScriptMgr::Init()
//
//  Example:
//
//    class BindMyModule : public ScriptBindings
//    {
//    public:
//        const char* GetModuleName ( void ) const { return "mymodule"; }
//        void        Register      ( ScriptBackend* pBackend );
//    };
//
//    void BindMyModule::Register( ScriptBackend* pBackend )
//    {
//        pBackend->RegisterFunction( "my_func", BindMyFunc );
//    }
//
//==============================================================================

#ifndef SCRIPT_BINDINGS_HPP
#define SCRIPT_BINDINGS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "ScriptBackend.hpp"

//==============================================================================
//  SCRIPT BINDINGS
//==============================================================================

class ScriptBindings
{
public:
    virtual                ~ScriptBindings  ( void ) {}

    virtual const char*     GetModuleName   ( void ) const = 0;
    virtual void            Register        ( ScriptBackend* pBackend ) = 0;
};

//==============================================================================
#endif // SCRIPT_BINDINGS_HPP
//==============================================================================
