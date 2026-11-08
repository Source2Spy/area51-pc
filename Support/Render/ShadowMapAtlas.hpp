//=========================================================================
//
//  ShadowMapAtlas.hpp
//
//=========================================================================

#ifndef SHADOW_MAP_ATLAS_HPP
#define SHADOW_MAP_ATLAS_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "geom.hpp"
#include "Material.hpp"
#include "ShadowTypes.hpp"

class object;

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

enum
{
    POINT_SHADOW_FACE_COUNT      = 6,
    MAX_SHADOW_LIGHTS            = 8,
    MAX_SHADOW_SOURCES           = 64,
    MAX_SHADOW_ATLAS_SIZE        = 8192,
    MAX_SHADOW_MOMENT_ATLAS_SIZE = 2048,
    SHADOW_EVSM_BLUR_RADIUS      = 4,
};

//=========================================================================

class ShadowMapAtlas
{
public:

    enum SourceType
    {
        SHADOW_SOURCE_POINT_FACE = 0,
        SHADOW_SOURCE_SPOT,
    };

    struct ShadowSource
    {
        s32     Type;
        s32     DynamicLightIndex;
        s32     PointLightIndex;
        s32     FaceIndex;
        s32     RequestedResolution;
        s32     ShadowPriority;
        f32     ShadowScore;
        matrix4 WorldToClip;
        matrix4 WorldToAtlas;
        vector4 LightPosRadius;
        vector4 FaceLightDirFalloff;
        vector4 FaceLightData;
        f32     LightFalloff;
        f32     NearZ;
        f32     ReceiveNearZ;
        f32     FarZ;
        f32     ProjectionScale;
        bbox    WorldBBox;
        s32     AtlasX;
        s32     AtlasY;
        s32     AtlasWidth;
        s32     AtlasHeight;
        s32     AtlasTileX;
        s32     AtlasTileY;
        s32     AtlasTileWidth;
        s32     AtlasTileHeight;
    };

private:

    struct ScratchData;

    ShadowMapAtlas( ShadowMapAtlas const& );
    ShadowMapAtlas& operator=( ShadowMapAtlas const& );

    ScratchData& GetScratch               ( void );
    void         ComputePerspectiveSource ( ShadowSource& Destination, s32 SourceType,
                                            matrix4 const& LocalToWorld, radian FieldOfView,
                                            f32 LightRadius, f32 LightFalloff,
                                            s32 ShadowMapResolution ) const;
    void         UpdateAtlasLayout        ( void );

    //-------------------------------------------------------------------------

public:

    ShadowMapAtlas( void );
    ~ShadowMapAtlas( void );

    void  ClearSources        ( void );
    void  FinalizeSources     ( void );
    void  SetEnabled          ( xbool Enabled );
    void  SetShadowMode       ( ShadowMode Mode );
    void  SetShadowFilterType ( ShadowFilterType Type );
    xbool AddPointSource      ( matrix4 const& LocalToWorld, radian FieldOfView,
                                f32 LightRadius, f32 LightFalloff, s32 ShadowMapResolution,
                                s32 ShadowPriority, f32 ShadowScore, s32 DynamicLightIndex );
    xbool AddSpotSource       ( matrix4 const& LocalToWorld, radian FieldOfView,
                                f32 LightRadius, f32 LightFalloff, s32 ShadowMapResolution,
                                s32 ShadowPriority, f32 ShadowScore, s32 DynamicLightIndex );

    ShadowSource const& GetSource           ( s32 SourceIndex ) const;
    s32                 GetSourceCount      ( void ) const;
    s32                 GetAtlasSize        ( void ) const;
    s32                 GetAtlasSourceCount ( void ) const;
    ShadowMode          GetShadowMode       ( void ) const;
    ShadowFilterType    GetShadowFilterType ( void ) const;
    xbool               HasActiveSources    ( void ) const;

    void CreateShadowMap ( void );

    //-------------------------------------------------------------------------

private:

    ScratchData*     m_pScratch;
    s32              m_SourceCount;
    s32              m_PointFaceCount;
    s32              m_PointLightCount;
    s32              m_AtlasSourceCount;
    s32              m_AtlasSize;
    s32              m_AtlasSizeFloor;
    xbool            m_AtlasLayoutDirty;
    xbool            m_Enabled;
    ShadowMode       m_ShadowMode;
    ShadowFilterType m_ShadowFilterType;
    ShadowSource     m_Sources[MAX_SHADOW_SOURCES];
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline ShadowMapAtlas::ShadowSource const& ShadowMapAtlas::GetSource( s32 SourceIndex ) const
{
    ASSERT( ( SourceIndex >= 0 ) && ( SourceIndex < m_SourceCount ) );
    ASSERT( !m_AtlasLayoutDirty );

    return m_Sources[SourceIndex];
}

//=========================================================================

inline s32 ShadowMapAtlas::GetSourceCount( void ) const
{
    return m_SourceCount;
}

//=========================================================================

inline s32 ShadowMapAtlas::GetAtlasSize( void ) const
{
    return ( m_AtlasSize > 0 ) ? m_AtlasSize : MAX_SHADOW_ATLAS_SIZE;
}

//=========================================================================

inline s32 ShadowMapAtlas::GetAtlasSourceCount( void ) const
{
    return m_AtlasSourceCount;
}

//=========================================================================

inline ShadowFilterType ShadowMapAtlas::GetShadowFilterType( void ) const
{
    return m_ShadowFilterType;
}

//========================================================================= 

inline ShadowMode ShadowMapAtlas::GetShadowMode( void ) const
{
    return m_ShadowMode;
}

//=========================================================================

inline xbool ShadowMapAtlas::HasActiveSources( void ) const
{
    return m_Enabled && ( m_SourceCount > 0 );
}

//=========================================================================
} // namespace render

#endif // SHADOW_MAP_ATLAS_HPP
//=========================================================================
