//==============================================================================
//
//  lua_backend.hpp
//
//==============================================================================

#ifndef LUA_BACKEND_HPP
#define LUA_BACKEND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../script_backend.hpp"

struct lua_State;

//==============================================================================
//  LUA_BACKEND
//==============================================================================

class lua_backend : public script_backend
{
public:
                        lua_backend     ( void );
    virtual            ~lua_backend     ( void );

    virtual void        Init            ( void ) override;
    virtual void        Kill            ( void ) override;

    virtual xbool       RunFile         ( const char* pPath ) override;
    virtual xbool       RunString       ( const char* pCode ) override;
    virtual xbool       CallFunction    ( const char* pFuncName ) override;
    virtual void        Update          ( f32 DeltaTime ) override;

    virtual void        RegisterFunction( const char* pName, script_fn Fn ) override;

    virtual const char* GetBackendName  ( void ) const override { return "Lua 5.4"; }

    lua_State*          GetState        ( void ) const { return m_pL; }

private:
    lua_State*          m_pL;
};

//==============================================================================
#endif // LUA_BACKEND_HPP
//==============================================================================