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

    if( !m_hasProjectionConstants )
    {
        return FALSE;
    }

    return shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->ProjTexturesPixel, &m_ResetProjectionConstants,
                                  sizeof( m_ResetProjectionConstants ) );
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

    if( !m_hasProjectionConstants )
    {
        return FALSE;
    }

    return shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->ProjTexturesPixel, &m_ProjectionConstants,
                                  sizeof( m_ProjectionConstants ) );
}

//=========================================================================

xbool FrameCompiler::PrepareProjectionConstants( void )
{
    m_hasProjectionConstants = FALSE;

    x_memset( &m_ResetProjectionConstants, 0, sizeof( m_ResetProjectionConstants ) );
    m_ResetProjectionConstants.ProjectionAtlasParams = m_pContext->pProjection->GetSamplingParams();
    m_ProjectionConstants                            = m_ResetProjectionConstants;

    ProjectionTextureConstants& Constants = m_ProjectionConstants;

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

    m_hasProjectionConstants = TRUE;

    return TRUE;
}

} // namespace render
