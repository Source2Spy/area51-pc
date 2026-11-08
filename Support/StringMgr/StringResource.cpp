//==============================================================================
//
//  StringResource.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "StringMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"

//==============================================================================
//  RESOURCE LOADER
//==============================================================================

class string_loader : public rsc_loader
{
public:

    string_loader( void ) :
        rsc_loader( "String Table", ".string" )
    {
    }

    //------------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFile )
    {
        (void)pFile;
        return NULL;
    }

    //------------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE*& pFile, const char* pFileName )
    {
        char Drive[X_MAX_DRIVE];
        char Path[X_MAX_PATH];
        char Name[X_MAX_FNAME];
        char Extension[X_MAX_EXT];

        x_splitpath( pFileName, Drive, Path, Name, Extension );

        if( !g_StringTableMgr.LoadTable( Name, pFileName, pFile ) )
        {
            return NULL;
        }

        return new xstring( Name );
    }

    //------------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        return pData;
    }

    //------------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        xstring* pTableName = static_cast<xstring*>( pData );

        if( pTableName )
        {
            g_StringTableMgr.UnloadTable( *pTableName );
            delete pTableName;
        }
    }
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

void string_mgr::RegisterResourceLoader( void )
{
    // Explicit registration keeps this loader linked with the manager.
    static string_loader s_StringLoader;
}
