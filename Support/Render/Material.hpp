//=========================================================================
//
//  Material.hpp
//
//=========================================================================

#ifndef MATERIAL_HPP
#define MATERIAL_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Texture.hpp"
#include "Material_Prefs.hpp"
#include "ShaderContract.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum material_render_order
{
    MATERIAL_RENDER_ORDER_OPAQUE         = 0,
    MATERIAL_RENDER_ORDER_GLOWING        = 1,
    MATERIAL_RENDER_ORDER_ALPHA_GLOWING  = 2,
    MATERIAL_RENDER_ORDER_ALPHA          = 3,
    MATERIAL_RENDER_ORDER_DISTORTION     = 7,
};

//-------------------------------------------------------------------------

enum material_blend_mode
{
    MATERIAL_BLEND_OPAQUE = 0,
    MATERIAL_BLEND_ALPHA,
    MATERIAL_BLEND_ADDITIVE,
    MATERIAL_BLEND_SUBTRACTIVE,
    MATERIAL_BLEND_DISTORTION,
};

//=========================================================================

class material
{
public:

    struct uvanim
    {
        f32 CurrentFrame;
        s16 iKey;
        s8  iFrame;
        s8  Dir;
        s8  Type;
        s8  nFrames;
        s8  FPS;
        s8  StartFrame;

        uvanim( void );
    };

    material( void );
    ~material( void );

    void  Finalize   ( void );
    render::MaterialDesc const& GetDesc( void ) const { return m_Desc; }
    xbool operator== ( material const& Rhs ) const;

    xbool               HasUVAnimation         ( void ) const;
    material_blend_mode GetBlendMode           ( void ) const;
    u8                  GetRenderOrder         ( void ) const;
    u8                  GetSortPriority        ( void ) const;
    f32                 GetAlphaRef            ( void ) const;
    f32                 GetDetailScale         ( void ) const;
    f32                 GetFixedAlpha          ( void ) const;
    f32                 GetCubeMapIntensity    ( void ) const;
    xbool               IsAlpha                ( void ) const;
    xbool               IsDistortion           ( void ) const;
    xbool               IsDistortionPerPolyEnv ( void ) const;
    xbool               ForcesZFill            ( void ) const;
    xbool               IsPunchThrough         ( void ) const;
    xbool               UsesCubeMap            ( void ) const;
    xbool               RequiresForwardPass    ( void ) const;
    xbool               RequiresDepthSort      ( void ) const;
    xbool               IsPostEffectBlend      ( void ) const;
    xbool               ReceivesProjection     ( void ) const;
    xbool               ReceivesShadow         ( void ) const;
    xbool               UsesShadowAlpha        ( void ) const;
    xbool               WritesDepth            ( void ) const;
    xbool               CullsBackFaces         ( void ) const;
    xbool               ClampsSampler          ( void ) const;
    s32                 AddRef                 ( void );
    s32                 Release                ( void );
    s32                 GetRefCount            ( void ) const;

public:

    s8  m_Type;
    f32 m_DetailScale;
    f32 m_FixedAlpha;
    u16 m_Flags;

    texture::handle m_DiffuseMap;
    texture::handle m_EnvironmentMap;
    texture::handle m_DetailMap;
    uvanim          m_UVAnim;
    s32             m_RefCount;

private:

    struct render_data
    {
        u32                 Flags;
        material_blend_mode BlendMode;
        u8                  RenderOrder;
        u8                  SortPriority;
        f32                 AlphaRef;
        f32                 DetailScale;
        f32                 FixedAlpha;
        f32                 CubeMapIntensity;
    };

    render_data m_RenderData;
    render::MaterialDesc m_Desc;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline xbool material::HasUVAnimation( void ) const
{
    return ( m_UVAnim.nFrames > 0 );
}

//=========================================================================

inline material_blend_mode material::GetBlendMode( void ) const
{
    return m_RenderData.BlendMode;
}

//=========================================================================

inline u8 material::GetRenderOrder( void ) const
{
    return m_RenderData.RenderOrder;
}

//=========================================================================

inline u8 material::GetSortPriority( void ) const
{
    return m_RenderData.SortPriority;
}

//=========================================================================

inline f32 material::GetAlphaRef( void ) const
{
    return m_RenderData.AlphaRef;
}

//=========================================================================

inline f32 material::GetDetailScale( void ) const
{
    return m_RenderData.DetailScale;
}

//=========================================================================

inline f32 material::GetFixedAlpha( void ) const
{
    return m_RenderData.FixedAlpha;
}

//=========================================================================

inline f32 material::GetCubeMapIntensity( void ) const
{
    return m_RenderData.CubeMapIntensity;
}

//=========================================================================

inline s32 material::AddRef( void )
{
    return ++m_RefCount;
}

//=========================================================================

inline s32 material::Release( void )
{
    return --m_RefCount;
}

//=========================================================================

inline s32 material::GetRefCount( void ) const
{
    return m_RefCount;
}

//=========================================================================
#endif // MATERIAL_HPP
//=========================================================================
