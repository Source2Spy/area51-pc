//=========================================================================
//
//  FontResource.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontResource.hpp"
#include "FontMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "x_locale.hpp"

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static const char* GetLocalizedFontName( const char* pName )
{
    if( x_GetLocale() == XL_LANG_JAPANESE )
    {
        if( !x_strcmp( pName, "small" ) )
        {
            return "small-ml";
        }

        if( !x_strcmp( pName, "large" ) )
        {
            return "large-ml";
        }
    }

    return pName;
}

//=========================================================================
//  RESOURCE LOADER
//=========================================================================

static struct font_collection_loader : public rsc_loader
{
    font_collection_loader( void ) :
        rsc_loader( "FONT", ".font" )
    {
    }

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFile )
    {
        FontCollection* pCollection = new FontCollection;

        if( !pCollection->Load( pFile ) )
        {
            delete pCollection;
            x_throw( "Invalid font collection" );
            return NULL;
        }

        return pCollection;
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        return pData;
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        delete static_cast<FontCollection*>( pData );
    }
} s_FontCollectionLoader;

//=========================================================================
//  FUNCTIONS
//=========================================================================

FontHandle LoadFontResource( const char* pPath, const char* pName )
{
    rhandle<FontCollection> Resource;
    Resource.SetName( pPath );

    FontCollection* pCollection = Resource.GetPointer();

    if( !pCollection )
    {
        return FontHandle();
    }

    return g_FontMgr.FindFont( *pCollection, GetLocalizedFontName( pName ) );
}
