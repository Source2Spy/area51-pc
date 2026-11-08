//==============================================================================
//
//  LuaBackend.hpp
//
//==============================================================================

#ifndef LUA_BACKEND_HPP
#define LUA_BACKEND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../ScriptBackend.hpp"

//==============================================================================
//  LUA BACKEND
//==============================================================================

struct lua_State;

class LuaBackend : public ScriptBackend
{
public:
                        LuaBackend      ( void );
    virtual            ~LuaBackend      ( void );

    virtual void        Init            ( void ) override;
    virtual void        Kill            ( void ) override;

    virtual xbool       RunFile         ( const char* pPath ) override;
    virtual xbool       RunString       ( const char* pCode ) override;
    virtual xbool       CallFunction    ( const char* pFuncName ) override;
    virtual void        Update          ( f32 DeltaTime ) override;

    virtual void        RegisterFunction( const char* pName, ScriptFn Fn ) override;

    virtual const char* GetBackendName  ( void ) const override { return "Lua 5.5"; }

    //lua_State*          GetState        ( void ) const { return m_pL; }

private:
    lua_State*          m_pL;
};

//==============================================================================
#endif // LUA_BACKEND_HPP
//==============================================================================
