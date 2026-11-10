//=========================================================================
//
//  ForwardEncoder.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ForwardEncoder.hpp"

#include "Decal/DecalEncoder.hpp"
#include "Frame/FrameCompiler.hpp"
#include "Primitive/PrimitiveStorage.hpp"
#include "Primitive/PrimitiveEncoder.hpp"
#include "../Features/Shadow/ShadowRenderer.hpp"
#include "../Features/Bloom/BloomEffect.hpp"
#include "../FrameGraph/RenderFailure.hpp"

#include "e_Composite.hpp"
#include "x_debug.hpp"
#include "x_profile.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

ForwardEncoder::ForwardEncoder( void )
    : m_Items(),
      m_BatchItemIndices(),
      m_Batches(),
      m_nBatches( 0 ),
      m_NextSequence( 0 ),
      m_SubmissionState( SUBMISSION_COLLECTING ),
      m_pFrame( NULL ),
      m_DistortionScene(),
      m_pGeom( NULL ),
      m_pDecals( NULL ),
      m_pPrimitives( NULL ),
      m_pBloom( NULL ),
      m_pShadows( NULL )
{
    m_Items.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_BatchItemIndices.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
}

//=========================================================================

xbool ForwardEncoder::Init( FrameCompiler&    Geometry,
                            PrimitiveStorage& Primitives,
                            DecalEncoder&     Decals,
                            BloomEffect&      Bloom,
                            ShadowRenderer&   Shadows )
{
    ASSERT( !m_pGeom && !m_pDecals && !m_pPrimitives );
    m_pGeom       = &Geometry;
    m_pDecals     = &Decals;
    m_pPrimitives = &Primitives;
    m_pBloom      = &Bloom;
    m_pShadows    = &Shadows;

    Clear();

    return TRUE;
}

//=========================================================================

void ForwardEncoder::Kill( void )
{
    Clear();
    m_Items.Clear();
    m_BatchItemIndices.Clear();

    for( s32 i = 0; i < m_Batches.GetCount(); ++i )
    {
        delete m_Batches[i];
    }

    m_Batches.Clear();

    m_pGeom       = NULL;
    m_pDecals     = NULL;
    m_pPrimitives = NULL;
    m_pBloom      = NULL;
    m_pShadows    = NULL;
}

//=========================================================================

void ForwardEncoder::BeginFrame( void )
{
    ASSERTS( m_Items.GetCount() == 0, "Forward draw queue survived the previous frame" );
    ASSERTS( m_SubmissionState == SUBMISSION_COLLECTING, "Sealed forward submission survived the previous frame" );
    Clear();
}

//=========================================================================

void ForwardEncoder::DiscardQueuedDraws( void )
{
    Geom().ClearPackets();
    Decals().DiscardQueuedDraws();
    Primitives().DiscardQueuedDraws();
    Clear();
}

//=========================================================================

