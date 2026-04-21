//==============================================================================
//
//  MaterialMgr.cpp
//
//  Material Manager for PC platform
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "MaterialMgr.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

material_mgr g_MaterialMgr;

//==============================================================================
//  INITIALIZATION / SHUTDOWN
//==============================================================================

void material_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    x_DebugMsg( "MaterialMgr: Initializing material manager\n" );

    // Initialize member variables
    m_pCurrentTexture       = NULL;
    m_pCurrentDetailTexture = NULL;
    m_pCurrentEnvironmentTexture = NULL;
    m_pCurrentEnvCubemap    = NULL;

    // Initialize shaders and resources
    InitRigidShaders();
    InitSkinShaders();
    InitProjTextures();

    m_bInitialized = TRUE;
    x_DebugMsg( "MaterialMgr: Material manager initialized successfully\n" );
}

//==============================================================================

void material_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    x_DebugMsg( "MaterialMgr: Shutting down material manager\n" );

    KillRigidShaders();
    KillSkinShaders();
    KillProjTextures();
    InvalidateCache();
    SetEnvironmentCubemap( NULL );

    m_bInitialized = FALSE;
    x_DebugMsg( "MaterialMgr: Material manager shutdown complete\n" );
}

//==============================================================================

material_mgr::material_constants material_mgr::BuildMaterialFlags( const material* pMaterial,
                                                                  u32             RenderFlags,
                                                                  xbool           IncludeVertexColor ) const
{
    material_constants constants;
    constants.Flags    = 0;
    constants.AlphaRef = 0.0f;

    if( !pMaterial )
        return constants;

    const xbool bPunchThru = !!(pMaterial->m_Flags & geom::material::FLAG_IS_PUNCH_THRU);

    switch( pMaterial->m_Type )
    {
        case Material_Diff:
            break;
        case Material_Alpha:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST;
            break;
        case Material_Diff_PerPixelIllum:
            constants.Flags |= MATERIAL_FLAG_PERPIXEL_ILLUM;
            break;
        case Material_Alpha_PerPixelIllum:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPIXEL_ILLUM;
            break;
        case Material_Alpha_PerPolyIllum:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ILLUM;
            break;
        case Material_Diff_PerPixelEnv:
            constants.Flags |= MATERIAL_FLAG_PERPIXEL_ENV;
            break;
        case Material_Alpha_PerPolyEnv:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ENV;
            break;
        case Material_Distortion:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_DISTORTION;
            break;
        case Material_Distortion_PerPolyEnv:
            constants.Flags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_PERPOLY_ENV;
            break;
    }

    if( pMaterial->m_Flags & geom::material::FLAG_HAS_DETAIL_MAP )
        constants.Flags |= MATERIAL_FLAG_DETAIL;

    //if( pMaterial->m_Flags & geom::material::FLAG_HAS_ENV_MAP )
        constants.Flags |= MATERIAL_FLAG_ENVIRONMENT;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_CUBE_MAP )
        constants.Flags |= MATERIAL_FLAG_ENV_CUBEMAP;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_VIEW_SPACE )
        constants.Flags |= MATERIAL_FLAG_ENV_VIEWSPACE;

    if( pMaterial->m_Flags & geom::material::FLAG_IS_PUNCH_THRU )
        constants.Flags |= MATERIAL_FLAG_ALPHA_TEST;

    if( pMaterial->m_Flags & geom::material::FLAG_DOUBLE_SIDED )
        constants.Flags |= MATERIAL_FLAG_TWO_SIDED;

    if( pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE )
        constants.Flags |= MATERIAL_FLAG_ADDITIVE;
    else if( pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE )
        constants.Flags |= MATERIAL_FLAG_SUBTRACTIVE;

    if( pMaterial->m_Flags & geom::material::FLAG_ILLUM_USES_DIFFUSE )
        constants.Flags |= MATERIAL_FLAG_ILLUM_USE_DIFFUSE;

    if( constants.Flags & MATERIAL_FLAG_ALPHA_TEST )
        constants.AlphaRef = bPunchThru ? 0.5f : (4.0f / 255.0f);

    if( IncludeVertexColor )
        constants.Flags |= MATERIAL_FLAG_VERTEX_COLOR;

    return constants;
}

//==============================================================================

