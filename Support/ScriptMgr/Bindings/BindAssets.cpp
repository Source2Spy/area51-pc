//==============================================================================
//
//  BindAssets.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "BindAssets.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "IOManager/io_filesystem.hpp"
#include "TweakMgr/TweakMgr.hpp"

//==============================================================================
//  BINDING FUNCTIONS
//==============================================================================

static
s32 BindAssetMount( ScriptContext& Context )
{
    const char* pPath = Context.ArgString( 0 );
    xbool       Ok    = g_IOFSMgr.MountFileSystem( pPath, 4 );
    if( !Ok )
        x_DebugMsg( "[assets] mount FAILED: '%s'\n", pPath );
    Context.PushBool( Ok );
    return 1;
}

//==============================================================================

static
s32 BindAssetUnmount( ScriptContext& Context )
{
    Context.PushBool( g_IOFSMgr.UnmountFileSystem( Context.ArgString( 0 ) ) );
    return 1;
}

//==============================================================================

static
s32 BindAssetLoad( ScriptContext& Context )
{
    const char* pName = Context.ArgString( 0 );
    g_RscMgr.Load( pName );
    xbool Loaded = g_RscMgr.IsLoaded( pName );
    if( !Loaded )
        x_DebugMsg( "[assets] load FAILED (not found in mounted filesystems): '%s'\n", pName );
    Context.PushBool( Loaded );
    return 1;
}

//==============================================================================

static
s32 BindAssetIsLoaded( ScriptContext& Context )
{
    Context.PushBool( g_RscMgr.IsLoaded( Context.ArgString( 0 ) ) );
    return 1;
}

//==============================================================================

static
s32 BindTweakSet( ScriptContext& Context )
{
    const char* pName  = Context.ArgString( 0 );
    f32         Value  = Context.ArgFloat( 1 );

    xbool Ok = SetTweakF32( pName, Value );
    if( !Ok )
        x_DebugMsg( "[assets] tweak_set: script tweak table full, dropped '%s'\n", pName );
    Context.PushBool( Ok );
    return 1;
}

//==============================================================================
//  REGISTER
//==============================================================================

void BindAssets::Register( ScriptBackend* pBackend )
{
    pBackend->RegisterFunction( "asset_mount",     BindAssetMount    );
    pBackend->RegisterFunction( "asset_unmount",   BindAssetUnmount  );
    pBackend->RegisterFunction( "asset_load",      BindAssetLoad     );
    pBackend->RegisterFunction( "asset_is_loaded", BindAssetIsLoaded );
    pBackend->RegisterFunction( "tweak_set",       BindTweakSet      );
}
