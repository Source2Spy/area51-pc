//=========================================================================
//
//  FrameCompiler_ProjTex.cpp
//
//  Projected texture utilities for geometry submission.
//
//=========================================================================

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameCompiler.hpp"

namespace render
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool FrameCompiler::ResetProjTextures( void )
{
    const ShaderBindingLayout* pBindings = GetShaderBindings( m_ActiveShaderKind );

    if( !pBindings )
    {
        return FALSE;
    }

    if( pBindings->ProjTexturesPixel == 0xFFFFFFFFu )
    {
        return TRUE;
    }

    ProjectionTextureConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.ProjectionAtlasParams = m_pContext->pProjection->GetSamplingParams();

    return shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->ProjTexturesPixel, &Constants, sizeof( Constants ) );
}

//=========================================================================

xbool FrameCompiler::UpdateProjTextures( void )
{
    const ShaderBindingLayout* pBindings = GetShaderBindings( m_ActiveShaderKind );

    if( !pBindings )
    {
        return FALSE;
    }

    if( pBindings->ProjTexturesPixel == 0xFFFFFFFFu )
    {
        return TRUE;
    }

    ProjectionTextureConstants Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.ProjectionAtlasParams = m_pContext->pProjection->GetSamplingParams();

    const s32 nProjLights =
        MIN( m_pContext->pProjections->GetProjLightCount(), ProjectionTextureRegistry::MaxLightProjectionCount );

    for( s32 i = 0; i < nProjLights; ++i )
    {
        matrix4        ProjMatrix;
        const texture* pTexture = NULL;
        m_pContext->pProjections->GetProjLight( i, ProjMatrix, pTexture );

        if( !pTexture )
        {
            return FALSE;
        }

        ProjectionAtlasRegion Region;

        if( !m_pContext->pProjection->GetRegion( m_pContext->pProjections->GetProjLightTexture( i ),
                                                 ProjectionAtlasEncoding::BlueMask, Region ) )
        {
            return FALSE;
        }

        const s32 Dest                      = Constants.ProjLightCount++;
        Constants.ProjLightMatrix[Dest]     = ProjMatrix;
        Constants.ProjLightAtlas[Dest]      = Region.UVScaleBias;
        Constants.ProjLightTileBounds[Dest] = Region.TileBounds;
        Constants.ProjLightInfo[Dest]       = Region.Sampling;
    }

    const s32 nProjShadows =
        MIN( m_pContext->pProjections->GetProjShadowCount(), ProjectionTextureRegistry::MaxShadowProjectionCount );

    for( s32 i = 0; i < nProjShadows; ++i )
    {
        matrix4        ProjMatrix;
        const texture* pTexture = NULL;
        m_pContext->pProjections->GetProjShadow( i, ProjMatrix, pTexture );

        if( !pTexture )
        {
            return FALSE;
        }

        ProjectionAtlasRegion Region;

        if( !m_pContext->pProjection->GetRegion( m_pContext->pProjections->GetProjShadowTexture( i ),
                                                 ProjectionAtlasEncoding::BlueMask, Region ) )
        {
            return FALSE;
        }

        const s32 Dest                       = Constants.ProjShadowCount++;
        Constants.ProjShadowMatrix[Dest]     = ProjMatrix;
        Constants.ProjShadowAtlas[Dest]      = Region.UVScaleBias;
        Constants.ProjShadowTileBounds[Dest] = Region.TileBounds;
        Constants.ProjShadowInfo[Dest]       = Region.Sampling;
    }

    return shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->ProjTexturesPixel, &Constants, sizeof( Constants ) );
}

} // namespace render
