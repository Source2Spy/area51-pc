//==============================================================================
//
//  ScriptMgr.hpp
//
//==============================================================================

#ifndef SCRIPT_MGR_HPP
#define SCRIPT_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "x_array.hpp"
#include "ScriptBackend.hpp"
#include "ScriptBindings.hpp"

//==============================================================================
//  SCRIPT MGR
//==============================================================================

class ScriptMgr
{
public:
                        ScriptMgr       ( void );
                       ~ScriptMgr       ( void );

    //--------------------------------------------------------------------------
    //  Lifecycle
    //--------------------------------------------------------------------------

    // Creates the backend, registers all binding modules, and initializes.
    void                Init            ( void );
    void                Kill            ( void );
    void                SetEnabled      ( xbool Enabled );

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
    //  ScriptMgr takes ownership of the pointer.
    //--------------------------------------------------------------------------

    void                AddBindings     ( ScriptBindings* pModule );

    //--------------------------------------------------------------------------
    //  Access
    //--------------------------------------------------------------------------

    ScriptBackend*      GetBackend      ( void ) const { return m_Enabled ? m_pBackend : NULL; }
    xbool               IsEnabled       ( void ) const { return m_Enabled; }
    xbool               IsInitialized   ( void ) const { return m_pBackend != NULL; }

private:
    ScriptBackend*              m_pBackend;
    xbool                       m_Enabled;
    xarray<ScriptBindings*>     m_Modules;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern ScriptMgr g_ScriptMgr;

//==============================================================================
#endif // SCRIPT_MGR_HPP
//==============================================================================
