//=========================================================================
//
//  Texture.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Texture.hpp"
#include "TextureFile.hpp"
#include "e_Engine.hpp"

//=========================================================================
//  TEXTURE STATISTICS
//=========================================================================

static s32 s_nTexture   = 0;
static s32 s_TextureMem = 0;

//=========================================================================

void texture::GetStats( s32* pNumTextureLoaded, s32* pTextureMemorySize )
{
    *pNumTextureLoaded  = s_nTexture;
    *pTextureMemorySize = s_TextureMem;
}

//=========================================================================
//  TEXTURE RESOURCE LOADER
//=========================================================================

static
struct texture_loader : public rsc_loader
{
    texture_loader( void ) : rsc_loader( "TEXTURE", ".xbmp" )
    {
    }

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE*& pFile, const char* pFileName ) override
    {
        MEMORY_OWNER( "TEXTURE DATA" );
        texture* pTexture = new texture;
        ASSERT( pTexture );

        pFile = texture_file::LoadWithFallback( pTexture->m_bitmap, pFileName );
        if( !pFile )
        {
            delete pTexture;
            return NULL;
        }

        return PreloadComplete( pTexture );
    }

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFp ) override
    {
        MEMORY_OWNER( "TEXTURE DATA" );
        texture* pTexture = new texture;
        ASSERT( pTexture );

        if( !texture_file::LoadXBMP( pTexture->m_bitmap, pFp ) )
        {
            delete pTexture;
            return NULL;
        }

        return PreloadComplete( pTexture );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        texture* pTexture = static_cast<texture*>( pData );

        if( !vram_CreateTexture( pTexture->m_texture, pTexture->m_bitmap, TRUE, "texture" ) )
        {
            Unload( pTexture );
            x_throw( "Failed to create GPU texture." );
            return NULL;
        }

        return ( pTexture );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        texture* pTexture = static_cast<texture*>( pData );

        s_TextureMem -= pTexture->m_bitmap.GetDataSize();
        s_nTexture--;

        vram_DestroyTexture( pTexture->m_texture );

        delete pTexture;
    }

private:

    static texture* PreloadComplete( texture* pTexture )
    {
        s_TextureMem += pTexture->m_bitmap.GetDataSize();
        s_nTexture++;
        return pTexture;
    }

} s_Texture_Loader;

//=========================================================================
//  CUBEMAP RESOURCE LOADER
//=========================================================================

static
struct cubemap_loader : public rsc_loader
{
    cubemap_loader( void ) : rsc_loader( "CUBEMAP", ".envmap" )
    {
    }

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFp )
    {
        cubemap* pCubemap = new cubemap;
        ASSERT( pCubemap );

        for( s32 i = 0; i < 6; i++ )
        {
            pCubemap->m_bitmap[i].Load( pFp );
            s_TextureMem += pCubemap->m_bitmap[i].GetDataSize();
            s_nTexture++;
        }

        return ( pCubemap );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        cubemap* pCubemap = static_cast<cubemap*>( pData );

        if( !vram_CreateTextureCube( pCubemap->m_texture, pCubemap->m_bitmap, 6, TRUE, "cubemap" ) )
        {
            x_throw( "Failed to create GPU cubemap." );
            return NULL;
        }

        return ( pCubemap );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        cubemap* pCubemap = static_cast<cubemap*>( pData );

        for( s32 i = 0; i < 6; i++ )
        {
            s_TextureMem -= pCubemap->m_bitmap[i].GetDataSize();
            s_nTexture--;
        }

        vram_DestroyTexture( pCubemap->m_texture );

        delete pCubemap;
    }

} s_Cubemap_Loader;

//=========================================================================
//  TEXTURE
//=========================================================================

texture::texture( void )
{
    m_texture                 = vram_texture();
    m_pShaderResourceOverride = NULL;
}

//=========================================================================

vram_texture const& texture::GetTexture( void ) const
{
    return m_texture;
}

//=========================================================================

shader_resource const* texture::GetShaderResource( void ) const
{
    if( m_pShaderResourceOverride )
    {
        return m_pShaderResourceOverride;
    }

    return vram_GetShaderResource( m_texture );
}

//=========================================================================

shader_resource const* texture::GetShaderResourceOverride( void ) const
{
    return m_pShaderResourceOverride;
}

//=========================================================================

void texture::SetShaderResourceOverride( shader_resource const* pResource )
{
    m_pShaderResourceOverride = pResource;
}

//=========================================================================
//  CUBEMAP
//=========================================================================

cubemap::cubemap( void )
{
    m_texture = vram_texture();
}

//=========================================================================

vram_texture const& cubemap::GetTexture( void ) const
{
    return m_texture;
}

//=========================================================================

shader_resource const* cubemap::GetShaderResource( void ) const
{
    return vram_GetShaderResource( m_texture );
}
