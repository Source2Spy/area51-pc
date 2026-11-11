//==============================================================================
//
//  BindInput.hpp
//
//==============================================================================

//==============================================================================
//
//  Exposed functions:
//
//  input_waspressed ( gadget_name ) ── edge: pressed this frame
//  input_ispressed  ( gadget_name ) ── held: pressed every frame
//  input_getvalue   ( gadget_name ) ── analog value [0, 1]
//  input_ispresent  ( gadget_name ) ── device is connected
//
//  gadget_name is the engine constant string from "e_Input_Gadget_Defines.hpp"
//
//==============================================================================

#ifndef BIND_INPUT_HPP
#define BIND_INPUT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../ScriptBindings.hpp"

//==============================================================================
//  BIND INPUT
//==============================================================================

class BindInput : public ScriptBindings
{
public:
    const char* GetModuleName ( void ) const override { return "input"; }
    void        Register      ( ScriptBackend* pBackend ) override;
};

//==============================================================================
#endif // BIND_INPUT_HPP
//==============================================================================
