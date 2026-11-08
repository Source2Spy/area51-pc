//=========================================================================
//
//  ProjectionTextureRegistry.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ProjectionTextureRegistry.hpp"
#ifndef X_RETAIL
#include "Pipeline/RenderPipeline.hpp"
#endif

namespace render
{

//=========================================================================
//  PIPELINE-OWNED SERVICE
//=========================================================================

//=========================================================================
//  FUNCTIONS
//=========================================================================

ProjectionTextureRegistry::ProjectionTextureRegistry( void )
    : m_LightProjectionCount( 0 )
    , m_ShadowProjectionCount( 0 )
    , m_CollectedLightCount( 0 )
    , m_CollectedShadowCount( 0 )
    , m_CurrentCollectedLightIndex( 0 )
    , m_CurrentCollectedShadowIndex( 0 )
{
}

//=========================================================================

ProjectionTextureRegistry::~ProjectionTextureRegistry( void )
{
    ClearProjTextures();
}

//=========================================================================

void ProjectionTextureRegistry::ClearProjections( Projection* pProjections, s32& ProjectionCount )
{
    ASSERT( pProjections );
    ASSERT( ProjectionCount >= 0 );

    for( s32 ProjectionIndex = 0; ProjectionIndex < ProjectionCount; ProjectionIndex++ )
    {
        pProjections[ProjectionIndex].m_Texture.Destroy();
    }

    ProjectionCount = 0;
}

//=========================================================================

void ProjectionTextureRegistry::AddProjection( Projection* pProjections, s32& ProjectionCount,
                                    s32 MaxProjectionCount, matrix4 const& LocalToWorld,
                                    radian FieldOfView, f32 Length, texture::handle Texture )
{
    ASSERT( pProjections );
    ASSERT( ProjectionCount >= 0 );
    ASSERT( ProjectionCount <= MaxProjectionCount );
    ASSERT( MaxProjectionCount >= 0 );
    ASSERT( Texture.GetPointer() );

    if( Texture.GetPointer() == nullptr )
    {
        return;
    }

    if( ProjectionCount >= MaxProjectionCount )
    {
        return;
    }

    SetupProjection( pProjections[ProjectionCount], LocalToWorld, FieldOfView, Length, Texture );
    ProjectionCount++;
}

//=========================================================================

s32 ProjectionTextureRegistry::CollectProjections( Projection const* pProjections, s32 ProjectionCount,
                                        s32* pCollectedIndices, s32& CollectedCount,
                                        s32& CurrentCollectedIndex, matrix4 const& LocalToWorld,
                                        bbox const& LocalBBox, s32 MaxCollectedCount )
{
    ASSERT( pProjections );
    ASSERT( pCollectedIndices );
    ASSERT( ProjectionCount >= 0 );
    ASSERT( MaxCollectedCount >= 0 );

    bbox WorldBBox = LocalBBox;
    WorldBBox.Transform( LocalToWorld );

    CollectedCount = 0;
    CurrentCollectedIndex = 0;

    if( MaxCollectedCount <= 0 )
    {
        return 0;
    }

    for( s32 ProjectionIndex = 0;
         ( ProjectionIndex < ProjectionCount ) && ( CollectedCount < MaxCollectedCount );
         ProjectionIndex++ )
    {
        if( ProjectionIntersectsBBox( pProjections[ProjectionIndex], WorldBBox ) )
        {
            pCollectedIndices[CollectedCount++] = ProjectionIndex;
        }
    }

    return CollectedCount;
}

//=========================================================================

void ProjectionTextureRegistry::GetCollectedProjection( Projection const* pProjections,
                                             s32 const* pCollectedIndices, s32 CollectedCount,
                                             s32& CurrentCollectedIndex, matrix4& ProjectionMatrix,
                                             texture const*& pTexture )
{
    ASSERT( pProjections );
    ASSERT( pCollectedIndices );
    ASSERT( CollectedCount >= 0 );
    ASSERT( ( CurrentCollectedIndex >= 0 ) && ( CurrentCollectedIndex < CollectedCount ) );

    s32 const ProjectionIndex = pCollectedIndices[CurrentCollectedIndex++];
    Projection const& ProjectionData = pProjections[ProjectionIndex];

    ProjectionMatrix = ProjectionData.m_Matrix;
    pTexture = ProjectionData.m_Texture.GetPointer();
}

//=========================================================================

xbool ProjectionTextureRegistry::ProjectionIntersectsBBox( Projection const& ProjectionData,
                                                bbox const& WorldBBox )
{
    return ( ProjectionData.m_View.BBoxInView( WorldBBox ) != view::VISIBLE_NONE );
}

//=========================================================================

xbool ProjectionTextureRegistry::AnyProjectionIntersectsBBox( Projection const* pProjections,
                                                   s32 ProjectionCount,
                                                   bbox const& WorldBBox )
{
    ASSERT( pProjections );
    ASSERT( ProjectionCount >= 0 );

    for( s32 ProjectionIndex = 0; ProjectionIndex < ProjectionCount; ProjectionIndex++ )
    {
        if( ProjectionIntersectsBBox( pProjections[ProjectionIndex], WorldBBox ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

void ProjectionTextureRegistry::AddProjLight( matrix4 const& LocalToWorld, radian FieldOfView, f32 Length,
                                             texture::handle Texture )
{
#ifndef X_RETAIL
    if( GetRenderPipeline().GetDebugOptions().DisableProjectedLights )
    {
        return;
    }
#endif
    AddProjection( m_LightProjections, m_LightProjectionCount, MaxLightProjectionCount,
                   LocalToWorld, FieldOfView, Length, Texture );
}

//=========================================================================

void ProjectionTextureRegistry::AddProjShadow( matrix4 const& LocalToWorld, radian FieldOfView, f32 Length,
                                              texture::handle Texture )
{
#ifndef X_RETAIL
    if( GetRenderPipeline().GetDebugOptions().DisableProjectedShadows )
    {
        return;
    }
#endif
    AddProjection( m_ShadowProjections, m_ShadowProjectionCount, MaxShadowProjectionCount,
                   LocalToWorld, FieldOfView, Length, Texture );
}

//=========================================================================

s32 ProjectionTextureRegistry::CollectLights( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                   s32 MaxLightCount )
{
    if( MaxLightCount > MaxLightProjectionCount )
    {
        MaxLightCount = MaxLightProjectionCount;
    }

    return CollectProjections( m_LightProjections, m_LightProjectionCount,
                               m_CollectedLightIndices, m_CollectedLightCount,
                               m_CurrentCollectedLightIndex, LocalToWorld, LocalBBox,
                               MaxLightCount );
}

//=========================================================================

void ProjectionTextureRegistry::GetCollectedLight( matrix4& LightMatrix, texture const*& pTexture )
{
    GetCollectedProjection( m_LightProjections, m_CollectedLightIndices,
                            m_CollectedLightCount, m_CurrentCollectedLightIndex,
                            LightMatrix, pTexture );
}

//=========================================================================

s32 ProjectionTextureRegistry::CollectShadows( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                    s32 MaxShadowCount )
{
    if( MaxShadowCount > MaxShadowProjectionCount )
    {
        MaxShadowCount = MaxShadowProjectionCount;
    }

    return CollectProjections( m_ShadowProjections, m_ShadowProjectionCount,
                               m_CollectedShadowIndices, m_CollectedShadowCount,
                               m_CurrentCollectedShadowIndex, LocalToWorld, LocalBBox,
                               MaxShadowCount );
}

//=========================================================================

void ProjectionTextureRegistry::GetCollectedShadow( matrix4& ShadowMatrix, texture const*& pTexture )
{
    GetCollectedProjection( m_ShadowProjections, m_CollectedShadowIndices,
                            m_CollectedShadowCount, m_CurrentCollectedShadowIndex,
                            ShadowMatrix, pTexture );
}

//=========================================================================

geom_inst_features ProjectionTextureRegistry::CollectProjFeatures( u32 RenderFlags, bbox const& WorldBBox )
{
    geom_inst_features ProjFeatures = 0;

    if( ( m_LightProjectionCount == 0 ) && ( m_ShadowProjectionCount == 0 ) )
    {
        return ProjFeatures;
    }

    if( !( RenderFlags & render::DISABLE_PROJ_LIGHT ) &&
        AnyProjectionIntersectsBBox( m_LightProjections, m_LightProjectionCount, WorldBBox ) )
    {
        ProjFeatures |= render::GEOM_INST_PROJ_LIGHT;
    }

    if( !( RenderFlags & render::DISABLE_PROJ_SHADOWS ) &&
        AnyProjectionIntersectsBBox( m_ShadowProjections, m_ShadowProjectionCount, WorldBBox ) )
    {
        ProjFeatures |= render::GEOM_INST_PROJ_SHADOW;
    }

    return ProjFeatures;
}

//=========================================================================

void ProjectionTextureRegistry::SetupProjection( Projection& Destination, matrix4 const& LocalToWorld,
                                      radian FieldOfView, f32 Length, texture::handle Texture )
{
    ASSERT( Texture.GetPointer() );
    ASSERT( Length > 1.0f );

    Destination.m_Texture = Texture;

    texture* pProjectionTexture = Texture.GetPointer();
    xbitmap& ProjectionBitmap = pProjectionTexture->m_bitmap;

    Destination.m_View.SetXFOV( FieldOfView );
    Destination.m_View.SetZLimits( 1.0f, Length );
    Destination.m_View.SetViewport( 0, 0, ProjectionBitmap.GetWidth(), ProjectionBitmap.GetHeight() );
    Destination.m_View.SetV2W( LocalToWorld );

    Destination.m_Matrix = Destination.m_View.GetV2C();
    Destination.m_Matrix( 2, 2 ) = -1.0f / ( Length - 1.0f );
    Destination.m_Matrix( 3, 2 ) = 1.0f + 1.0f / ( Length - 1.0f );
    Destination.m_Matrix *= Destination.m_View.GetW2V();
    Destination.m_Matrix.Scale( vector3( 0.5f, -0.5f, 1.0f ) );
    Destination.m_Matrix.Translate( vector3( 0.5f, 0.5f, 0.0f ) );
}
} // namespace render
