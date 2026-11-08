//==============================================================================
//
//  Config.cpp
//
//==============================================================================

  
//==============================================================================
//  INCLUDES
//==============================================================================

#include "Config.hpp"
#include "IniFile.hpp"

#ifndef CONFIG_RETAIL

//==============================================================================
//  STORAGE
//==============================================================================

config_options  g_Config;

//==============================================================================
//  FUNCTIONS
//==============================================================================

config_options::config_options( void )
{
    // Self lobotomy.
    x_memset( this, 0, sizeof(config_options) );
    x_strcpy( AutoClientAddress, "127.0.0.1:3658" );
}

//==============================================================================

config_options::~config_options( void )
{
}

//==============================================================================

void config_options::Load( const char* pFileName )
{
    ASSERT( pFileName );

    ini_file IniFile;

    if( !IniFile.Load( pFileName ) )
        return;

    //
    // Read settings from the ini file.
    //

    IniFile.GetBool     ( "DemoBuild",          DemoBuild       );
    IniFile.GetBool     ( "AutoCampaign",       AutoCampaign    );
    IniFile.GetBool     ( "AutoSplitScreen",    AutoSplitScreen );
    IniFile.GetBool     ( "AutoServer",         AutoServer      );
    IniFile.GetBool     ( "AutoClient",         AutoClient      );
    IniFile.GetS32      ( "AutoLevel",          AutoLevel       );
    IniFile.GetBool     ( "AutoServer",         AutoServer      );
    IniFile.GetS32      ( "AutoServerType",     AutoServerType  );
    IniFile.GetS32      ( "AutoMutateMode",     AutoMutateMode  );
    IniFile.GetString   ( "AutoServerName",     AutoServerName  );
}

//==============================================================================

#endif // CONFIG_RETAIL