u32 material_mgr::BuildInstanceFlags( u32 RenderFlags ) const
{
    u32 flags = 0;

    if( !(RenderFlags & render::DISABLE_SPOTLIGHT) )
        flags |= INSTANCE_FLAG_PROJ_LIGHT;
    
    if( !(RenderFlags & render::DISABLE_PROJ_SHADOWS) )
        flags |= INSTANCE_FLAG_PROJ_SHADOW;

    //static f32 fDist = 1200.0f;
    //if( s_DetailMapPresent && (pc_CalcDistance( *Inst.Data.Rigid.pL2W,s_CurrentBBox ) < fDist) )
    //    flags |= INSTANCE_FLAG_DETAIL;

    if( render::IsFilterLightEnabled() && !(RenderFlags & render::DISABLE_FILTERLIGHT) )
        flags |= INSTANCE_FLAG_FILTERLIGHT;

    if( RenderFlags & render::CLIPPED )
        flags |= INSTANCE_FLAG_CLIPPED;

    //if ( ( RenderFlags & render::SHADOW_PASS ) && s_bMatCanReceiveShadow )
    //    flags |= INSTANCE_FLAG_SHADOW_PASS;

    if( RenderFlags & render::GLOWING )
        flags |= INSTANCE_FLAG_GLOWING;

    if( RenderFlags & render::FADING_ALPHA )
	{
        flags |= INSTANCE_FLAG_FADING_ALPHA;
		//flags &= ~(INSTANCE_FLAG_DETAIL | INSTANCE_FLAG_SHADOW_PASS);
	}

    return flags;
}

//==============================================================================

cb_lighting material_mgr::BuildLightingConstants( const d3d_lighting* pLighting ) const
{
    cb_lighting lighting;
    x_memset( &lighting, 0, sizeof(cb_lighting) );

    if( pLighting )
    {
        for( s32 i = 0; i < MAX_GEOM_LIGHTS; i++ )
        {
            if( i < pLighting->LightCount )
            {
                lighting.LightVec[i] = pLighting->LightVec[i];
                lighting.LightCol[i] = pLighting->LightCol[i];
            }
        }
        lighting.LightCount  = pLighting->LightCount;
        lighting.LightAmbCol = pLighting->AmbCol;
    }
    return lighting;
}

//==============================================================================
//  GENERAL STATE HELPERS
//==============================================================================

void material_mgr::ApplyRenderStates( const material* pMaterial, u32 RenderFlags )
{
    s32 blendMode   = STATE_BLEND_NONE;
    s32 depthMode   = STATE_DEPTH_NORMAL;
    s32 rasterMode  = STATE_RASTER_SOLID;
    s32 samplerMode = STATE_SAMPLER_ANISOTROPIC_WRAP;

    material_type materialType = Material_Diff;

    xbool bAlphaMaterial = FALSE;
    xbool bForceZFill    = FALSE;
    xbool bDoubleSided   = FALSE;
    xbool bAdditive      = FALSE;
    xbool bSubtractive   = FALSE;

    if( pMaterial )
    {
        materialType   = (material_type)pMaterial->m_Type;
        bAlphaMaterial = IsAlphaMaterial( materialType );
        bForceZFill    = !!(pMaterial->m_Flags & geom::material::FLAG_FORCE_ZFILL);
        bDoubleSided   = !!(pMaterial->m_Flags & geom::material::FLAG_DOUBLE_SIDED);
        bAdditive      = !!(pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE);
        bSubtractive   = !!(pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE);
    }

    const u32   FadeMask     = (render::FADING_ALPHA | render::INSTFLAG_FADING_ALPHA);
    const xbool bFadingAlpha = !!(RenderFlags & FadeMask);

    xbool bEnableBlend = FALSE;

    if( bFadingAlpha )
    {
        blendMode    = STATE_BLEND_ALPHA;
        bEnableBlend = TRUE;
    }
    else
    {
        switch( materialType )
        {
            case Material_Alpha:
            case Material_Alpha_PerPolyEnv:
            case Material_Alpha_PerPixelIllum:
            case Material_Alpha_PerPolyIllum:
            {
                if( bAdditive )
                {
                    blendMode = STATE_BLEND_ADD;
                }
                else if( bSubtractive )
                {
                    blendMode = STATE_BLEND_SUB;
                }
                else
                {
                    blendMode = STATE_BLEND_ALPHA;
                }

                bEnableBlend = TRUE;
            }
            break;

            case Material_Distortion:
            case Material_Distortion_PerPolyEnv:
            {
                // Distortion classes keep blending disabled; they rely on shader
                // authored offsets rather than color compositing.
                blendMode    = STATE_BLEND_NONE;
                bEnableBlend = FALSE;
            }
            break;

            default:
                break;
        }
    }

    const xbool bDisableDepthWrite = ((bAlphaMaterial || bEnableBlend) && !bForceZFill);

    depthMode = bDisableDepthWrite ? STATE_DEPTH_NO_WRITE : STATE_DEPTH_NORMAL;

    //const xbool bWireframe       = !!(RenderFlags & render::WIREFRAME);
    //const xbool bWireframeNoCull = !!(RenderFlags & render::WIREFRAME2);
    const xbool bDisableCull     = bDoubleSided || bAlphaMaterial;

    //if( bWireframeNoCull )
    //{
    //    rasterMode = STATE_RASTER_WIRE_NO_CULL;
    //}
    //else if( bWireframe )
    //{
    //    rasterMode = bDisableCull ? STATE_RASTER_WIRE_NO_CULL : STATE_RASTER_WIRE;
    //}
    //else
    {
        rasterMode = bDisableCull ? STATE_RASTER_SOLID_NO_CULL : STATE_RASTER_SOLID;
    }

    //if( RenderFlags & (render::CLIPPED | render::INSTFLAG_CLIPPED) )
    //{
    //    samplerMode = STATE_SAMPLER_LINEAR_CLAMP;
    //}

    state_SetBlend      ((state_blend_mode)blendMode );
    state_SetDepth      ((state_depth_mode)depthMode );
    state_SetRasterizer ((state_raster_mode)rasterMode );
    state_SetSampler    ((state_sampler_mode)samplerMode, 0, STATE_SAMPLER_STAGE_PS );
}

