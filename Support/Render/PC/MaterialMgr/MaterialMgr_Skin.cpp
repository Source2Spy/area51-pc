//==============================================================================
//
//  MaterialMgr_Skin.cpp
//
//  Skin material handling for the PC material manager
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
//  FUNCTIONS
//==============================================================================

xbool material_mgr::InitSkinShaders( void )
{
    x_DebugMsg( "MaterialMgr: Initializing skin shaders\n" );

    // Initialize member variables
    m_pSkinVertexShader   = NULL;
    m_pSkinPixelShader    = NULL;
    m_pSkinInputLayout    = NULL;
    m_pSkinFrameBuffer    = NULL;
    m_pSkinBoneBuffer     = NULL;
    m_pSkinLightBuffer    = NULL;
    m_bSkinFrameDirty     = TRUE;
    m_bSkinLightingDirty  = TRUE;

    D3D11_INPUT_ELEMENT_DESC skinLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    char shaderPath[256];
    x_sprintf( shaderPath, "a51_skin_simple.hlsl" );

    m_pSkinVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                 &m_pSkinInputLayout,
                                                                 skinLayout,
                                                                 ARRAYSIZE(skinLayout),
                                                                 "VSMain",
                                                                 "vs_5_0" );

    m_pSkinPixelShader  = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );

    m_pSkinFrameBuffer  = shader_CreateConstantBuffer( sizeof(cb_geom_frame), CB_TYPE_DYNAMIC );
    m_pSkinBoneBuffer   = shader_CreateConstantBuffer( sizeof(cb_skin_bone) * MAX_SKIN_BONES, CB_TYPE_DYNAMIC );
    m_pSkinLightBuffer  = shader_CreateConstantBuffer( sizeof(cb_lighting), CB_TYPE_DYNAMIC );

    x_DebugMsg( "MaterialMgr: Skin shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void material_mgr::KillSkinShaders( void )
{
    if( m_pSkinVertexShader )
    {
        m_pSkinVertexShader->Release();
        m_pSkinVertexShader = NULL;
    }

    if( m_pSkinPixelShader )
    {
        m_pSkinPixelShader->Release();
        m_pSkinPixelShader = NULL;
    }

    if( m_pSkinInputLayout )
    {
        m_pSkinInputLayout->Release();
        m_pSkinInputLayout = NULL;
    }

    if( m_pSkinFrameBuffer )
    {
        m_pSkinFrameBuffer->Release();
        m_pSkinFrameBuffer = NULL;
    }

    if( m_pSkinBoneBuffer )
    {
        m_pSkinBoneBuffer->Release();
        m_pSkinBoneBuffer = NULL;
    }

    if( m_pSkinLightBuffer )
    {
        m_pSkinLightBuffer->Release();
        m_pSkinLightBuffer = NULL;
    }

    x_DebugMsg( "MaterialMgr: Skin shaders released\n" );
}

//==============================================================================

void material_mgr::SetSkinMaterial( const matrix4*      pL2W,
                                    const bbox*         pBBox,
                                    const d3d_lighting* pLighting,
                                    const material*     pMaterial,
                                    u32                 RenderFlags,
                                    u8                  UOffset,
                                    u8                  VOffset,
                                    u8                  Alpha,
                                    u8                  OverrideMat )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    g_pd3dContext->IASetInputLayout( m_pSkinInputLayout );
    g_pd3dContext->VSSetShader( m_pSkinVertexShader, NULL, 0 );
    g_pd3dContext->PSSetShader( m_pSkinPixelShader, NULL, 0 );
    g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    if( !UpdateSkinConstants( pLighting, pMaterial, RenderFlags, UOffset, VOffset, Alpha, OverrideMat ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update skin constants\n" );
        return;
    }
    
    ApplyRenderStates( pMaterial, RenderFlags );

    if( pL2W && pBBox )
    {
        if( !pMaterial || g_ProjTextureMgr.CanReceiveProjTexture( *pMaterial ) )
        {
            UpdateProjTextures( *pL2W, *pBBox, 4, RenderFlags );
        }
        else
        {
            UpdateProjTextures( *pL2W,
                                *pBBox,
                                4,
                                RenderFlags | render::DISABLE_SPOTLIGHT | render::DISABLE_PROJ_SHADOWS );
        }
    }
}

//==============================================================================

//-------------------------------------------------------------------------------------------------------------------
// TODO: GS: There's a lot of code here in common with UpdateRigidConstants. 
// It's worth separating a couple of functions for this and simply calling them from there to avoid code duplication.
//-------------------------------------------------------------------------------------------------------------------

xbool material_mgr::UpdateSkinConstants( const d3d_lighting* pLighting,
                                         const material*     pMaterial,
                                         u32                 RenderFlags,
                                         u8                  UOffset,
                                         u8                  VOffset,
                                         u8                  Alpha,
                                         u8                  OverrideMat )
{
	(void)Alpha;
    (void)OverrideMat;
	
    if( !m_pSkinFrameBuffer || !m_pSkinBoneBuffer|| !m_pSkinLightBuffer  )
        return FALSE;
    
    if( !g_pd3dDevice || !g_pd3dContext )
        return FALSE;    

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    f32 nearZ = 0.0f;
    f32 farZ  = 0.0f;
    pView->GetZLimits( nearZ, farZ );

    cb_geom_frame skinFrame;
    x_memset( &skinFrame, 0, sizeof(cb_geom_frame) );
    skinFrame.View        = pView->GetW2V();
    skinFrame.Projection  = pView->GetV2C();

    const f32 invByte = 1.0f / 255.0f;

    f32 detailScale = pMaterial ? pMaterial->m_DetailScale : 1.0f;
    if( detailScale <= 0.0f )
        detailScale = 1.0f;

    skinFrame.UVAnim.Set( (f32)UOffset * invByte,
                          (f32)VOffset * invByte,
                          detailScale,
                          0.0f );

    material_constants constants = BuildMaterialFlags( pMaterial, RenderFlags, FALSE );
    constants.Flags |= BuildInstanceFlags( RenderFlags );	
	
    skinFrame.MaterialParams.Set( (f32)constants.Flags,
                                  constants.AlphaRef,
                                  nearZ,
                                  farZ );
    const vector3& camPos = pView->GetPosition();
    skinFrame.CameraPosition.Set( camPos.GetX(),
                                  camPos.GetY(),
                                  camPos.GetZ(),
                                  1.0f );
    f32 fixedAlpha = pMaterial ? pMaterial->m_FixedAlpha : 0.0f;
    const f32 cubeIntensity = ComputeCubeMapIntensity( pMaterial );
    skinFrame.EnvParams.Set( fixedAlpha, cubeIntensity, 0.0f, 0.0f );

    const cb_lighting lightMatrices = BuildLightingConstants( pLighting );

    const xbool bFrameChanged = ( m_bSkinFrameDirty ||
                                  x_memcmp( &m_CachedSkinFrame,
                                            &skinFrame,
                                            sizeof(cb_geom_frame) ) != 0 );

    if( bFrameChanged )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pSkinFrameBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map skin frame buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &skinFrame, sizeof(cb_geom_frame) );
        g_pd3dContext->Unmap( m_pSkinFrameBuffer, 0 );

        m_CachedSkinFrame = skinFrame;
        m_bSkinFrameDirty = FALSE;
    }

    g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pSkinFrameBuffer );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pSkinFrameBuffer );
    g_pd3dContext->VSSetConstantBuffers( 2, 1, &m_pSkinBoneBuffer );

    const xbool bLightingChanged = ( m_bSkinLightingDirty ||
                                     x_memcmp( &m_CachedSkinLighting,
                                               &lightMatrices,
                                               sizeof(cb_lighting) ) != 0 );

    if( bLightingChanged )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pSkinLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map skin light buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &lightMatrices, sizeof(cb_lighting) );
        g_pd3dContext->Unmap( m_pSkinLightBuffer, 0 );

        m_CachedSkinLighting = lightMatrices;
        m_bSkinLightingDirty = FALSE;
    }

    g_pd3dContext->PSSetConstantBuffers( 3, 1, &m_pSkinLightBuffer );

    return TRUE;
}