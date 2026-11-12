//=========================================================================
//
//  FrameCompiler_ShadowMaps.cpp
//
//  Shadow map utilities for geometry submission.
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
#include "../../Features/Shadow/ShadowFeature.hpp"

namespace render
{

namespace
{
const u32 kInvalidShadowIndex = 0xFFFFFFFFu;
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool FrameCompiler::InitShadowMaps( void )
{
    x_memset( m_FaceShadowMatrices, 0, sizeof( m_FaceShadowMatrices ) );
    x_memset( &m_ShadowMapData, 0, sizeof( m_ShadowMapData ) );

    for( s32 i = 0; i < LightSystem::MAX_SHADOW_LIGHTS; ++i )
    {
        m_DynamicLightShadowIndex[i] = kInvalidShadowIndex;
    }

    rbuffer_desc MatrixDesc;
    MatrixDesc.Size       = sizeof( m_FaceShadowMatrices );
    MatrixDesc.Stride     = sizeof( matrix4 );
    MatrixDesc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    MatrixDesc.pDebugName = "GeomShadowMatrices";

    if( !rbuffer_Create( m_ShadowMatrixBuffer, MatrixDesc ) )
    {
        return FALSE;
    }

    rbuffer_desc DataDesc;
    DataDesc.Size       = sizeof( m_ShadowMapData );
    DataDesc.Stride     = sizeof( vector4 );
    DataDesc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    DataDesc.pDebugName = "GeomShadowData";

    if( !rbuffer_Create( m_ShadowDataBuffer, DataDesc ) )
    {
        rbuffer_Destroy( m_ShadowMatrixBuffer );
        return FALSE;
    }

    m_pCachedFaceShadowResource      = NULL;
    m_pCachedFaceShadowDepthResource = NULL;
    m_areShadowMapConstantsValid     = FALSE;

    return TRUE;
}

//=========================================================================

void FrameCompiler::KillShadowMaps( void )
{
    rbuffer_Destroy( m_ShadowDataBuffer );
    rbuffer_Destroy( m_ShadowMatrixBuffer );
    m_pCachedFaceShadowResource      = NULL;
    m_pCachedFaceShadowDepthResource = NULL;
    m_areShadowMapConstantsValid     = FALSE;
}

//=========================================================================

xbool FrameCompiler::PrepareSharedShadowData( FrameGraphBuilder& Graph )
{
    return m_areShadowMapConstantsValid || BuildShadowMapConstants( Graph );
}

//=========================================================================

xbool FrameCompiler::ApplyPassInputs( const BackendPassInputs& Inputs )
{
    const shader_resource* pSample = Inputs.pShadowSample ? rtarget_GetShaderResource( *Inputs.pShadowSample ) : NULL;
    const shader_resource* pDepth  = Inputs.pShadowDepth ? rtarget_GetShaderResource( *Inputs.pShadowDepth ) : NULL;

    if( ( Inputs.pShadowSample || Inputs.pShadowDepth ) && ( !pSample || !pDepth ) )
    {
        return FALSE;
    }

    const shader_resource* pWhite    = vram_GetShaderResource( m_WhiteTexture );
    m_pCachedFaceShadowResource      = pSample ? pSample : pWhite;
    m_pCachedFaceShadowDepthResource = pDepth ? pDepth : pWhite;
    m_pDistortionSceneResource       = Inputs.pDistortionScene;
    m_pAmbientVisibilityResource     = Inputs.pAmbientVisibility
                                          ? rtarget_GetShaderResource( *Inputs.pAmbientVisibility ) : pWhite;
    m_hasAmbientVisibility           = Inputs.pAmbientVisibility != NULL;

    return m_pCachedFaceShadowResource && m_pCachedFaceShadowDepthResource && m_pAmbientVisibilityResource;
}

//=========================================================================

u32 FrameCompiler::GetDynamicLightShadowIndex( s32 DynamicLightIndex ) const
{
    if( ( DynamicLightIndex < 0 ) || ( DynamicLightIndex >= LightSystem::MAX_SHADOW_LIGHTS ) )
    {
        return kInvalidShadowIndex;
    }

    return m_DynamicLightShadowIndex[DynamicLightIndex];
}

//=========================================================================

const shader_resource* FrameCompiler::GetFaceShadowResource( void ) const
{
    return m_pCachedFaceShadowResource;
}

//=========================================================================

const shader_resource* FrameCompiler::GetFaceShadowDepthResource( void ) const
{
    return m_pCachedFaceShadowDepthResource;
}

//=========================================================================

const shader_resource* FrameCompiler::GetShadowMatricesResource( void ) const
{
    return rbuffer_GetResource( m_ShadowMatrixBuffer );
}

//=========================================================================

const shader_resource* FrameCompiler::GetShadowDataResource( void ) const
{
    return rbuffer_GetResource( m_ShadowDataBuffer );
}

//=========================================================================

rstate_sampler_preset FrameCompiler::GetFaceShadowSampler( void ) const
{
    return m_FaceShadowSamplerPreset;
}

//=========================================================================

xbool FrameCompiler::BuildShadowMapConstants( FrameGraphBuilder& Graph )
{
    x_memset( m_FaceShadowMatrices, 0, sizeof( m_FaceShadowMatrices ) );
    x_memset( &m_ShadowMapData, 0, sizeof( m_ShadowMapData ) );

    for( s32 i = 0; i < LightSystem::MAX_SHADOW_LIGHTS; ++i )
    {
        m_DynamicLightShadowIndex[i] = kInvalidShadowIndex;
    }

    ShadowFeature* pShadowFeature = m_pContext->pShadows;

    if( !pShadowFeature )
    {
        return FALSE;
    }

    const ShadowRenderer&  ShadowBackend        = pShadowFeature->Backend();
    const f32              DepthAtlasTexelSize  = ShadowBackend.GetAtlasTexelSize();
    const vector4          ShadowFilterParams   = ShadowBackend.GetShadowFilterParams();
    const f32              SampleAtlasTexelSize = ShadowFilterParams.GetX();
    const ShadowFilterType FilterType           = m_pContext->pShadowAtlas->GetShadowFilterType();
    s32                    nFaceShadows         = 0;
    s32                    nPointLights         = 0;
    const s32              nSources             = m_pContext->pShadowAtlas->GetSourceCount();

    for( s32 i = 0; i < nSources; i++ )
    {
        const ShadowMapAtlas::ShadowSource& Source = m_pContext->pShadowAtlas->GetSource( i );

        if( nFaceShadows >= MAX_SHADOW_SOURCES )
        {
            continue;
        }

        const s32 PackedFaceIndex                                 = nFaceShadows;
        m_FaceShadowMatrices[PackedFaceIndex]                     = Source.WorldToAtlas;
        m_ShadowMapData.FaceShadowLightPosRadius[PackedFaceIndex] = Source.LightPosRadius;
        m_ShadowMapData.FaceShadowLightDirRadial[PackedFaceIndex] = Source.FaceLightDirFalloff;
        m_ShadowMapData.FaceShadowLightData[PackedFaceIndex]      = Source.FaceLightData;

        const f32 Radius       = MAX( Source.LightPosRadius.GetW(), 1.0e-4f );
        const f32 InnerRadius  = Radius * ( 1.0f - x_clamp( Source.LightFalloff, 0.0f, 1.0f ) );
        const f32 InverseRange = 1.0f / MAX( Radius - InnerRadius, 1.0e-4f );
        m_ShadowMapData.FaceShadowLightData[PackedFaceIndex].GetZ()      = InnerRadius;
        m_ShadowMapData.FaceShadowLightDirRadial[PackedFaceIndex].GetW() = InverseRange;

        const f32 Resolution = MAX( static_cast<f32>( Source.AtlasWidth ), 1.0f );
        const f32 TanHalfFov = 1.0f / Source.ProjectionScale;
        m_ShadowMapData.FaceShadowProjectionData[PackedFaceIndex].Set( Source.NearZ, Source.FarZ,
                                                                       2.0f * TanHalfFov / Resolution, Resolution );
        m_ShadowMapData.FaceShadowAtlasRect[PackedFaceIndex].Set(
            static_cast<f32>( Source.AtlasX ) * DepthAtlasTexelSize + SampleAtlasTexelSize * 0.5f,
            static_cast<f32>( Source.AtlasY ) * DepthAtlasTexelSize + SampleAtlasTexelSize * 0.5f,
            static_cast<f32>( Source.AtlasX + Source.AtlasWidth ) * DepthAtlasTexelSize - SampleAtlasTexelSize * 0.5f,
            static_cast<f32>( Source.AtlasY + Source.AtlasHeight ) * DepthAtlasTexelSize -
                SampleAtlasTexelSize * 0.5f );
        nFaceShadows++;

        const xbool HasDynamicLightIndex =
            ( Source.DynamicLightIndex >= 0 ) && ( Source.DynamicLightIndex < LightSystem::MAX_SHADOW_LIGHTS );

        if( Source.Type != ShadowMapAtlas::SHADOW_SOURCE_POINT_FACE )
        {
            if( HasDynamicLightIndex )
            {
                m_DynamicLightShadowIndex[Source.DynamicLightIndex] = PackedFaceIndex;
            }

            continue;
        }

        if( Source.FaceIndex != 0 )
        {
            continue;
        }

        if( ( Source.PointLightIndex < 0 ) || ( Source.PointLightIndex >= MAX_SHADOW_LIGHTS ) )
        {
            continue;
        }

        if( nPointLights >= MAX_SHADOW_LIGHTS )
        {
            continue;
        }

        s32 FaceCount = 1;

        while( FaceCount < POINT_SHADOW_FACE_COUNT && i + FaceCount < nSources )
        {
            const ShadowMapAtlas::ShadowSource& Face = m_pContext->pShadowAtlas->GetSource( i + FaceCount );

            if( Face.Type != ShadowMapAtlas::SHADOW_SOURCE_POINT_FACE ||
                Face.PointLightIndex != Source.PointLightIndex || Face.FaceIndex != FaceCount )
            {
                break;
            }

            ++FaceCount;
        }

        m_ShadowMapData.PointShadowLightPosRadius[nPointLights] = Source.LightPosRadius;
        m_ShadowMapData.PointShadowLightData[nPointLights].Set( InnerRadius, Source.ReceiveNearZ, InverseRange, 0.0f );
        m_ShadowMapData.PointShadowLightParams[nPointLights].Set( static_cast<f32>( PackedFaceIndex ),
                                                                  static_cast<f32>( FaceCount ), 0.0f, 0.0f );

        if( HasDynamicLightIndex )
        {
            m_DynamicLightShadowIndex[Source.DynamicLightIndex] = nPointLights;
        }

        nPointLights++;
    }

    m_ShadowMapData.FaceShadowCount       = nFaceShadows;
    m_ShadowMapData.PointShadowLightCount = nPointLights;

    m_ShadowMapData.ShadowParams.Set( DepthAtlasTexelSize, ShadowBackend.GetShadowNormalBiasTexels(),
                                      ShadowBackend.GetShadowSeamBlendTexels(),
                                      FilterType == ShadowFilterType::Evsm ? 1.0f : 0.0f );
    m_ShadowMapData.ShadowFilterParams = ShadowFilterParams;

    m_pCachedFaceShadowResource      = vram_GetShaderResource( m_WhiteTexture );
    m_pCachedFaceShadowDepthResource = m_pCachedFaceShadowResource;
    m_FaceShadowSamplerPreset        = ( FilterType == ShadowFilterType::Evsm ) ? RSTATE_SAMPLER_PRESET_LINEAR_CLAMP
                                                                                : RSTATE_SAMPLER_PRESET_POINT_CLAMP;

    if( !m_pCachedFaceShadowResource || !m_pCachedFaceShadowDepthResource )
    {
        return FALSE;
    }

    rbuffer_upload_batch_item Uploads[2];
    Uploads[0].pBuffer       = &m_ShadowMatrixBuffer;
    Uploads[0].Upload.pData  = m_FaceShadowMatrices;
    Uploads[0].Upload.Size   = sizeof( m_FaceShadowMatrices );
    Uploads[0].Upload.bCycle = TRUE;

    Uploads[1].pBuffer       = &m_ShadowDataBuffer;
    Uploads[1].Upload.pData  = &m_ShadowMapData;
    Uploads[1].Upload.Size   = sizeof( m_ShadowMapData );
    Uploads[1].Upload.bCycle = TRUE;

    if( !Graph.QueueUploadBatch( Uploads, ARRAYSIZE( Uploads ) ) )
    {
        return FALSE;
    }

    m_areShadowMapConstantsValid = TRUE;

    return TRUE;
}

} // namespace render