//==============================================================================

void material_mgr::SetBitmap( const xbitmap* pBitmap, texture_slot slot )
{
    if( !g_pd3dContext )
        return;

    // Cache check to avoid redundant texture changes
    if( slot == TEXTURE_SLOT_DIFFUSE && pBitmap == m_pCurrentTexture )
        return;
    if( slot == TEXTURE_SLOT_DETAIL && pBitmap == m_pCurrentDetailTexture )
        return;
    if( slot == TEXTURE_SLOT_ENVIRONMENT &&
        (pBitmap == m_pCurrentEnvironmentTexture) &&
        (m_pCurrentEnvCubemap == NULL) )
    {
        return;
    }

    if( slot == TEXTURE_SLOT_ENVIRONMENT )
    {
        // Ensure cubemap binding is cleared when switching to 2D textures.
        SetEnvironmentCubemap( NULL );
    }

    if( pBitmap )
    {
        ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBitmap );

        if( pSRV )
        {
            g_pd3dContext->PSSetShaderResources( slot, 1, &pSRV );

            // Update cache
            if( slot == TEXTURE_SLOT_DIFFUSE )
                m_pCurrentTexture = pBitmap;
            else if( slot == TEXTURE_SLOT_DETAIL )
                m_pCurrentDetailTexture = pBitmap;
            else if( slot == TEXTURE_SLOT_ENVIRONMENT )
                m_pCurrentEnvironmentTexture = pBitmap;
        }
        else
        {
            x_DebugMsg( "MaterialMgr: ERROR - vram_GetSRV returned NULL for slot %d\n", slot );
        }
    }
    else
    {
        // Clear texture slot
        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( slot, 1, &pNullSRV );

        // Update cache
        if( slot == TEXTURE_SLOT_DIFFUSE )
            m_pCurrentTexture = NULL;
        else if( slot == TEXTURE_SLOT_DETAIL )
            m_pCurrentDetailTexture = NULL;
        else if( slot == TEXTURE_SLOT_ENVIRONMENT )
            m_pCurrentEnvironmentTexture = NULL;
    }
}

//==============================================================================

void material_mgr::SetEnvironmentCubemap( const cubemap* pCubemap )
{
    if( !g_pd3dContext )
        return;

    if( m_pCurrentEnvCubemap == pCubemap )
        return;

    ID3D11ShaderResourceView* pSRV = NULL;

    if( pCubemap )
    {
        if( !pCubemap->m_hTexture )
        {
            x_DebugMsg( "MaterialMgr: WARNING - Cubemap has no VRAM handle\n" );
        }
        else
        {
            s32 vramID = (s32)(uaddr)pCubemap->m_hTexture;
            pSRV = vram_GetSRV( vramID );
            if( !pSRV )
            {
                x_DebugMsg( "MaterialMgr: ERROR - Failed to get cubemap SRV (id %d)\n", vramID );
            }
        }
    }

    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_ENVIRONMENT_CUBE, 1, &pSRV );

    if( pCubemap && pSRV )
    {
        m_pCurrentEnvCubemap = pCubemap;
    }
    else
    {
        m_pCurrentEnvCubemap = NULL;
    }

    m_pCurrentEnvironmentTexture = NULL;
}

//==============================================================================

void material_mgr::InvalidateCache( void )
{
    m_pCurrentTexture = NULL;
    m_pCurrentDetailTexture = NULL;
    m_pCurrentEnvironmentTexture = NULL;
    m_pCurrentEnvCubemap = NULL;
    m_bRigidFrameDirty   = TRUE;
    m_bRigidObjectDirty  = TRUE;
    m_bRigidLightingDirty = TRUE;
    m_bSkinFrameDirty   = TRUE;
    m_bSkinLightingDirty = TRUE;
}

//==============================================================================

// Skin bone buffer access for SoftVertexMgr
ID3D11Buffer* material_mgr::GetSkinBoneBuffer( void )
{
    return m_pSkinBoneBuffer;
}