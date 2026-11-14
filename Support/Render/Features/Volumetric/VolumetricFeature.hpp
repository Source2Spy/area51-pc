//=========================================================================
//
//  VolumetricFeature.hpp
//
//=========================================================================

#ifndef VOLUMETRIC_FEATURE_HPP
#define VOLUMETRIC_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../FrameGraph/FrameGraph.hpp"
#include "../../Volumetric.hpp"
#include "../../GraphicsPipelineCache.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class VolumetricFeature : public IRenderFeature
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    virtual char const* GetName( void ) const;
    virtual u32         GetGraphStageMask( void ) const;
    virtual u32         GetSceneTextureRequirements( void ) const;
    virtual xbool       Init( void );
    virtual void        Kill( void );
    virtual xbool       Declare( FrameGraphBuilder&, RenderFrameContext const& );
    virtual xbool       Build( FrameGraphBuilder&, RenderFrameContext const& );
    virtual xbool       BuildGraphStage( RenderFeatureGraphStage Stage,
                                        FrameGraphBuilder&      Graph,
                                        RenderFrameContext&     Frame );

    xbool QueuePipelines( GraphicsPipelineCache&    Cache,
                          RenderFormatPolicy const& Formats,
                          u32                       Samples );

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct PassData
    {
        struct Constants
        {
            matrix4 ViewToLocal;
            vector4 CameraLocal;
            vector4 RayMulAdd;
            vector4 Shape;
            vector4 StartColor;
            vector4 EndColor;
            vector4 DepthInfo;
            vector4 FadeParams;
            matrix4 LocalToShadow;
            vector4 ShadowRect;
            vector4 ShadowInfo;
        } Uniforms;

        VolumetricFeature*   pOwner;
        RenderResourceHandle Depth;
        RenderResourceHandle Mask;
        RenderResourceHandle Shadow;
        xbool                Multisample;
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    static xbool Execute( RenderPassContext& Context, PassData const* pData );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    shader          m_PixelShader;
    shader          m_MultisampleShader;
    rstate_sampler  m_PointSampler;
    rstate_sampler  m_LinearSampler;

    // Sized once per scope before storing pass pointers; retained through execution.
    xarray<PassData> m_Passes;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
char const* VolumetricFeature::GetName( void ) const
{
    return "Volumetric";
}

//=========================================================================

inline
u32 VolumetricFeature::GetGraphStageMask( void ) const
{
    return RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_FOG );
}

//=========================================================================

inline
u32 VolumetricFeature::GetSceneTextureRequirements( void ) const
{
    return SCENE_TEXTURE_DEPTH | SCENE_TEXTURE_DEPTH_SAMPLES;
}

//=========================================================================

inline
xbool VolumetricFeature::Declare( FrameGraphBuilder&, RenderFrameContext const& )
{
    return TRUE;
}

//=========================================================================

inline
xbool VolumetricFeature::Build( FrameGraphBuilder&, RenderFrameContext const& )
{
    return TRUE;
}

} // namespace render

//=========================================================================
#endif // VOLUMETRIC_FEATURE_HPP
//=========================================================================
