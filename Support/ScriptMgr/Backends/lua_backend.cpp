//==============================================================================
//
//  lua_backend.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "lua_backend.hpp"

extern "C"
{
#include "3rdParty\Lua\5.4.8\lua.h"
#include "3rdParty\Lua\5.4.8\lualib.h"
#include "3rdParty\Lua\5.4.8\lauxlib.h"
}

#include "x_files.hpp"

//==============================================================================
//  LUA_CONTEXT
//==============================================================================

class lua_context : public script_context
{
public:
    explicit lua_context( lua_State* L ) : m_pL( L ) {}

    //--------------------------------------------------------------------------
    //  Arguments, 0-indexed externally, Lua is 1-indexed internally.
    //--------------------------------------------------------------------------

    s32             ArgCount    ( void )        const override { return lua_gettop( m_pL ); }

    xbool           ArgBool     ( s32 i )       const override { luaL_checktype( m_pL, i + 1, LUA_TBOOLEAN ); return lua_toboolean( m_pL, i + 1 ); }
    s32             ArgInt      ( s32 i )       const override { return (s32)luaL_checkinteger( m_pL, i + 1 ); }
    u64             ArgU64      ( s32 i )       const override { return (u64)luaL_checkinteger( m_pL, i + 1 ); }  // For guids
    f32             ArgFloat    ( s32 i )       const override { return (f32)luaL_checknumber(  m_pL, i + 1 ); }
    const char*     ArgString   ( s32 i )       const override { return luaL_checkstring( m_pL, i + 1 ); }

    //--------------------------------------------------------------------------
    //  Return values, just push onto the Lua stack.
    //--------------------------------------------------------------------------

    void            PushBool    ( xbool         v ) override { lua_pushboolean( m_pL, v ); }
    void            PushInt     ( s32           v ) override { lua_pushinteger( m_pL, v ); }
    void            PushU64     ( u64           v ) override { lua_pushinteger( m_pL, (lua_Integer)v ); }  // For guids
    void            PushFloat   ( f32           v ) override { lua_pushnumber(  m_pL, v ); }
    void            PushString  ( const char*   v ) override { lua_pushstring(  m_pL, v ); }
    void            PushNil     ( void )            override { lua_pushnil( m_pL ); }

private:
    lua_State* m_pL;
};

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
void ScriptPrintBlock( const char* pTag, const char* pContext, const char* pMsg )
{
    x_DebugMsg( "\n" );
    x_DebugMsg( "***\n" );
    x_DebugMsg( "*** %s [%s]\n", pTag, pContext );

    const char* pLine = pMsg;
    while( *pLine )
    {
        const char* pEnd = pLine;
        while( *pEnd && *pEnd != '\n' )
            pEnd++;

        char Buf[ 4096 ];
        s32  Len = (s32)( pEnd - pLine );
        Len = x_min( Len, (s32)sizeof(Buf) - 1 );
        x_memcpy( Buf, pLine, Len );
        Buf[ Len ] = '\0';

        x_DebugMsg( "*** %s\n", Buf );

        pLine = *pEnd ? pEnd + 1 : pEnd;
    }

    x_DebugMsg( "***\n" );
    x_DebugMsg( "\n" );
}

//==============================================================================

static 
void ReportError( lua_State* L, const char* pContext )
{
    const char* pMsg = lua_tostring( L, -1 );
    ScriptPrintBlock( "SCRIPT ERROR", pContext, pMsg ? pMsg : "(no message)" );
    lua_pop( L, 1 );
    ASSERT( FALSE );
}

//==============================================================================

static 
void ReportWarning( const char* pContext, const char* pMsg )
{
    ScriptPrintBlock( "SCRIPT WARNING", pContext, pMsg ? pMsg : "(no message)" );
}

//==============================================================================

static 
int lua_dispatch( lua_State* L )
{
    script_fn Fn = (script_fn)lua_touserdata( L, lua_upvalueindex(1) );
    lua_context ctx( L );
    return Fn( ctx );
}

