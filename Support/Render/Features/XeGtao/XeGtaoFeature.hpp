//=========================================================================
//
//  XeGtaoFeature.hpp
//
//=========================================================================

#ifndef XE_GTAO_FEATURE_HPP
#define XE_GTAO_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../AmbientOcclusion.hpp"
#include "../../FrameGraph/FrameGraph.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class XeGtaoFeature : public IRenderFeature
{
  public:
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    XeGtaoFeature( void );

    virtual const char* GetName( void ) const;
    virtual u32         GetGraphStageMask( void ) const;
    virtual u32         GetSceneTextureRequirements( void ) const;
    virtual xbool       Init( void );
    virtual void        Kill( void );
    virtual xbool       Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool       Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );

    void                    SetQuality( AmbientOcclusionQuality Quality );
    AmbientOcclusionQuality GetQuality( void ) const;
    void                    SetRadius( f32 Radius );
    void                    SetStrength( f32 Strength );

  private:
    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum Kernel
    {
        KERNEL_PREPARE,
        KERNEL_PREPARE_MSAA,
        KERNEL_PREPARE_HALF,
        KERNEL_PREPARE_HALF_MSAA,
        KERNEL_MAIN_MEDIUM,
        KERNEL_MAIN_HIGH,
        KERNEL_MAIN_ULTRA,
        KERNEL_MAIN_MEDIUM_MSAA,
        KERNEL_MAIN_HIGH_MSAA,
        KERNEL_MAIN_ULTRA_MSAA,
        KERNEL_DENOISE,
        KERNEL_DENOISE_FINAL,
        KERNEL_RESOLVE,
        KERNEL_COUNT
    };

    struct PassData;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    static xbool Execute( RenderPassContext& Context, const PassData& Data );
    xbool BuildVisibility( FrameGraphBuilder& Graph, const RenderFrameContext& Frame,
                            RenderResourceHandle Source, f32 Radius, xbool FirstPerson,
                            RenderResourceHandle& Visibility );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    shader                  m_Shaders[KERNEL_COUNT];
    shader_compute_desc     m_Compute[KERNEL_COUNT];
    rstate_sampler          m_PointSampler;
    AmbientOcclusionQuality m_Quality;
    f32                     m_Radius;
    f32                     m_Strength;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline const char* XeGtaoFeature::GetName( void ) const
{
    return "XeGTAO";
}

//=========================================================================

inline u32 XeGtaoFeature::GetGraphStageMask( void ) const
{
    return RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_AMBIENT_OCCLUSION ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_FIRST_PERSON_AMBIENT_OCCLUSION );
}

//=========================================================================

inline u32 XeGtaoFeature::GetSceneTextureRequirements( void ) const
{
    return m_Quality == AmbientOcclusionQuality::Disabled ? SCENE_TEXTURE_NONE
                                                         : SCENE_TEXTURE_DEPTH | SCENE_TEXTURE_DEPTH_SAMPLES |
                                                           SCENE_TEXTURE_NORMALS;
}

//=========================================================================

inline xbool XeGtaoFeature::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    (void)Graph;
    (void)Frame;
    return TRUE;
}

//=========================================================================

inline xbool XeGtaoFeature::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    (void)Graph;
    (void)Frame;
    return TRUE;
}

//=========================================================================

inline void XeGtaoFeature::SetQuality( AmbientOcclusionQuality Quality )
{
    ASSERT( Quality >= AmbientOcclusionQuality::Disabled && Quality <= AmbientOcclusionQuality::Ultra );
    m_Quality = Quality;
}

//=========================================================================

inline AmbientOcclusionQuality XeGtaoFeature::GetQuality( void ) const
{
    return m_Quality;
}

//=========================================================================

inline void XeGtaoFeature::SetRadius( f32 Radius )
{
    ASSERT( Radius > 0.0f );
    m_Radius = Radius;
}

//=========================================================================

inline void XeGtaoFeature::SetStrength( f32 Strength )
{
    ASSERT( Strength >= 0.0f && Strength <= 1.0f );
    m_Strength = Strength;
}

} // namespace render

#endif // XE_GTAO_FEATURE_HPP
