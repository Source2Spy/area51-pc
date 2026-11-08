//==============================================================================
//
//  ScriptMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ScriptMgr.hpp"
#include "Backends/LuaBackend.hpp"

#include "Bindings/BindSystem.hpp"
#include "Bindings/BindObjects.hpp"
#include "Bindings/BindInput.hpp"
#include "Bindings/BindRaycast.hpp"
#include "Bindings/BindAssets.hpp"

#include "x_files.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

ScriptMgr g_ScriptMgr;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ScriptMgr::ScriptMgr( void )
    : m_pBackend( NULL )
    , m_Enabled( TRUE )
{
}

//==============================================================================

ScriptMgr::~ScriptMgr( void )
{
    Kill();
}

//==============================================================================

void ScriptMgr::Init( void )
{
    ASSERT( !m_pBackend );

    //--------------------------------------------------------------------------
    //  Create backend
    //  Swap LuaBackend for any other ScriptBackend implementation here.
    //--------------------------------------------------------------------------

    m_pBackend = new LuaBackend();
    m_pBackend->Init();

    //--------------------------------------------------------------------------
    //  Register built-in binding modules
    //  To add a module: include its header, AddBindings() it here.
    //--------------------------------------------------------------------------

    AddBindings( new BindSystem()   );
    AddBindings( new BindObjects()  );
    AddBindings( new BindInput()    );
    AddBindings( new BindRaycast()  );
    AddBindings( new BindAssets()   );

    //--------------------------------------------------------------------------
    //  Call Register() on all modules, including any added before Init()
    //--------------------------------------------------------------------------

    for( s32 i = 0; i < m_Modules.GetCount(); i++ )
        m_Modules[i]->Register( m_pBackend );

    x_DebugMsg( "ScriptMgr: initialized with backend '%s', %d module(s).\n",
                 m_pBackend->GetBackendName(), m_Modules.GetCount() );
}

//==============================================================================

void ScriptMgr::Kill( void )
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

void ScriptMgr::SetEnabled( xbool Enabled )
{
    m_Enabled = Enabled;
}

//==============================================================================

void ScriptMgr::AddBindings( ScriptBindings* pModule )
{
    ASSERT( pModule );
    m_Modules.Append() = pModule;
}

//==============================================================================

xbool ScriptMgr::RunFile( const char* pPath )
{
    if( !m_Enabled )
        return FALSE;

    ASSERT( m_pBackend );
    return m_pBackend->RunFile( pPath );
}

//==============================================================================

xbool ScriptMgr::RunString( const char* pCode )
{
    if( !m_Enabled )
        return FALSE;

    ASSERT( m_pBackend );
    return m_pBackend->RunString( pCode );
}

//==============================================================================

xbool ScriptMgr::CallFunction( const char* pFuncName )
{
    if( !m_Enabled )
        return FALSE;

    ASSERT( m_pBackend );
    return m_pBackend->CallFunction( pFuncName );
}

//==============================================================================

void ScriptMgr::Update( f32 DeltaTime )
{
    if( !m_Enabled )
        return;

    ASSERT( m_pBackend );
    m_pBackend->Update( DeltaTime );
}

//==============================================================================

void ScriptMgr::NotifyLevelStart( void )
{
    if( !m_Enabled )
        return;

    ASSERT( m_pBackend );
    m_pBackend->CallFunction( "OnLevelLoad" );
}

//==============================================================================

void ScriptMgr::NotifyLevelEnd( void )
{
    if( !m_Enabled )
        return;

    ASSERT( m_pBackend );
    m_pBackend->CallFunction( "OnLevelUnload" );
}

//==============================================================================