//==============================================================================

static 
int lua_traceback_handler( lua_State* L )
{
    const char* pMsg = lua_tostring( L, 1 );
    luaL_traceback( L, L, pMsg, 1 );
    return 1;
}

//==============================================================================

static
xbool lua_exec_loaded( lua_State* L, int MsghIdx, int nArgs, const char* pContext )
{
    if( lua_pcall( L, nArgs, 0, MsghIdx ) != LUA_OK )
    {
        lua_remove( L, MsghIdx );
        ReportError( L, pContext );
        return FALSE;
    }

    lua_remove( L, MsghIdx );
    return TRUE;
}

//==============================================================================

static
xbool lua_push_global_function( lua_State* L, int MsghIdx, const char* pName )
{
    lua_getglobal( L, pName );
    if( !lua_isfunction( L, -1 ) )
    {
        lua_settop( L, MsghIdx - 1 ); // restore stack to before handler push
        return FALSE;
    }
    return TRUE;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

lua_backend::lua_backend( void )
    : m_pL( NULL )
{
}

//==============================================================================

lua_backend::~lua_backend( void )
{
    Kill();
}

//==============================================================================

void lua_backend::Init( void )
{
    ASSERT( !m_pL );

    m_pL = luaL_newstate();
    ASSERT( m_pL );

    luaL_openlibs( m_pL );
}

//==============================================================================

void lua_backend::Kill( void )
{
    if( m_pL )
    {
        lua_close( m_pL );
        m_pL = NULL;
    }
}

//==============================================================================

void lua_backend::RegisterFunction( const char* pName, script_fn Fn )
{
    ASSERT( m_pL );
    ASSERT( pName );
    ASSERT( Fn );

    // Store Fn as an upvalue so lua_dispatch can retrieve it.
    lua_pushlightuserdata( m_pL, (void*)Fn );
    lua_pushcclosure( m_pL, lua_dispatch, 1 );
    lua_setglobal( m_pL, pName );
}

//==============================================================================

xbool lua_backend::RunFile( const char* pPath )
{
    ASSERT( m_pL );

    lua_pushcfunction( m_pL, lua_traceback_handler );
    int MsghIdx = lua_gettop( m_pL );

    if( luaL_loadfile( m_pL, pPath ) != LUA_OK )
    {
        lua_remove( m_pL, MsghIdx );
        ReportError( m_pL, pPath );
        return FALSE;
    }

    return lua_exec_loaded( m_pL, MsghIdx, 0, pPath );
}

//==============================================================================

xbool lua_backend::RunString( const char* pCode )
{
    ASSERT( m_pL );

    lua_pushcfunction( m_pL, lua_traceback_handler );
    int MsghIdx = lua_gettop( m_pL );

    if( luaL_loadstring( m_pL, pCode ) != LUA_OK )
    {
        lua_remove( m_pL, MsghIdx );
        ReportError( m_pL, "RunString" );
        return FALSE;
    }

    return lua_exec_loaded( m_pL, MsghIdx, 0, "RunString" );
}

//==============================================================================

xbool lua_backend::CallFunction( const char* pFuncName )
{
    ASSERT( m_pL );

    lua_pushcfunction( m_pL, lua_traceback_handler );
    int MsghIdx = lua_gettop( m_pL );

    if( !lua_push_global_function( m_pL, MsghIdx, pFuncName ) )
        return FALSE;

    return lua_exec_loaded( m_pL, MsghIdx, 0, pFuncName );
}

//==============================================================================

void lua_backend::Update( f32 DeltaTime )
{
    ASSERT( m_pL );

    lua_pushcfunction( m_pL, lua_traceback_handler );
    int MsghIdx = lua_gettop( m_pL );

    if( !lua_push_global_function( m_pL, MsghIdx, "OnUpdate" ) )
        return;

    lua_pushnumber( m_pL, DeltaTime );
    lua_exec_loaded( m_pL, MsghIdx, 1, "OnUpdate" );
}