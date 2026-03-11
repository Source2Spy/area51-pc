//==============================================================================
//
//  script_mgr.hpp
//
//==============================================================================

#ifndef SCRIPT_MGR_HPP
#define SCRIPT_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "x_array.hpp"
#include "script_backend.hpp"
#include "script_bindings.hpp"

//==============================================================================
//  script_mgr
//==============================================================================

class script_mgr
{
public:
                        script_mgr      ( void );
                       ~script_mgr      ( void );

    //--------------------------------------------------------------------------
    //  Lifecycle
    //--------------------------------------------------------------------------

    // Creates the backend, registers all binding modules, and initializes.
    void                Init            ( void );
    void                Kill            ( void );

    //--------------------------------------------------------------------------
    //  Execution (forwarded to backend)
    //--------------------------------------------------------------------------

    xbool               RunFile         ( const char* pPath );
    xbool               RunString       ( const char* pCode );
    xbool               CallFunction    ( const char* pFuncName );
    void                Update          ( f32 DeltaTime );

    // Called by the engine at level start/end.
    // Invokes OnLevelLoad() / OnLevelUnload() in Lua if defined.
    void                NotifyLevelStart( void );
    void                NotifyLevelEnd  ( void );

    //--------------------------------------------------------------------------
    //  Extensibility
    //
    //  Call AddBindings() before Init() to inject custom modules.
    //  script_mgr takes ownership of the pointer.
    //--------------------------------------------------------------------------

    void                AddBindings     ( script_bindings* pModule );

    //--------------------------------------------------------------------------
    //  Access
    //--------------------------------------------------------------------------

    script_backend*     GetBackend      ( void ) const { return m_pBackend; }
    xbool               IsInitialized   ( void ) const { return m_pBackend != NULL; }

private:
    script_backend*             m_pBackend;
    xarray<script_bindings*>    m_Modules;
};

//==============================================================================

extern script_mgr g_ScriptMgr;

//==============================================================================
#endif // SCRIPT_MGR_HPP
//==============================================================================
