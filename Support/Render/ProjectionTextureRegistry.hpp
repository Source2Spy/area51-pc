//=========================================================================
//
//  ProjectionTextureRegistry.hpp
//
//=========================================================================

#ifndef PROJECTION_TEXTURE_REGISTRY_HPP
#define PROJECTION_TEXTURE_REGISTRY_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "e_View.hpp"
#include "Texture.hpp"
#include "Render.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class ProjectionTextureRegistry
{
public:

    static constexpr s32 MaxLightProjectionCount  = 4;
    static constexpr s32 MaxShadowProjectionCount = 8;

protected:

    struct Projection
    {
        view            m_View;
        matrix4         m_Matrix;
        texture::handle m_Texture;
    };

    static void  ClearProjections            ( Projection* pProjections, s32& ProjectionCount );
    static void  AddProjection               ( Projection* pProjections, s32& ProjectionCount,
                                               s32 MaxProjectionCount, matrix4 const& LocalToWorld,
                                               radian FieldOfView, f32 Length, texture::handle Texture );
    static s32   CollectProjections          ( Projection const* pProjections, s32 ProjectionCount,
                                               s32* pCollectedIndices, s32& CollectedCount,
                                               s32& CurrentCollectedIndex, matrix4 const& LocalToWorld,
                                               bbox const& LocalBBox, s32 MaxCollectedCount );
    static void  GetCollectedProjection      ( Projection const* pProjections,
                                               s32 const* pCollectedIndices, s32 CollectedCount,
                                               s32& CurrentCollectedIndex, matrix4& ProjectionMatrix,
                                               texture const*& pTexture );
    static xbool ProjectionIntersectsBBox    ( Projection const& ProjectionData,
                                               bbox const& WorldBBox );
    static xbool AnyProjectionIntersectsBBox ( Projection const* pProjections, s32 ProjectionCount,
                                               bbox const& WorldBBox );
    static void  SetupProjection             ( Projection& Destination, matrix4 const& LocalToWorld,
                                               radian FieldOfView, f32 Length,
                                               texture::handle Texture );

    //-------------------------------------------------------------------------

public:

    ProjectionTextureRegistry( void );
    virtual ~ProjectionTextureRegistry( void );

    // Rebuild the projection list once per frame.
    void ClearProjTextures ( void );
    void AddProjLight      ( matrix4 const& LocalToWorld, radian FieldOfView, f32 Length,
                             texture::handle Texture );
    void AddProjShadow     ( matrix4 const& LocalToWorld, radian FieldOfView, f32 Length,
                             texture::handle Texture );

    s32                    CollectLights          ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                                    s32 MaxLightCount = MaxLightProjectionCount );
    void                   GetCollectedLight      ( matrix4& LightMatrix, texture const*& pTexture );
    s32                    GetProjLightCount      ( void ) const;
    void                   GetProjLight           ( s32 Index, matrix4& LightMatrix,
                                                    texture const*& pTexture ) const;
    texture::handle const& GetProjLightTexture    ( s32 Index ) const;
    s32                    CollectShadows         ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                                    s32 MaxShadowCount = MaxShadowProjectionCount );
    void                   GetCollectedShadow     ( matrix4& ShadowMatrix, texture const*& pTexture );
    s32                    GetProjShadowCount     ( void ) const;
    void                   GetProjShadow          ( s32 Index, matrix4& ShadowMatrix,
                                                    texture const*& pTexture ) const;
    texture::handle const& GetProjShadowTexture   ( s32 Index ) const;
    geom_inst_features CollectProjFeatures( u32 RenderFlags, bbox const& WorldBBox );

    //-------------------------------------------------------------------------

protected:

    Projection m_LightProjections[MaxLightProjectionCount];
    Projection m_ShadowProjections[MaxShadowProjectionCount];
    s32        m_LightProjectionCount;
    s32        m_ShadowProjectionCount;
    s32        m_CollectedLightCount;
    s32        m_CollectedShadowCount;
    s32        m_CurrentCollectedLightIndex;
    s32        m_CurrentCollectedShadowIndex;
    s32        m_CollectedLightIndices[MaxLightProjectionCount];
    s32        m_CollectedShadowIndices[MaxShadowProjectionCount];
};

//=========================================================================
//  PIPELINE-OWNED SERVICE
//=========================================================================

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline void ProjectionTextureRegistry::ClearProjTextures( void )
{
    ClearProjections( m_LightProjections, m_LightProjectionCount );
    ClearProjections( m_ShadowProjections, m_ShadowProjectionCount );

    m_CollectedLightCount          = 0;
    m_CollectedShadowCount         = 0;
    m_CurrentCollectedLightIndex   = 0;
    m_CurrentCollectedShadowIndex  = 0;
}

//=========================================================================

inline s32 ProjectionTextureRegistry::GetProjLightCount( void ) const
{
    return m_LightProjectionCount;
}

//=========================================================================

inline void ProjectionTextureRegistry::GetProjLight( s32 Index, matrix4& LightMatrix, texture const*& pTexture ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_LightProjectionCount ) );

    Projection const& ProjectionData = m_LightProjections[Index];
    LightMatrix = ProjectionData.m_Matrix;
    pTexture = ProjectionData.m_Texture.GetPointer();
}

//=========================================================================

inline texture::handle const& ProjectionTextureRegistry::GetProjLightTexture( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_LightProjectionCount ) );
    return m_LightProjections[Index].m_Texture;
}

//=========================================================================

inline s32 ProjectionTextureRegistry::GetProjShadowCount( void ) const
{
    return m_ShadowProjectionCount;
}

//=========================================================================

inline void ProjectionTextureRegistry::GetProjShadow( s32 Index, matrix4& ShadowMatrix, texture const*& pTexture ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_ShadowProjectionCount ) );

    Projection const& ProjectionData = m_ShadowProjections[Index];
    ShadowMatrix = ProjectionData.m_Matrix;
    pTexture = ProjectionData.m_Texture.GetPointer();
}

//=========================================================================

inline texture::handle const& ProjectionTextureRegistry::GetProjShadowTexture( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_ShadowProjectionCount ) );
    return m_ShadowProjections[Index].m_Texture;
}

//=========================================================================
} // namespace render

#endif // PROJECTION_TEXTURE_REGISTRY_HPP
//=========================================================================
