//=========================================================================
//
//  Texture.hpp
//
//=========================================================================

#ifndef TEXTURE_HPP
#define TEXTURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../ResourceMgr/ResourceMgr.hpp"
#include "x_files.hpp"
#include "x_bitmap.hpp"
#include "e_VRAM.hpp"

//=========================================================================
//  CLASS
//=========================================================================

class texture
{
public:

    texture( void );

    static void GetStats( s32* pNumTexturesLoaded, s32* pTextureMemorySize );

    using handle = rhandle<texture>;

    vram_texture const&    GetTexture                ( void ) const;
    shader_resource const* GetShaderResource         ( void ) const;
    shader_resource const* GetShaderResourceOverride ( void ) const;
    void                   SetShaderResourceOverride ( shader_resource const* pResource );

    xbitmap                m_bitmap;
    vram_texture           m_texture;
    shader_resource const* m_pShaderResourceOverride;

    friend struct texture_loader;
};

//=========================================================================

class cubemap
{
public:

    cubemap( void );

    enum sides
    {
        TOP = 0,
        BOTTOM,
        FRONT,
        BACK,
        LEFT,
        RIGHT
    };

    using handle = rhandle<cubemap>;

    vram_texture const&    GetTexture        ( void ) const;
    shader_resource const* GetShaderResource ( void ) const;

    vram_texture m_texture;
    xbitmap      m_bitmap[6];

    friend struct cubemap_loader;
};

//=========================================================================
#endif // TEXTURE_HPP
//=========================================================================
