//==============================================================================
//
//  script_bindings.hpp
//
//==============================================================================

//==============================================================================
//
//  Base class for a binding module, a self-contained group of script functions.
//
//  To add a new module:
//    1. Subclass script_bindings in Bindings/bind_mymodule.hpp
//    2. Override GetModuleName() and Register()
//    3. Add it to script_mgr::Init()
//
//  Example:
//
//    class bind_mymodule : public script_bindings
//    {
//    public:
//        const char* GetModuleName ( void ) const { return "mymodule"; }
//        void        Register      ( script_backend* pBackend );
//    };
//
//    void bind_mymodule::Register( script_backend* pBackend )
//    {
//        pBackend->RegisterFunction( "my_func", bind_my_func );
//    }
//
//==============================================================================

#ifndef SCRIPT_BINDINGS_HPP
#define SCRIPT_BINDINGS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "script_backend.hpp"

//==============================================================================
//  SCRIPT_BINDINGS
//==============================================================================

class script_bindings
{
public:
    virtual                ~script_bindings ( void ) {}

    virtual const char*     GetModuleName   ( void ) const = 0;
    virtual void            Register        ( script_backend* pBackend ) = 0;
};

//==============================================================================
#endif // SCRIPT_BINDINGS_HPP
//==============================================================================
