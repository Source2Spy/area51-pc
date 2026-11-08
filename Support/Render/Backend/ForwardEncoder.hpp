//=========================================================================
//
//  ForwardEncoder.hpp
//
//=========================================================================

#ifndef FORWARD_ENCODER_HPP
#define FORWARD_ENCODER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../FrameGraph/FrameGraph.hpp"
#include "RenderBackendContext.hpp"
#include "x_array.hpp"
#include "e_Engine.hpp"

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class FrameCompiler;
class PrimitiveStorage;
class DecalEncoder;
class BloomEffect;
class ShadowRenderer;

//=========================================================================
//  TYPES
//=========================================================================

enum ForwardRenderPhase
{
    FORWARD_PHASE_SURFACE = 0,
    FORWARD_PHASE_DECAL,
    FORWARD_PHASE_TRANSPARENT,
    FORWARD_PHASE_ADDITIVE,
    FORWARD_PHASE_FORCE_LAST,
    FORWARD_PHASE_ZPRIME,
    FORWARD_PHASE_FADING,
    FORWARD_PHASE_DISTORTION,
    FORWARD_PHASE_FIRST_PERSON,
    FORWARD_PHASE_COUNT
};

//=========================================================================

class ForwardEncoder
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    ForwardEncoder( void );

    xbool Init( FrameCompiler&    Geometry,
                PrimitiveStorage& Primitives,
                DecalEncoder&     Decals,
                BloomEffect&      Bloom,
                ShadowRenderer&   Shadows );
    void  Kill( void );
    void  BeginFrame( void );
    void  DiscardQueuedDraws( void );

    xbool QueueGeom( s32                    PacketIndex,
                     ForwardRenderPhase     Phase,
                     f32                    SortDepth,
                     u32                    Sequence,
                     RenderContributionMask Contributions = RENDER_CONTRIBUTION_SCENE_COLOR );
    xbool BuildDepthPass( const RenderFrameContext& Frame );
    xbool BuildFirstPersonDepthPass( RenderFrameContext& Frame );
    xbool BuildOpaquePass( const RenderFrameContext& Frame );
    xbool BuildPreFogPass( const RenderFrameContext& Frame );
    xbool BuildPostFogPass( const RenderFrameContext& Frame );
    xbool BuildFirstPersonPass( const RenderFrameContext& Frame );

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum SubmissionState
    {
        SUBMISSION_COLLECTING,
        SUBMISSION_SEALED,
        SUBMISSION_DECLARED,
    };

    //-------------------------------------------------------------------------

    enum class DrawSource
    {
        Geom,
        Decal,
        Primitive
    };

    //-------------------------------------------------------------------------

    struct DrawItem
    {
        DrawItem( void );

        //-------------------------------------------------------------------------

        DrawSource             m_Source;
        ForwardRenderPhase     m_Phase;
        ForwardRenderPhase     m_SortPhase;
        f32                    m_SortDepth;
        u32                    m_Sequence;
        s32                    m_iPacketIndex;
        u32                    m_FirstCommand = 0;
        u32                    m_nCommands    = 0;
        RenderContributionMask m_Contributions;
    };

    //-------------------------------------------------------------------------

    struct SubmissionBatch
    {
        ForwardEncoder*      pOwner;
        ForwardRenderPhase   FirstPhase;
        ForwardRenderPhase   LastPhase;
        RenderResourceHandle DistortionScene;
        render_fog_mode      SceneFogMode;
        render_fog_mode      BloomFogMode;
        xbool                WriteSceneDepth;
        s32                  FirstItem;
        s32                  ItemCount;

        SubmissionBatch( void );
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    static s32 CompareItems( const void* pA, const void* pB );

    xbool            QueueDecals( void );
    xbool            QueuePrimitives( void );
    xbool            SealSubmission( const RenderFrameContext& Frame );
    void             Sort( void );
    xbool            BuildDrawBatches( const RenderFrameContext& Frame );
    xbool BuildOrderedPasses( const RenderFrameContext& Frame,
                              const char*               pName,
                              ForwardRenderPhase        FirstPhase,
                              ForwardRenderPhase        LastPhase );
    xbool BuildPhasePass( const RenderFrameContext& Frame,
                          const char*               pName,
                          ForwardRenderPhase        FirstPhase,
                          ForwardRenderPhase        LastPhase,
                          s32                       FirstItem,
                          s32                       ItemCount,
                          xbool                     WriteSceneDepth = FALSE );
    xbool            CaptureDistortionScene( const RenderFrameContext& Frame );
    static xbool     ExecuteDepthPass( RenderPassContext& Context, ForwardEncoder* pBuilder );
    static xbool     ExecuteFirstPersonDepthPass( RenderPassContext& Context, ForwardEncoder* pBuilder );
    static xbool     ExecuteOpaquePass( RenderPassContext& Context, ForwardEncoder* pBuilder );
    static xbool     ExecuteBatchPass( RenderPassContext& Context, SubmissionBatch* pBatch );
    static xbool     CaptureDistortionPass( RenderPassContext& Context, ForwardEncoder* pBuilder );
    xbool            IsSubtractiveItem( const DrawItem& Item ) const;
    xbool            ItemReadsSceneColor( const DrawItem& Item ) const;
    xbool ExecuteItem( const DrawItem&          Item,
                       const rtarget&           Color,
                       const rtarget*           pDepth,
                       const RenderView*        pView,
                       render_fog_mode          SceneFogMode,
                       render_fog_mode          BloomFogMode,
                       xbool                    WriteSceneDepth,
                       const BackendPassInputs& Inputs );
    void             Clear( void );
    SubmissionBatch* AcquireBatch( void );

    FrameCompiler&    Geom( void ) const;
    DecalEncoder&     Decals( void ) const;
    PrimitiveStorage& Primitives( void ) const;
    BloomEffect&      Bloom( void ) const;
    ShadowRenderer&   Shadows( void ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<DrawItem>         m_Items;
    xarray<s32>              m_BatchItemIndices;
    xarray<SubmissionBatch*> m_Batches;
    s32                      m_nBatches;
    u32                      m_NextSequence;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    SubmissionState           m_SubmissionState;
    const RenderFrameContext* m_pFrame;

    RenderResourceHandle m_DistortionScene;

    FrameCompiler*    m_pGeom;
    DecalEncoder*     m_pDecals;
    PrimitiveStorage* m_pPrimitives;
    BloomEffect*      m_pBloom;
    ShadowRenderer*   m_pShadows;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
ForwardEncoder::DrawItem::DrawItem( void )
    : m_Source( DrawSource::Geom ),
      m_Phase( FORWARD_PHASE_TRANSPARENT ),
      m_SortPhase( FORWARD_PHASE_TRANSPARENT ),
      m_SortDepth( 0.0f ),
      m_Sequence( 0 ),
      m_iPacketIndex( -1 ),
      m_Contributions( RENDER_CONTRIBUTION_SCENE_COLOR )
{
}

//=========================================================================

inline
ForwardEncoder::SubmissionBatch::SubmissionBatch( void )
    : pOwner( NULL ),
      FirstPhase( FORWARD_PHASE_SURFACE ),
      LastPhase( FORWARD_PHASE_SURFACE ),
      DistortionScene(),
      SceneFogMode( RENDER_FOG_NONE ),
      BloomFogMode( RENDER_FOG_NONE ),
      WriteSceneDepth( FALSE ),
      FirstItem( 0 ),
      ItemCount( 0 )
{
}

} // namespace render

#endif // FORWARD_ENCODER_HPP
//=========================================================================