xbool ForwardEncoder::QueueGeom( s32                    PacketIndex,
                                 ForwardRenderPhase     Phase,
                                 f32                    SortDepth,
                                 u32                    Sequence,
                                 RenderContributionMask Contributions )
{
    if( ( m_SubmissionState != SUBMISSION_COLLECTING ) || ( PacketIndex < 0 ) || ( Phase < FORWARD_PHASE_SURFACE ) ||
        ( Phase >= FORWARD_PHASE_COUNT ) || !x_isvalid( SortDepth ) || ( Contributions == 0 ) ||
        ( Contributions & ~( RENDER_CONTRIBUTION_SCENE_COLOR | RENDER_CONTRIBUTION_BLOOM ) ) )
    {
        RENDER_THROW( "ForwardEncoder", "rejected geometry queue item packet=%d phase=%d state=%d", PacketIndex,
                      static_cast<s32>( Phase ), m_SubmissionState );
        return FALSE;
    }

    if( m_Items.GetCount() >= m_Items.GetCapacity() )
    {
        RENDER_THROW( "ForwardEncoder", "forward draw item capacity exhausted" );
        return FALSE;
    }

    DrawItem& Item       = m_Items.Append();
    Item                 = DrawItem();
    Item.m_Source        = DrawSource::Geom;
    Item.m_SortPhase     = Geom().PacketReadsSceneColor( PacketIndex ) ? FORWARD_PHASE_DISTORTION : Phase;
    Item.m_Phase         = Geom().IsFirstPersonPacket( PacketIndex ) ? FORWARD_PHASE_FIRST_PERSON : Item.m_SortPhase;
    Item.m_SortDepth     = SortDepth;
    Item.m_Sequence      = Sequence;
    Item.m_iPacketIndex  = PacketIndex;
    Item.m_Contributions = Contributions;
    m_NextSequence       = MAX( m_NextSequence, Sequence + 1 );

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::QueueDecals( void )
{
    DecalEncoder& DecalDraws = Decals();

    if( DecalDraws.GetQueuedDrawCount() > ( m_Items.GetCapacity() - m_Items.GetCount() ) )
    {
        RENDER_THROW( "ForwardEncoder", "forward draw item capacity exhausted by decals" );
        return FALSE;
    }

    for( s32 i = 0; i < DecalDraws.GetQueuedDrawCount(); ++i )
    {
        DrawItem& Item       = m_Items.Append();
        Item                 = DrawItem();
        Item.m_Source        = DrawSource::Decal;
        Item.m_Phase         = FORWARD_PHASE_DECAL;
        Item.m_SortDepth     = 0.0f;
        Item.m_Sequence      = m_NextSequence++;
        Item.m_iPacketIndex  = i;
        Item.m_Contributions = RENDER_CONTRIBUTION_SCENE_COLOR | ( DecalDraws.IsQueuedDrawGlow( i ) ? RENDER_CONTRIBUTION_BLOOM : 0 );
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::QueuePrimitives( void )
{
    PrimitiveStorage& PrimitiveDraws = Primitives();
    const u64         RequiredCount  = static_cast<u64>( m_Items.GetCount() ) + PrimitiveDraws.GetQueuedDrawCount();

    if( RequiredCount > FrameStorage::MAX_STREAM_BYTES / sizeof( DrawItem ) )
    {
        return FALSE;
    }

    if( RequiredCount > static_cast<u64>( m_Items.GetCapacity() ) )
    {
        u32 Capacity = MAX( static_cast<u32>( m_Items.GetCapacity() ), 1u );

        while( Capacity < RequiredCount )
        {
            Capacity *= 2;
        }

        Capacity = MIN( Capacity, FrameStorage::MAX_STREAM_BYTES / static_cast<u32>( sizeof( DrawItem ) ) );
        m_Items.SetCapacity( Capacity );
        m_BatchItemIndices.SetCapacity( Capacity );
    }

    for( s32 i = 0; i < PrimitiveDraws.GetQueuedDrawCount(); ++i )
    {
        ForwardRenderPhase Phase;

        if( PrimitiveDraws.QueuedDrawReadsSceneColor( i ) )
        {
            Phase = FORWARD_PHASE_DISTORTION;
        }
        else
        {
            switch( PrimitiveDraws.GetQueuedDrawBlend( i ) )
            {
                case render::PRIMITIVE_BLEND_OPAQUE:
                {
                    Phase = FORWARD_PHASE_SURFACE;
                    break;
                }
                case render::PRIMITIVE_BLEND_ALPHA:
                case render::PRIMITIVE_BLEND_INTENSITY:
                {
                    Phase = FORWARD_PHASE_TRANSPARENT;
                    break;
                }
                case render::PRIMITIVE_BLEND_ADDITIVE:
                case render::PRIMITIVE_BLEND_SUBTRACTIVE:
                {
                    Phase = FORWARD_PHASE_ADDITIVE;
                    break;
                }
                default:
                {
                    RENDER_THROW( "ForwardEncoder", "rejected primitive queue item with invalid blend=%d",
                                  static_cast<s32>( PrimitiveDraws.GetQueuedDrawBlend( i ) ) );

                    return FALSE;
                }
            }
        }

        DrawItem& Item       = m_Items.Append();
        Item                 = DrawItem();
        Item.m_Source        = DrawSource::Primitive;
        Item.m_Phase         = Phase;
        Item.m_SortDepth     = PrimitiveDraws.GetQueuedDrawSortDepth( i );
        Item.m_Sequence      = m_NextSequence++;
        Item.m_iPacketIndex  = i;
        Item.m_Contributions = RENDER_CONTRIBUTION_SCENE_COLOR |  ( PrimitiveDraws.GetQueuedDrawOutput( i ) == render::PRIMITIVE_OUTPUT_GLOW ? RENDER_CONTRIBUTION_BLOOM
                                                                                       : 0 );
    }

    return TRUE;
}

//=========================================================================
//  SORTING AND BATCHING
//=========================================================================

static s32 GetForwardSortGroup( ForwardRenderPhase Phase )
{
    switch( Phase )
    {
        case FORWARD_PHASE_SURFACE:
        {
            return 0;
        }
        case FORWARD_PHASE_DECAL:
        {
            return 1;
        }
        case FORWARD_PHASE_TRANSPARENT:
        case FORWARD_PHASE_ADDITIVE:
        case FORWARD_PHASE_FADING:
        case FORWARD_PHASE_DISTORTION:
        {
            return 2;
        }
        case FORWARD_PHASE_FORCE_LAST:
        {
            return 3;
        }
        case FORWARD_PHASE_ZPRIME:
        {
            return 4;
        }
        default:
        {
            return 5;
        }
    }
}

//=========================================================================

s32 ForwardEncoder::CompareItems( const void* pA, const void* pB )
{
    const DrawItem& A      = *static_cast<const DrawItem*>( pA );
    const DrawItem& B      = *static_cast<const DrawItem*>( pB );
    const s32       AGroup = GetForwardSortGroup( A.m_Phase == FORWARD_PHASE_FIRST_PERSON ? A.m_SortPhase : A.m_Phase );
    const s32       BGroup = GetForwardSortGroup( B.m_Phase == FORWARD_PHASE_FIRST_PERSON ? B.m_SortPhase : B.m_Phase );

    if( ( A.m_Phase == FORWARD_PHASE_FIRST_PERSON ) != ( B.m_Phase == FORWARD_PHASE_FIRST_PERSON ) )
    {
        return A.m_Phase == FORWARD_PHASE_FIRST_PERSON ? 1 : -1;
    }

    if( AGroup < BGroup )
    {
        return -1;
    }

    if( AGroup > BGroup )
    {
        return 1;
    }

    if( ( AGroup == 2 ) || ( AGroup == 3 ) )
    {
        if( A.m_SortDepth > B.m_SortDepth )
        {
            return -1;
        }

        if( A.m_SortDepth < B.m_SortDepth )
        {
            return 1;
        }
    }

    if( A.m_Sequence < B.m_Sequence )
    {
        return -1;
    }

    if( A.m_Sequence > B.m_Sequence )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

void ForwardEncoder::Sort( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Forward/Sort" );

    if( m_Items.GetCount() > 1 )
    {
        x_qsort( m_Items.GetPtr(), m_Items.GetCount(), sizeof( DrawItem ), CompareItems );
    }
}

//=========================================================================

xbool ForwardEncoder::BuildDrawBatches( const RenderFrameContext& Frame )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Forward/BuildDrawBatches" );

    Primitives().BeginSpriteBatches();
    Geom().BeginForwardBatches();

    s32 Count = 0;

    for( s32 i = 0; i < m_Items.GetCount(); ++i )
    {
        DrawItem    Item      = m_Items[i];
        DrawItem*   pPrevious = Count ? &m_Items[Count - 1] : NULL;

        // Refraction needs a fresh background between draws, including adjacent sprites.
        const xbool SamePhase = pPrevious && !ItemReadsSceneColor( Item ) &&
                                pPrevious->m_Source == Item.m_Source &&
                                pPrevious->m_Phase == Item.m_Phase && pPrevious->m_SortPhase == Item.m_SortPhase &&
                                pPrevious->m_Contributions == Item.m_Contributions;

        if( Item.m_Source == DrawSource::Primitive && Primitives().IsSpriteDraw( Item.m_iPacketIndex ) )
        {
            const s32 Batch =
                Primitives().AppendSpriteBatch( Item.m_iPacketIndex, SamePhase ? pPrevious->m_iPacketIndex : -1 );

            if( Batch < 0 )
            {
                return FALSE;
            }

            if( Batch != Item.m_iPacketIndex )
            {
                continue;
            }
        }
        else if( SamePhase && Item.m_Source == DrawSource::Geom &&
                 Geom().CanBatchForwardPackets( pPrevious->m_iPacketIndex, Item.m_iPacketIndex ) )
        {
            if( !pPrevious->m_nCommands )
            {
                const s32 First = Geom().AppendForwardCommands( pPrevious->m_iPacketIndex );

                if( First < 0 )
                {
                    return FALSE;
                }

                pPrevious->m_FirstCommand = First;
            }

            if( Geom().AppendForwardCommands( Item.m_iPacketIndex ) < 0 )
            {
                return FALSE;
            }

            pPrevious->m_nCommands = Geom().GetForwardCommandCount() - pPrevious->m_FirstCommand;
            continue;
        }

        m_Items[Count++] = Item;
    }

    m_Items.SetCount( Count );

    return Primitives().UploadSpriteBatches( *Frame.pGraph ) && Geom().UploadForwardCommands( *Frame.pGraph );
}

//=========================================================================
//  DRAW EXECUTION
//=========================================================================

xbool ForwardEncoder::ItemReadsSceneColor( const DrawItem& Item ) const
{
    return Item.m_Phase == FORWARD_PHASE_DISTORTION ||
           ( Item.m_Phase == FORWARD_PHASE_FIRST_PERSON && Geom().PacketReadsSceneColor( Item.m_iPacketIndex ) );
}

//=========================================================================

xbool ForwardEncoder::IsSubtractiveItem( const DrawItem& Item ) const
{
    if( Item.m_Source == DrawSource::Decal )
    {
        return Decals().GetQueuedDrawBlend( Item.m_iPacketIndex ) == DECAL_BLEND_SUBTRACTIVE;
    }

    if( Item.m_Source == DrawSource::Primitive )
    {
        return Primitives().GetQueuedDrawBlend( Item.m_iPacketIndex ) == PRIMITIVE_BLEND_SUBTRACTIVE;
    }

    return FALSE;
}

//=========================================================================

xbool ForwardEncoder::ExecuteItem( const DrawItem&          Item,
                                   const rtarget&           Color,
                                   const rtarget*           pDepth,
                                   const RenderView*        pView,
                                   render_fog_mode          SceneFogMode,
                                   render_fog_mode          BloomFogMode,
                                   xbool                    WriteSceneDepth,
                                   const BackendPassInputs& Inputs )
{
    if( Item.m_Phase == FORWARD_PHASE_DISTORTION )
    {
        SceneFogMode = RENDER_FOG_NONE;
    }

    if( Item.m_Source == DrawSource::Geom )
    {
        geom_pass_desc PassData;
        PassData.TargetWidth     = Color.Desc.Width;
        PassData.TargetHeight    = Color.Desc.Height;
        PassData.SceneFogMode    = SceneFogMode;
        PassData.BloomFogMode    = BloomFogMode;
        PassData.EnableShadows   = Inputs.EnableShadows;
        PassData.pView           = pView;
        PassData.ColorFormat     = Color.Desc.Format;
        PassData.DepthFormat     = pDepth ? pDepth->Desc.Format : RTARGET_FORMAT_COUNT;
        PassData.SampleCount     = Color.Desc.SampleCount;
        PassData.WriteSceneDepth = WriteSceneDepth;

        if( Item.m_Phase == FORWARD_PHASE_FIRST_PERSON )
        {
            const geom_render_pass PacketPass = Geom().GetPacketPass( Item.m_iPacketIndex );
            PassData.SceneFogMode    = PacketPass != GEOM_PASS_FORWARD_OPAQUE && PacketPass != GEOM_PASS_DISTORTION
                                           ? RENDER_FOG_FORWARD_COLOR
                                           : RENDER_FOG_NONE;
            PassData.WriteSceneDepth = WriteSceneDepth && PacketPass == GEOM_PASS_FORWARD_OPAQUE;
        }

        if( !Geom().ExecutePacket( Item.m_iPacketIndex, PassData, Inputs, Item.m_FirstCommand, Item.m_nCommands ) )
        {
            RENDER_THROW( "ForwardEncoder", "geometry draw failed packet=%d phase=%d pass=%s", Item.m_iPacketIndex,
                          static_cast<s32>( Item.m_Phase ), "scene" );
            return FALSE;
        }

        return TRUE;
    }

    if( Item.m_Source == DrawSource::Decal )
    {
        DecalEncoder::PassDesc PassData;
        PassData.EnableShadows   = Inputs.EnableShadows;
        PassData.ColorFormat     = Color.Desc.Format;
        PassData.DepthFormat     = pDepth ? pDepth->Desc.Format : RTARGET_FORMAT_COUNT;
        PassData.SampleCount     = Color.Desc.SampleCount ? Color.Desc.SampleCount : 1;
        PassData.WriteSceneDepth = WriteSceneDepth;
        PassData.SceneFogMode    = SceneFogMode;
        PassData.BloomFogMode    = BloomFogMode;

        if( !Decals().DrawQueuedDraw( Item.m_iPacketIndex, PassData, Inputs ) )
        {
            RENDER_THROW( "ForwardEncoder", "decal draw failed packet=%d phase=%d pass=%s", Item.m_iPacketIndex,
                          static_cast<s32>( Item.m_Phase ), "scene" );
            return FALSE;
        }

        return TRUE;
    }

    PrimitiveStorage::PassDesc PassData;
    PassData.Width           = Color.Desc.Width;
    PassData.Height          = Color.Desc.Height;
    PassData.ColorFormat     = Color.Desc.Format;
    PassData.DepthFormat     = pDepth ? pDepth->Desc.Format : RTARGET_FORMAT_COUNT;
    PassData.SampleCount     = Color.Desc.SampleCount ? Color.Desc.SampleCount : 1;
    PassData.WriteSceneDepth = WriteSceneDepth;
    PassData.SceneFogMode    = SceneFogMode;
    PassData.BloomFogMode    = BloomFogMode;

    if( !Primitives().DrawQueuedDraw( Item.m_iPacketIndex, PassData, Inputs ) )
    {
        RENDER_THROW( "ForwardEncoder", "primitive draw failed packet=%d phase=%d pass=%s", Item.m_iPacketIndex,
                      static_cast<s32>( Item.m_Phase ), "scene" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  DISTORTION CAPTURE
//=========================================================================

xbool ForwardEncoder::CaptureDistortionScene( const RenderFrameContext& Frame )
{
    const RenderResourceHandle Source      = Frame.GetSceneColorTarget();
    const RenderResourceDesc*  pSceneColor = Frame.pGraph->GetDesc( Source );

    if( !pSceneColor )
    {
        RENDER_THROW( "ForwardEncoder", "distortion capture scene color resource is unavailable" );
        return FALSE;
    }

    const xbool Multisample = pSceneColor->SampleCount > 1;

    if( !m_DistortionScene.IsValid() )
    {
        RenderResourceDesc ResourceData;
        ResourceData.Extent      = pSceneColor->Extent;
        ResourceData.Format      = pSceneColor->Format;
        ResourceData.SampleCount = 1;
        ResourceData.Usage       = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COPY_DST;
        ResourceData.ViewMask    = 1;
        ResourceData.pDebugName  = "DistortionScene";
        m_DistortionScene        = Frame.pGraph->CreateResource( ResourceData );
    }

    RenderPassDesc PassDesc;
    PassDesc.pDebugName = "CaptureDistortionScene";

    if( Multisample )
    {
        // Distortion reads the resolved scene and continues rendering into the MSAA attachment.
        PassDesc.Color        = Source;
        PassDesc.ColorResolve = m_DistortionScene;
        PassDesc.ColorLoadOp  = RTARGET_LOAD_LOAD;
        PassDesc.ColorStoreOp = RTARGET_STORE_RESOLVE_AND_STORE;
    }
    else
    {
        PassDesc.Type        = RENDER_PASS_COPY;
        PassDesc.Source      = Source;
        PassDesc.Destination = m_DistortionScene;
    }

    const RenderPassId PassData = Frame.pGraph->AddPass( PassDesc, CaptureDistortionPass, this );

    if( !m_DistortionScene.IsValid() || ( PassData == INVALID_RENDER_PASS ) )
    {
        RENDER_THROW( "ForwardEncoder", "failed to build distortion capture pass" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::CaptureDistortionPass( RenderPassContext& Context, ForwardEncoder* pBuilder )
{
    if( !pBuilder )
    {
        RENDER_THROW( "ForwardEncoder", "distortion scene capture copy failed" );
        return FALSE;
    }

    if( Context.GetPass().ColorResolve.IsValid() )
    {
        return Context.Write( Context.GetPass().Color ) && Context.Write( Context.GetPass().ColorResolve );
    }

    if( !Context.Copy( Context.GetPass().Destination, Context.GetPass().Source ) )
    {
        RENDER_THROW( "ForwardEncoder", "distortion scene capture copy failed" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  GRAPH DECLARATION
//=========================================================================

xbool ForwardEncoder::SealSubmission( const RenderFrameContext& Frame )
{
    if( m_SubmissionState == SUBMISSION_SEALED )
    {
        return m_pFrame == &Frame;
    }

    if( m_SubmissionState != SUBMISSION_COLLECTING )
    {
        RENDER_THROW( "ForwardEncoder", "submission cannot be sealed after pass declaration" );
        return FALSE;
    }

    const RenderResourceDesc* pColor = Frame.pGraph->GetDesc( Frame.GetSceneColorTarget() );
    const RenderResourceDesc* pDepth = Frame.pGraph->GetDesc( Frame.GetSceneDepthTarget() );

    if( !pColor || !pDepth || rtarget_IsDepthFormat( pColor->Format ) || !rtarget_IsDepthFormat( pDepth->Format ) )
    {
        RENDER_THROW( "ForwardEncoder", "forward queue has invalid scene target descriptors" );
        DiscardQueuedDraws();

        return FALSE;
    }

    PrimitiveStorage& PrimitiveDraws = Primitives();

    if( ( Decals().GetQueuedDrawCount() > 0 ) && !QueueDecals() )
    {
        DiscardQueuedDraws();
        return FALSE;
    }

    const xbool HasPrimitives = PrimitiveDraws.HasQueuedDraws();

    if( HasPrimitives )
    {
        if( !QueuePrimitives() )
        {
            DiscardQueuedDraws();
            return FALSE;
        }
    }

    Sort();

    if( !BuildDrawBatches( Frame ) )
    {
        return FALSE;
    }

    m_SubmissionState = SUBMISSION_SEALED;
    m_pFrame          = &Frame;

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::BuildOrderedPasses( const RenderFrameContext& Frame,
                                          const char*               pName,
                                          ForwardRenderPhase        FirstPhase,
                                          ForwardRenderPhase        LastPhase )
{
    const char* pRefractionName = FirstPhase == FORWARD_PHASE_FIRST_PERSON
                                     ? "FirstPersonRefractive" : "ForwardRefractiveComposite";
    s32 FirstItem = 0;

    for( s32 i = 0; i < m_Items.GetCount(); ++i )
    {
        const DrawItem& Item = m_Items[i];

        if( Item.m_Phase < FirstPhase || Item.m_Phase > LastPhase ||
            !( Item.m_Contributions & RENDER_CONTRIBUTION_SCENE_COLOR ) ||
            !Frame.Desc.EnablePostProcessing || !ItemReadsSceneColor( Item ) )
        {
            continue;
        }

        // Capture only the background already drawn in back-to-front order.
        // Refresh even for adjacent refractive draws so they can see each other.
        if( !BuildPhasePass( Frame, pName, FirstPhase, LastPhase, FirstItem, i - FirstItem ) ||
            !CaptureDistortionScene( Frame ) ||
            !BuildPhasePass( Frame, pRefractionName, FirstPhase, LastPhase, i, 1 ) )
        {
            return FALSE;
        }

        FirstItem = i + 1;
    }

    return BuildPhasePass( Frame, pName, FirstPhase, LastPhase, FirstItem, m_Items.GetCount() - FirstItem );
}

//=========================================================================

xbool ForwardEncoder::BuildPhasePass( const RenderFrameContext& Frame,
                                      const char*               pName,
                                      ForwardRenderPhase        FirstPhase,
                                      ForwardRenderPhase        LastPhase,
                                      s32                       FirstItem,
                                      s32                       ItemCount,
                                      xbool                     WriteSceneDepth )
{
    if( FirstItem < 0 || ItemCount < 0 || FirstItem > m_Items.GetCount() ||
        ItemCount > m_Items.GetCount() - FirstItem )
    {
        RENDER_THROW( "ForwardEncoder", "invalid forward pass item range '%s'", pName );
        return FALSE;
    }

    SubmissionBatch* pBatch = AcquireBatch();

    pBatch->pOwner          = this;
    pBatch->FirstPhase      = FirstPhase;
    pBatch->LastPhase       = LastPhase;
    pBatch->DistortionScene = RenderResourceHandle();

    switch( FirstPhase )
    {
        case FORWARD_PHASE_SURFACE:
        case FORWARD_PHASE_DECAL:
        case FORWARD_PHASE_FIRST_PERSON:
        {
            pBatch->SceneFogMode = RENDER_FOG_NONE;
            break;
        }
        case FORWARD_PHASE_TRANSPARENT:
        case FORWARD_PHASE_ADDITIVE:
        case FORWARD_PHASE_FORCE_LAST:
        case FORWARD_PHASE_ZPRIME:
        case FORWARD_PHASE_FADING:
        {
            pBatch->SceneFogMode = RENDER_FOG_FORWARD_COLOR;
            break;
        }
        case FORWARD_PHASE_DISTORTION:
        {
            pBatch->SceneFogMode = RENDER_FOG_NONE;
            break;
        }
        default:
        {
            RENDER_THROW( "ForwardEncoder", "unsupported forward fog policy phase=%d", static_cast<s32>( FirstPhase ) );
            m_nBatches--;

            return FALSE;
        }
    }

    pBatch->BloomFogMode    = Bloom().IsEnabled() ? RENDER_FOG_FORWARD_EMISSION : RENDER_FOG_NONE;
    pBatch->WriteSceneDepth = WriteSceneDepth;
    pBatch->FirstItem       = m_BatchItemIndices.GetCount();
    pBatch->ItemCount       = 0;

    for( s32 i = FirstItem; i < FirstItem + ItemCount; ++i )
    {
        const DrawItem& Item = m_Items[i];

        if( ( Item.m_Contributions & RENDER_CONTRIBUTION_SCENE_COLOR ) && Item.m_Phase >= FirstPhase &&
            Item.m_Phase <= LastPhase )
        {
            if( FirstPhase == FORWARD_PHASE_FIRST_PERSON &&
                ( Geom().GetPacketPass( Item.m_iPacketIndex ) == GEOM_PASS_FORWARD_OPAQUE ) != WriteSceneDepth )
            {
                continue;
            }

            const xbool ReadsScene = ItemReadsSceneColor( Item );
            if( ReadsScene )
            {
                if( !Frame.Desc.EnablePostProcessing )
                {
                    continue;
                }
                if( !m_DistortionScene.IsValid() )
                {
                    RENDER_THROW( "ForwardEncoder", "distortion scene is unavailable for '%s'", pName );
                    return FALSE;
                }
                pBatch->DistortionScene = m_DistortionScene;
            }

            if( Item.m_Source != DrawSource::Geom )
            {
                const RenderResourceDesc* pColor = Frame.pGraph->GetDesc( Frame.GetSceneColorTarget() );
                const RenderResourceDesc* pDepth = Frame.pGraph->GetDesc( Frame.GetSceneDepthTarget() );

                if( !pColor || !pDepth )
                {
                    return FALSE;
                }

                if( Item.m_Source == DrawSource::Primitive )
                {
                    PrimitiveStorage::PassDesc Pass;
                    Pass.ColorFormat     = pColor->Format;
                    Pass.DepthFormat     = pDepth->Format;
                    Pass.SampleCount     = pColor->SampleCount;
                    Pass.WriteSceneDepth = WriteSceneDepth;
                    Pass.SceneFogMode    = ReadsScene ? RENDER_FOG_NONE : pBatch->SceneFogMode;
                    Pass.BloomFogMode    = pBatch->BloomFogMode;

                    if( !Primitives().PrepareQueuedDraw( Item.m_iPacketIndex, Pass ) )
                    {
                        return FALSE;
                    }
                }
                else
                {
                    DecalEncoder::PassDesc Pass;
                    Pass.ColorFormat     = pColor->Format;
                    Pass.DepthFormat     = pDepth->Format;
                    Pass.SampleCount     = pColor->SampleCount;
                    Pass.WriteSceneDepth = WriteSceneDepth;
                    Pass.SceneFogMode    = ReadsScene ? RENDER_FOG_NONE : pBatch->SceneFogMode;
                    Pass.BloomFogMode    = pBatch->BloomFogMode;
                    Pass.EnableShadows   = Frame.Desc.EnableShadows;

                    if( !Decals().PrepareQueuedDraw( Item.m_iPacketIndex, Pass ) )
                    {
                        return FALSE;
                    }
                }
            }

            m_BatchItemIndices.Append() = i;
            pBatch->ItemCount++;
        }
    }

    if( pBatch->ItemCount == 0 )
    {
        m_BatchItemIndices.SetCount( pBatch->FirstItem );
        m_nBatches--;
        return TRUE;
    }

    RenderPassDesc DescData;
    DescData.pDebugName = pName;
    DescData.Color      = Frame.GetSceneColorTarget();

    if( WriteSceneDepth )
    {
        DescData.Color1 = Frame.GetSceneLinearDepthTarget();
    }

    DescData.Depth       = Frame.GetSceneDepthTarget();
    DescData.ColorLoadOp = RTARGET_LOAD_LOAD;
    DescData.DepthLoadOp = RTARGET_LOAD_LOAD;

    const RenderPassId PassData = Frame.pGraph->AddPass( DescData, ExecuteBatchPass, pBatch );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "ForwardEncoder", "failed to add forward pass '%s'", pName );
        m_BatchItemIndices.SetCount( pBatch->FirstItem );
        m_nBatches--;

        return FALSE;
    }

    if( pBatch->DistortionScene.IsValid() )
    {
        Frame.pGraph->Read( PassData, pBatch->DistortionScene );
    }

    xbool HasSubtractive = FALSE;

    for( s32 i = 0; i < pBatch->ItemCount; ++i )
    {
        const DrawItem& Item = m_Items[m_BatchItemIndices[pBatch->FirstItem + i]];
        HasSubtractive |= IsSubtractiveItem( Item );

        if( Item.m_Source == DrawSource::Geom )
        {
            Geom().DeclarePacketResources( *Frame.pGraph, PassData, Item.m_iPacketIndex );
        }
        else if( Item.m_Source == DrawSource::Decal )
        {
            Decals().DeclareQueuedResources( *Frame.pGraph, PassData, Item.m_iPacketIndex );
        }
        else
        {
            Primitives().DeclareQueuedResources( *Frame.pGraph, PassData, Item.m_iPacketIndex );
        }
    }

    Geom().DeclarePassResources( *Frame.pGraph, PassData );

    if( FirstPhase == FORWARD_PHASE_FIRST_PERSON && WriteSceneDepth && Frame.FirstPersonAmbientVisibility.IsValid() )
    {
        Frame.pGraph->Read( PassData, Frame.FirstPersonAmbientVisibility );
    }

    const RenderResourceDesc* pColor = Frame.pGraph->GetDesc( Frame.GetSceneColorTarget() );

    if( !pColor || ( HasSubtractive && WriteSceneDepth ) )
    {
        RENDER_THROW( "ForwardEncoder", "invalid subtractive pass targets '%s'", pName );
        return FALSE;
    }

    if( HasSubtractive &&
        ( pColor->Format == RTARGET_FORMAT_RGBA16F || pColor->Format == RTARGET_FORMAT_RGBA32F ) &&
        !Frame.pGraph->DeclareCompositePipeline( PassData, COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB ) )
    {
        return FALSE;
    }

    if( FirstPhase == FORWARD_PHASE_FIRST_PERSON )
    {
        for( s32 i = 0; i < Frame.SceneColorCopies.GetCount(); i++ )
        {
            Frame.pGraph->Read( PassData, Frame.SceneColorCopies[i] );
        }
    }

    Shadows().DeclareConsumer( *Frame.pGraph, PassData );

    return TRUE;
}

//=========================================================================
//  PASS EXECUTION
//=========================================================================

xbool ForwardEncoder::ExecuteBatchPass( RenderPassContext& Context, SubmissionBatch* pBatch )
{
    if( !pBatch || !pBatch->pOwner )
    {
        RENDER_THROW( "ForwardEncoder", "forward pass received an invalid submission batch" );
        return FALSE;
    }

    ForwardEncoder&           Owner  = *pBatch->pOwner;
    const RenderFrameContext& Frame  = Context.GetFrame();
    const rtarget*            pColor = Context.Write( Frame.GetSceneColorTarget() );
    const rtarget*            pDepth = Context.Write( Frame.GetSceneDepthTarget() );

    if( !pColor || !pDepth )
    {
        RENDER_THROW( "ForwardEncoder", "forward pass targets are unavailable" );
        return FALSE;
    }

    const RenderResourceHandle ShadowSample = Owner.Shadows().GetShadowSampleAtlasHandle();
    const RenderResourceHandle ShadowDepth  = Owner.Shadows().GetShadowDepthAtlasHandle();
    BackendPassInputs          Inputs;
    Inputs.EnableShadows = Context.GetFrame().Desc.EnableShadows;
    Inputs.pShadowSample = ShadowSample.IsValid() ? Context.Read( ShadowSample ) : NULL;
    Inputs.pShadowDepth  = ShadowDepth.IsValid() ? Context.Read( ShadowDepth ) : NULL;

    if( pBatch->FirstPhase == FORWARD_PHASE_FIRST_PERSON && pBatch->WriteSceneDepth &&
        Frame.FirstPersonAmbientVisibility.IsValid() )
    {
        Inputs.pAmbientVisibility = Context.Read( Frame.FirstPersonAmbientVisibility );
        if( !Inputs.pAmbientVisibility )
        {
            return FALSE;
        }
    }

    if( pBatch->DistortionScene.IsValid() )
    {
        const rtarget*         pDistortion = Context.Read( pBatch->DistortionScene );
        const shader_resource* pScene      = pDistortion ? rtarget_GetShaderResource( *pDistortion ) : NULL;

        if( !pScene )
        {
            RENDER_THROW( "ForwardEncoder", "distortion pass scene resource is unavailable" );
            return FALSE;
        }

        Inputs.pDistortionScene = pScene;
    }

    if( !Owner.Geom().PreparePassConstants( Context.GetView(), pColor->Desc.Width, pColor->Desc.Height ) )
    {
        return FALSE;
    }

    const xbool ClampSubtractive = pColor->Desc.Format == RTARGET_FORMAT_RGBA16F ||
                                   pColor->Desc.Format == RTARGET_FORMAT_RGBA32F;
    xbool       PendingClamp     = FALSE;
    xbool       Result           = TRUE;

    for( s32 i = 0; Result && i < pBatch->ItemCount; ++i )
    {
        const s32 ItemIndex = pBatch->FirstItem + i;

        if( ( ItemIndex < 0 ) || ( ItemIndex >= Owner.m_BatchItemIndices.GetCount() ) )
        {
            RENDER_THROW( "ForwardEncoder", "forward pass item index is invalid" );
            return FALSE;
        }

        const s32 DrawIndex = Owner.m_BatchItemIndices[ItemIndex];

        if( ( DrawIndex < 0 ) || ( DrawIndex >= Owner.m_Items.GetCount() ) )
        {
            RENDER_THROW( "ForwardEncoder", "forward pass draw index is invalid" );
            return FALSE;
        }

        const DrawItem& Item        = Owner.m_Items[DrawIndex];
        const xbool     Subtractive = ClampSubtractive && Owner.IsSubtractiveItem( Item );

        // Float targets retain negative subtraction, unlike the original UNORM buffer.
        // Clamp each subtractive run before another blend can consume its result.
        if( PendingClamp && !Subtractive )
        {
            if( !composite_ClampNegative() )
            {
                return FALSE;
            }

            PendingClamp = FALSE;
        }

        Result = Owner.ExecuteItem( Item, *pColor, pDepth, Context.GetView(), pBatch->SceneFogMode,
                                    pBatch->BloomFogMode, pBatch->WriteSceneDepth, Inputs );
        PendingClamp = Subtractive;
    }

    return Result && ( !PendingClamp || composite_ClampNegative() );
}

//=========================================================================

xbool ForwardEncoder::ExecuteDepthPass( RenderPassContext& Context, ForwardEncoder* pBuilder )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/DepthPrepass" );

    const RenderFrameContext& Frame = Context.GetFrame();
    const rtarget* pLinearDepth = Context.Write( Frame.GetSceneLinearDepthTarget() );
    const rtarget* pDepth       = Context.Write( Frame.GetSceneDepthTarget() );
    const rtarget* pNormals     = Frame.SceneNormals.IsValid() ? Context.Write( Frame.SceneNormals ) : NULL;

    if( !pBuilder || !pLinearDepth || !pDepth || ( Frame.SceneNormals.IsValid() && !pNormals ) ||
        !pBuilder->Geom().PreparePassConstants( Context.GetView(), pLinearDepth->Desc.Width,
                                               pLinearDepth->Desc.Height ) )
    {
        RENDER_THROW( "ForwardEncoder", "depth prepass targets or view are unavailable" );
        return FALSE;
    }

    geom_pass_desc PassData;
    PassData.TargetWidth   = pLinearDepth->Desc.Width;
    PassData.TargetHeight  = pLinearDepth->Desc.Height;
    PassData.pView         = Context.GetView();
    PassData.ColorFormat   = pLinearDepth->Desc.Format;
    PassData.DepthFormat   = pDepth->Desc.Format;
    PassData.SampleCount   = pDepth->Desc.SampleCount;
    PassData.DepthMode     = GEOM_DEPTH_PREPASS;
    PassData.EnableShadows = FALSE;
    PassData.WriteNormals  = Frame.SceneNormals.IsValid();

    BackendPassInputs Inputs;
    Inputs.EnableShadows = FALSE;
    return pBuilder->Geom().ExecuteForwardOpaque( PassData, Inputs );
}

//=========================================================================

xbool ForwardEncoder::ExecuteFirstPersonDepthPass( RenderPassContext& Context, ForwardEncoder* pBuilder )
{
    const RenderFrameContext& Frame = Context.GetFrame();
    const rtarget* pLinearDepth = Context.Write( Frame.FirstPersonLinearDepth );
    const rtarget* pDepth = Context.Write( Frame.FirstPersonDepth );
    const rtarget* pNormals = Context.Write( Frame.FirstPersonNormals );

    if( !pBuilder || !pLinearDepth || !pDepth || !pNormals ||
        !pBuilder->Geom().PreparePassConstants( Context.GetView(), pLinearDepth->Desc.Width,
                                               pLinearDepth->Desc.Height ) )
    {
        RENDER_THROW( "ForwardEncoder", "first-person depth prepass targets or view are unavailable" );
        return FALSE;
    }

    geom_pass_desc PassData;
    PassData.TargetWidth   = pLinearDepth->Desc.Width;
    PassData.TargetHeight  = pLinearDepth->Desc.Height;
    PassData.pView         = Context.GetView();
    PassData.ColorFormat   = pLinearDepth->Desc.Format;
    PassData.DepthFormat   = pDepth->Desc.Format;
    PassData.SampleCount   = pDepth->Desc.SampleCount;
    PassData.DepthMode     = GEOM_DEPTH_PREPASS;
    PassData.EnableShadows = FALSE;
    PassData.WriteNormals  = TRUE;

    BackendPassInputs Inputs;
    Inputs.EnableShadows = FALSE;

    for( s32 i = 0; i < pBuilder->Geom().GetPacketCount(); ++i )
    {
        if( pBuilder->Geom().IsFirstPersonDepthPacket( i ) &&
            !pBuilder->Geom().ExecutePacket( i, PassData, Inputs ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::BuildFirstPersonDepthPass( RenderFrameContext& Frame )
{
    s32 PacketCount = 0;
    for( s32 i = 0; i < Geom().GetPacketCount(); ++i )
    {
        if( Geom().IsFirstPersonDepthPacket( i ) )
        {
            ++PacketCount;
        }
    }

    if( !PacketCount )
    {
        return TRUE;
    }

    const RenderResourceDesc* pLinearDepth = Frame.pGraph->GetDesc( Frame.GetSceneLinearDepthTarget() );
    const RenderResourceDesc* pDepth = Frame.pGraph->GetDesc( Frame.GetSceneDepthTarget() );
    if( !pLinearDepth || !pDepth )
    {
        return FALSE;
    }

    RenderResourceDesc LinearDepth;
    LinearDepth.Extent      = pLinearDepth->Extent;
    LinearDepth.Format      = RTARGET_FORMAT_R32F;
    LinearDepth.SampleCount = pLinearDepth->SampleCount;
    LinearDepth.Usage       = RENDER_RESOURCE_USAGE_COLOR |
        ( LinearDepth.SampleCount > 1 ? RENDER_RESOURCE_USAGE_STORAGE_READ : RENDER_RESOURCE_USAGE_SAMPLED );
    LinearDepth.ClearColor[0] = LinearDepth.ClearColor[1] =
        LinearDepth.ClearColor[2] = LinearDepth.ClearColor[3] = 1.0f;
    LinearDepth.pDebugName = "FirstPersonLinearDepth";

    RenderResourceDesc Depth;
    Depth.Extent      = LinearDepth.Extent;
    Depth.Format      = pDepth->Format;
    Depth.SampleCount = LinearDepth.SampleCount;
    Depth.Usage       = RENDER_RESOURCE_USAGE_DEPTH;
    Depth.pDebugName  = "FirstPersonDepth";

    RenderResourceDesc Normals = LinearDepth;
    Normals.Format     = RTARGET_FORMAT_RGBA8;
    Normals.pDebugName = "FirstPersonNormals";

    Frame.FirstPersonLinearDepth = Frame.pGraph->CreateResource( LinearDepth );
    Frame.FirstPersonDepth = Frame.pGraph->CreateResource( Depth );
    Frame.FirstPersonNormals = Frame.pGraph->CreateResource( Normals );
    if( !Frame.FirstPersonLinearDepth.IsValid() || !Frame.FirstPersonDepth.IsValid() ||
        !Frame.FirstPersonNormals.IsValid() )
    {
        return FALSE;
    }

    RenderPassDesc Desc;
    Desc.pDebugName   = "FirstPersonDepthPrepass";
    Desc.Color        = Frame.FirstPersonLinearDepth;
    Desc.Color1       = Frame.FirstPersonNormals;
    Desc.Depth        = Frame.FirstPersonDepth;
    Desc.ColorLoadOp  = RTARGET_LOAD_CLEAR;
    Desc.DepthLoadOp  = RTARGET_LOAD_CLEAR;
    const RenderPassId Pass = Frame.pGraph->AddPass( Desc, ExecuteFirstPersonDepthPass, this );
    if( Pass == INVALID_RENDER_PASS )
    {
        return FALSE;
    }

    Geom().DeclarePassResources( *Frame.pGraph, Pass );
    for( s32 i = 0; i < Geom().GetPacketCount(); ++i )
    {
        if( Geom().IsFirstPersonDepthPacket( i ) )
        {
            Geom().DeclarePacketResources( *Frame.pGraph, Pass, i );
        }
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::BuildDepthPass( const RenderFrameContext& Frame )
{
    RenderPassDesc DescData;
    DescData.pDebugName  = "SceneDepthPrepass";
    DescData.Color       = Frame.GetSceneLinearDepthTarget();
    DescData.Color1      = Frame.SceneNormals;
    DescData.Depth       = Frame.GetSceneDepthTarget();
    DescData.ColorLoadOp = RTARGET_LOAD_CLEAR;
    DescData.DepthLoadOp = RTARGET_LOAD_CLEAR;

    const RenderPassId Pass = Frame.pGraph->AddPass( DescData, ExecuteDepthPass, this );
    if( Pass == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "ForwardEncoder", "failed to declare SceneDepthPrepass" );
        return FALSE;
    }

    Geom().DeclarePassResources( *Frame.pGraph, Pass );
    for( s32 i = 0; i < Geom().GetPacketCount(); ++i )
    {
        if( Geom().GetPacketPass( i ) == GEOM_PASS_FORWARD_OPAQUE && !Geom().IsFirstPersonPacket( i ) )
        {
            Geom().DeclarePacketResources( *Frame.pGraph, Pass, i );
        }
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::ExecuteOpaquePass( RenderPassContext& Context, ForwardEncoder* pBuilder )
{
    const RenderFrameContext& Frame       = Context.GetFrame();
    const rtarget*            pSceneColor = Context.Write( Frame.GetSceneColorTarget() );
    const rtarget*            pSceneDepth = Context.Write( Frame.GetSceneDepthTarget() );

    if( !pBuilder || !pSceneColor || !pSceneDepth )
    {
        RENDER_THROW( "ForwardEncoder", "opaque pass scene targets are unavailable" );
        return FALSE;
    }

    if( !pBuilder->Geom().PreparePassConstants( Context.GetView(), pSceneColor->Desc.Width, pSceneColor->Desc.Height ) )
    {
        return FALSE;
    }

    geom_pass_desc PassData;
    PassData.TargetWidth     = pSceneColor->Desc.Width;
    PassData.TargetHeight    = pSceneColor->Desc.Height;
    PassData.BloomFogMode    = pBuilder->Bloom().IsEnabled() ? RENDER_FOG_FORWARD_EMISSION : RENDER_FOG_NONE;
    PassData.pView           = Context.GetView();
    PassData.ColorFormat     = pSceneColor->Desc.Format;
    PassData.DepthFormat     = pSceneDepth->Desc.Format;
    PassData.SampleCount     = pSceneColor->Desc.SampleCount;
    PassData.DepthMode       = GEOM_DEPTH_READ_ONLY;
    PassData.EnableShadows   = Frame.Desc.EnableShadows;

    const RenderResourceHandle ShadowSample = pBuilder->Shadows().GetShadowSampleAtlasHandle();
    const RenderResourceHandle ShadowDepth  = pBuilder->Shadows().GetShadowDepthAtlasHandle();
    BackendPassInputs          Inputs;
    Inputs.EnableShadows = Context.GetFrame().Desc.EnableShadows;
    Inputs.pShadowSample = ShadowSample.IsValid() ? Context.Read( ShadowSample ) : NULL;
    Inputs.pShadowDepth  = ShadowDepth.IsValid() ? Context.Read( ShadowDepth ) : NULL;

    if( Frame.SceneAmbientVisibility.IsValid() )
    {
        Inputs.pAmbientVisibility = Context.Read( Frame.SceneAmbientVisibility );
        if( !Inputs.pAmbientVisibility )
        {
            return FALSE;
        }
    }

    if( !pBuilder->Geom().ExecuteForwardOpaque( PassData, Inputs ) )
    {
        RENDER_THROW( "ForwardEncoder", "opaque geometry execution failed" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::BuildOpaquePass( const RenderFrameContext& Frame )
{
    RenderPassDesc DescData;
    DescData.pDebugName  = "SceneOpaque";
    DescData.Color       = Frame.GetSceneColorTarget();
    DescData.Depth       = Frame.GetSceneDepthTarget();
    DescData.ColorLoadOp = RTARGET_LOAD_CLEAR;
    DescData.DepthLoadOp = RTARGET_LOAD_LOAD;

    const RenderPassId PassData = Frame.pGraph->AddPass( DescData, ExecuteOpaquePass, this );

    if( PassData == INVALID_RENDER_PASS )
    {
        RENDER_THROW( "ForwardEncoder", "failed to declare SceneOpaque" );
        return FALSE;
    }

    Geom().DeclarePassResources( *Frame.pGraph, PassData );

    if( Frame.SceneAmbientVisibility.IsValid() )
    {
        Frame.pGraph->Read( PassData, Frame.SceneAmbientVisibility );
    }

    Shadows().DeclareConsumer( *Frame.pGraph, PassData );

    for( s32 i = 0; i < Geom().GetPacketCount(); ++i )
    {
        if( Geom().GetPacketPass( i ) == GEOM_PASS_FORWARD_OPAQUE && !Geom().IsFirstPersonPacket( i ) )
        {
            Geom().DeclarePacketResources( *Frame.pGraph, PassData, i );
        }
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::BuildPreFogPass( const RenderFrameContext& Frame )
{
    if( !SealSubmission( Frame ) )
    {
        return FALSE;
    }

    if( !BuildPhasePass( Frame, "ForwardPreFog", FORWARD_PHASE_SURFACE, FORWARD_PHASE_DECAL,
                         0, m_Items.GetCount() ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::BuildPostFogPass( const RenderFrameContext& Frame )
{
    if( m_SubmissionState != SUBMISSION_SEALED || m_pFrame != &Frame )
    {
        RENDER_THROW( "ForwardEncoder", "post-fog pass declared before submission seal" );
        return FALSE;
    }

    if( !BuildOrderedPasses( Frame, "ForwardOrderedComposite", FORWARD_PHASE_TRANSPARENT, FORWARD_PHASE_DISTORTION ) )
    {
        return FALSE;
    }

    m_SubmissionState = SUBMISSION_DECLARED;

    return TRUE;
}

//=========================================================================

xbool ForwardEncoder::BuildFirstPersonPass( const RenderFrameContext& Frame )
{
    if( !BuildPhasePass( Frame, "FirstPersonOpaque", FORWARD_PHASE_FIRST_PERSON, FORWARD_PHASE_FIRST_PERSON,
                         0, m_Items.GetCount(), TRUE ) )
    {
        return FALSE;
    }

    return BuildOrderedPasses( Frame, "FirstPersonTransparent", FORWARD_PHASE_FIRST_PERSON, FORWARD_PHASE_FIRST_PERSON );
}

//=========================================================================
//  SUBMISSION STATE
//=========================================================================

void ForwardEncoder::Clear( void )
{
    for( s32 i = 0; i < m_nBatches; ++i )
    {
        *m_Batches[i] = SubmissionBatch();
    }

    m_nBatches = 0;
    m_BatchItemIndices.SetCount( 0 );
    m_Items.SetCount( 0 );
    m_NextSequence    = 0;
    m_SubmissionState = SUBMISSION_COLLECTING;
    m_pFrame          = NULL;
    m_DistortionScene = RenderResourceHandle();
}

//=========================================================================

ForwardEncoder::SubmissionBatch* ForwardEncoder::AcquireBatch( void )
{
    if( m_nBatches == m_Batches.GetCount() )
    {
        m_Batches.Append() = new SubmissionBatch();
    }

    SubmissionBatch& Batch = *m_Batches[m_nBatches++];
    Batch                  = SubmissionBatch();
    Batch.pOwner           = this;

    return &Batch;
}

//=========================================================================

FrameCompiler& ForwardEncoder::Geom( void ) const
{
    ASSERT( m_pGeom );
    return *m_pGeom;
}

//=========================================================================

DecalEncoder& ForwardEncoder::Decals( void ) const
{
    ASSERT( m_pDecals );
    return *m_pDecals;
}

//=========================================================================

PrimitiveStorage& ForwardEncoder::Primitives( void ) const
{
    ASSERT( m_pPrimitives );
    return *m_pPrimitives;
}

//=========================================================================

BloomEffect& ForwardEncoder::Bloom( void ) const
{
    ASSERT( m_pBloom );
    return *m_pBloom;
}

//=========================================================================

ShadowRenderer& ForwardEncoder::Shadows( void ) const
{
    ASSERT( m_pShadows );
    return *m_pShadows;
}

//=========================================================================

} // namespace render
