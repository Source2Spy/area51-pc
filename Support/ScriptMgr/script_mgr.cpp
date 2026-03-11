//==============================================================================
//
//  script_mgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "script_mgr.hpp"
#include "Backends/lua_backend.hpp"

#include "Bindings/bind_system.hpp"
#include "Bindings/bind_objects.hpp"
#include "Bindings/bind_input.hpp"
#include "Bindings/bind_raycast.hpp"

#include "x_files.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

script_mgr g_ScriptMgr;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

script_mgr::script_mgr( void )
    : m_pBackend( NULL )
{
}

//==============================================================================

script_mgr::~script_mgr( void )
{
    Kill();
}

//==============================================================================

void script_mgr::Init( void )
{
    ASSERT( !m_pBackend );

    //--------------------------------------------------------------------------
    //  Create backend
    //  Swap lua_backend for any other script_backend implementation here.
    //--------------------------------------------------------------------------

    m_pBackend = new lua_backend();
    m_pBackend->Init();

    //--------------------------------------------------------------------------
    //  Register built-in binding modules
    //  To add a module: include its header, AddBindings() it here.
    //--------------------------------------------------------------------------

    AddBindings( new bind_system()   );
    AddBindings( new bind_objects()  );
    AddBindings( new bind_input()    );
    AddBindings( new bind_raycast()  );

    //--------------------------------------------------------------------------
    //  Call Register() on all modules, including any added before Init()
    //--------------------------------------------------------------------------

    for( s32 i = 0; i < m_Modules.GetCount(); i++ )
        m_Modules[i]->Register( m_pBackend );

    x_DebugMsg( "ScriptMgr: initialized with backend '%s', %d module(s).\n",
                 m_pBackend->GetBackendName(), m_Modules.GetCount() );
}

//==============================================================================

void script_mgr::Kill( void )
{
    if( m_pBackend )
    {
        m_pBackend->Kill();
        delete m_pBackend;
        m_pBackend = NULL;
    }

    for( s32 i = 0; i < m_Modules.GetCount(); i++ )
        delete m_Modules[i];

    m_Modules.Clear();
}

//==============================================================================

void script_mgr::AddBindings( script_bindings* pModule )
{
    ASSERT( pModule );
    m_Modules.Append() = pModule;
}

//==============================================================================

xbool script_mgr::RunFile( const char* pPath )
{
    ASSERT( m_pBackend );
    return m_pBackend->RunFile( pPath );
}

//==============================================================================

xbool script_mgr::RunString( const char* pCode )
{
    ASSERT( m_pBackend );
    return m_pBackend->RunString( pCode );
}

//==============================================================================

xbool script_mgr::CallFunction( const char* pFuncName )
{
    ASSERT( m_pBackend );
    return m_pBackend->CallFunction( pFuncName );
}

//==============================================================================

void script_mgr::Update( f32 DeltaTime )
{
    ASSERT( m_pBackend );
    m_pBackend->Update( DeltaTime );
}

//==============================================================================

void script_mgr::NotifyLevelStart( void )
{
    ASSERT( m_pBackend );
    m_pBackend->CallFunction( "OnLevelLoad" );
}

//==============================================================================

void script_mgr::NotifyLevelEnd( void )
{
    ASSERT( m_pBackend );
    m_pBackend->CallFunction( "OnLevelUnload" );
}

//==============================================================================
