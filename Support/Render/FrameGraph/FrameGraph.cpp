//=========================================================================
//
//  FrameGraph.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameGraph_Private.hpp"
#include "../RenderScale.hpp"
#include "e_Engine.hpp"
#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool RenderTargetBinding::IsValid( void ) const
{
    if( !pColor || !Extent.IsValid() || ( SampleCount == 0 ) || ( ArrayLayers == 0 ) || ( ViewMask == 0 ) )
    {
        return FALSE;
    }

    const xbool ColorValid = rtarget_HasRenderTarget( *pColor ) && ( pColor->Desc.Width == Extent.Width ) &&
                             ( pColor->Desc.Height == Extent.Height ) && ( pColor->Desc.Format == ColorFormat ) &&
                             ( pColor->Desc.SampleCount == SampleCount );
    const xbool DepthValid =
        !pDepth || ( rtarget_HasDepthStencil( *pDepth ) && ( pDepth->Desc.Format == DepthFormat ) &&
                     ( pDepth->Desc.SampleCount == SampleCount ) );

    return ColorValid && DepthValid && ( ArrayLayers == 1 ) && ( ViewMask == 1 );
}

//=========================================================================

RenderResourceDesc::RenderResourceDesc( void )
    : Type( RENDER_RESOURCE_TEXTURE ),
      Buffer(),
      Extent(),
      Format( RTARGET_FORMAT_COUNT ),
      MipCount( 1 ),
      SampleCount( 1 ),
      ArrayLayers( 1 ),
      ViewMask( 1 ),
      Usage( RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COLOR ),
      Lifetime( RENDER_RESOURCE_TRANSIENT ),
      PersistentKey( 0 ),
      PreserveContents( FALSE ),
      ClearDepth( 1.0f ),
      ClearStencil( 0 ),
      pDebugName( NULL )
{
    for( s32 i = 0; i < 4; ++i )
    {
        ClearColor[i] = 0.0f;
    }
}

//=========================================================================

RenderFrameContext::RenderFrameContext( void )
    : Desc(),
      pResources( NULL ),
      pGraph( NULL ),
      SceneColor(),
      ScenePostColor(),
      SceneColorMSAA(),
      SceneDepth(),
      SceneDepthTarget(),
      SceneDepthMSAA(),
      SceneAmbientVisibility(),
      SceneNormals(),
      FirstPersonDepth(),
      FirstPersonLinearDepth(),
      FirstPersonNormals(),
      FirstPersonAmbientVisibility(),
      AntiAliasingSource(),
      FinalColor(),
      OutputColor(),
      SceneRendered( FALSE )
{
}

//=========================================================================

const rtarget* RenderFrameContext::Get( RenderResourceHandle Resource ) const
{
    return pGraph ? pGraph->Get( Resource ) : NULL;
}

//=========================================================================

xbool RenderFrameContext::GetViewport( rdraw_viewport& Viewport, const rtarget& Target, u32 ViewIndex ) const
{
    const RenderView* pView = GetView( ViewIndex );

    if( !pView )
    {
        return FALSE;
    }

    Viewport = pView->Viewport;

    const RenderExtent SourceExtent =
        Desc.Output.Extent.IsValid()
            ? Desc.Output.Extent
            : RenderExtent( static_cast<u32>( Viewport.Width ), static_cast<u32>( Viewport.Height ) );

    if( SourceExtent.IsValid() )
    {
        const f32 ScaleX = static_cast<f32>( Target.Desc.Width ) / static_cast<f32>( SourceExtent.Width );
        const f32 ScaleY = static_cast<f32>( Target.Desc.Height ) / static_cast<f32>( SourceExtent.Height );
        Viewport.TopLeftX *= ScaleX;
        Viewport.TopLeftY *= ScaleY;
        Viewport.Width *= ScaleX;
        Viewport.Height *= ScaleY;
    }

    return TRUE;
}

//=========================================================================

const RenderView* RenderFrameContext::GetView( u32 ViewIndex ) const
{
    if( !Desc.Views.pViews )
    {
        return NULL;
    }

    for( u32 i = 0; i < Desc.Views.ViewCount; ++i )
    {
        if( Desc.Views.pViews[i].ViewIndex == ViewIndex )
        {
            return &Desc.Views.pViews[i];
        }
    }

    return NULL;
}

//=========================================================================

u64 RenderHistoryKeyForIdentity( u64 FeatureKey, s32 ScopeId, u64 ViewId )
{
    // Use stable feature/view identity for temporal history, not addresses.
    if( ( FeatureKey == 0 ) || ( ViewId == 0 ) )
    {
        return 0;
    }

    u64 Value = FeatureKey ^ ( static_cast<u64>( static_cast<u32>( ScopeId ) ) * 0x9e3779b97f4a7c15ull ) ^
                ( ViewId + 0x632be59bd9b4e019ull );
    Value ^= Value >> 30;
    Value *= 0xbf58476d1ce4e5b9ull;
    Value ^= Value >> 27;
    Value *= 0x94d049bb133111ebull;
    Value ^= Value >> 31;

    return Value ? Value : 1ull;
}

//=========================================================================

u64 RenderHistoryKey( u64 FeatureKey, const RenderFrameContext& Frame )
{
    if( !Frame.Desc.EnableTemporalHistory )
    {
        return 0;
    }

    const RenderView* pView  = Frame.GetView( 0 );
    const u64         ViewId = pView ? pView->ViewId : 0;

    return RenderHistoryKeyForIdentity( FeatureKey, Frame.Desc.ScopeId, ViewId );
}

//=========================================================================

RenderFormatPolicy::RenderFormatPolicy( void )
    : SceneColorFormat( RTARGET_FORMAT_RGBA16F ),
      BloomFormat( RTARGET_FORMAT_RGBA16F ),
      BloomCompositeToLDR( FALSE )
{
}

//=========================================================================

RenderCapabilities::RenderCapabilities( void )
    : SupportedSceneTextures( SCENE_TEXTURE_DEPTH | SCENE_TEXTURE_DEPTH_SAMPLES | SCENE_TEXTURE_HARDWARE_DEPTH |
                              SCENE_TEXTURE_NORMALS ),
      MaxColorAttachments( RTARGET_MAX_TARGETS < 3 ? RTARGET_MAX_TARGETS : 3 ),
      SupportsMultiview( FALSE )
{
}

//=========================================================================

RenderQualityProfile::RenderQualityProfile( void )
    : Formats(),
      RenderScale( RENDER_SCALE_DEFAULT ),
      SampleCount( 1 )
{
}

//=========================================================================

RenderQualityProfile RenderQualityProfile::PlatformDefault( void )
{
    RenderQualityProfile Profile;
#if defined( TARGET_ANDROID )
    Profile.Formats.SceneColorFormat    = RTARGET_FORMAT_RGBA8;
    Profile.Formats.BloomFormat         = RTARGET_FORMAT_RGBA8;
    Profile.Formats.BloomCompositeToLDR = TRUE;
    Profile.RenderScale                 = RENDER_SCALE_MIN;
#endif

    return Profile;
}

//=========================================================================

xbool IsSampleCountSupportedForDevice( u32 SampleCount )
{
    if( ( SampleCount != 1 ) && ( SampleCount != 2 ) && ( SampleCount != 4 ) && ( SampleCount != 8 ) )
    {
        return FALSE;
    }

    // Check every format used by scene and post-processing passes.
    static const rtarget_format Formats[] = { RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_RGBA16F, RTARGET_FORMAT_R32F,
                                              RTARGET_FORMAT_DEPTH_STENCIL };

    for( u32 i = 0; i < ARRAYSIZE( Formats ); ++i )
    {
        if( !rtarget_IsSampleCountSupported( Formats[i], SampleCount ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

u32 SelectSupportedSampleCount( u32 RequestedSampleCount )
{
    static const u32 Candidates[] = { 8, 4, 2, 1 };

    for( u32 i = 0; i < ARRAYSIZE( Candidates ); ++i )
    {
        if( ( Candidates[i] <= RequestedSampleCount ) && IsSampleCountSupportedForDevice( Candidates[i] ) )
        {
            return Candidates[i];
        }
    }

    return 1;
}

//=========================================================================

RenderPassDesc::RenderPassDesc( void )
    : pDebugName( NULL ),
      Type( RENDER_PASS_GRAPHICS ),
      pComputeShader( NULL ),
      ComputeTextureCount( 0 ),
      ComputeBufferCount( 0 ),
      Color(),
      Color1(),
      Color2(),
      ColorResolve(),
      Color1Resolve(),
      Color2Resolve(),
      Depth(),
      Source(),
      Destination(),
      // New attachments default to DONT_CARE; use LOAD only when prior contents exist.
      ColorLoadOp( RTARGET_LOAD_DONT_CARE ),
      ColorStoreOp( RTARGET_STORE_STORE ),
      DepthLoadOp( RTARGET_LOAD_DONT_CARE ),
      DepthStoreOp( RTARGET_STORE_STORE ),
      ViewMask( 1 ),
      ViewIndex( 0 ),
      bOptional( FALSE ),
      ValidateResources( TRUE ),
      CompositePipelineCount( 0 )
{
}

//=========================================================================

const rtarget* RenderPassContext::Read( RenderResourceHandle Resource ) const
{
    return m_Graph.PassDeclaresAccess( m_PassId, Resource, RENDER_RESOURCE_ACCESS_SAMPLED_READ )
               ? m_Frame.Get( Resource )
               : NULL;
}

//=========================================================================

const rtarget* RenderPassContext::Write( RenderResourceHandle Resource ) const
{
    return m_Graph.PassDeclaresWrite( m_PassId, Resource ) ? m_Frame.Get( Resource ) : NULL;
}

//=========================================================================

const rbuffer* RenderPassContext::ReadBuffer( RenderResourceHandle Resource ) const
{
    return m_Graph.PassDeclaresRead( m_PassId, Resource ) ? m_Graph.GetBuffer( Resource ) : NULL;
}

//=========================================================================

const shader_resource* RenderPassContext::ReadStorageTexture( RenderResourceHandle Resource ) const
{
    return m_Graph.PassDeclaresAccess( m_PassId, Resource, RENDER_RESOURCE_ACCESS_STORAGE_READ )
               ? m_Graph.GetShaderResource( Resource ) : NULL;
}

//=========================================================================

xbool RenderPassContext::Dispatch( u32 GroupCountX, u32 GroupCountY, u32 GroupCountZ )
{
    if( m_Pass.Type != RENDER_PASS_COMPUTE || m_isDispatched ||
        !shader_Dispatch( GroupCountX, GroupCountY, GroupCountZ ) )
    {
        return FALSE;
    }

    m_isDispatched = TRUE;
    return TRUE;
}

//=========================================================================

xbool RenderPassContext::DispatchIndirect( RenderResourceHandle Resource, u32 Offset )
{
    const rbuffer* pBuffer = m_Graph.PassDeclaresAccess( m_PassId, Resource, RENDER_RESOURCE_ACCESS_INDIRECT_READ )
                                ? m_Graph.GetBuffer( Resource ) : NULL;
    if( m_Pass.Type != RENDER_PASS_COMPUTE || m_isDispatched || !pBuffer ||
        !shader_DispatchIndirect( *pBuffer, Offset ) )
    {
        return FALSE;
    }

    m_isDispatched = TRUE;
    return TRUE;
}

//=========================================================================

rtarget* RenderPassContext::WriteMutable( RenderResourceHandle Resource ) const
{
    return m_Graph.PassDeclaresWrite( m_PassId, Resource ) ? m_Graph.GetMutable( Resource ) : NULL;
}

//=========================================================================

xbool RenderPassContext::CopyRegion( RenderResourceHandle Destination,
                                     RenderResourceHandle Source,
                                     u32                  SourceX,
                                     u32                  SourceY,
                                     u32                  Width,
                                     u32                  Height ) const
{
    if( m_Pass.Type != RENDER_PASS_COPY ||
        !m_Graph.PassDeclaresAccess( m_PassId, Destination, RENDER_RESOURCE_ACCESS_COPY_DEST ) ||
        !m_Graph.PassDeclaresAccess( m_PassId, Source, RENDER_RESOURCE_ACCESS_COPY_SOURCE ) )
    {
        RENDER_THROW( "FrameGraph", "region copy requires copy resource declarations" );
        return FALSE;
    }

    rtarget_copy_desc Copy;
    Copy.pDestination = m_Graph.GetMutable( Destination );
    Copy.pSource      = m_Frame.Get( Source );
    Copy.SrcX         = SourceX;
    Copy.SrcY         = SourceY;
    Copy.Width        = Width;
    Copy.Height       = Height;
    Copy.Depth        = 1;
    Copy.bCycle       = TRUE;

    if( !Copy.pDestination || !Copy.pSource || !rtarget_Copy( Copy ) )
    {
        RENDER_THROW( "FrameGraph", "backend rejected region copy" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool RenderPassContext::Copy( RenderResourceHandle Destination, RenderResourceHandle Source ) const
{
    if( m_Pass.Type != RENDER_PASS_COPY ||
        !m_Graph.PassDeclaresAccess( m_PassId, Destination, RENDER_RESOURCE_ACCESS_COPY_DEST ) ||
        !m_Graph.PassDeclaresAccess( m_PassId, Source, RENDER_RESOURCE_ACCESS_COPY_SOURCE ) )
    {
        RENDER_THROW( "FrameGraph", "copy context has incompatible pass or resource declarations" );
        return FALSE;
    }

    rtarget*       pDestination = m_Graph.GetMutable( Destination );
    const rtarget* pSource      = m_Frame.Get( Source );

    if( !pDestination || !pSource )
    {
        RENDER_THROW( "FrameGraph", "copy context resources are unavailable" );
        return FALSE;
    }

    const RenderResourceDesc* pDestinationDesc = m_Graph.GetDesc( Destination );

    if( !pDestinationDesc ||
        !rtarget_Copy( *pDestination, *pSource, pDestinationDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) )
    {
        RENDER_THROW( "FrameGraph", "backend rejected resource copy" );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  FRAME GRAPH BUILDER
//=========================================================================
//  GRAPH LIFETIME
//=========================================================================

FrameGraphBuilder::FrameGraphBuilder( FrameResourcePool& Resources )
    : m_Resources( Resources ),
      m_pFrame( NULL ),
      m_ExternalImports(),
      m_ExternalImportCount( 0 ),
      m_GraphResources(),
      m_TransientPhysicalUses(),
      m_TransientAliasBuckets(),
      m_Passes(),
      m_FreeResources(),
      m_FreePasses(),
      m_PayloadBlocks(),
      m_ExecutionOrder(),
      m_LiveQueue(),
      m_ReadyQueue(),
      m_Incoming(),
      m_Output(),
      m_Generation( 0 ),
      m_pLastError( NULL ),
      m_pFailurePass( NULL ),
      m_pFailureResource( NULL ),
      m_isCompiled( FALSE ),
      m_isExecuted( FALSE )
{
    // Keep pass payload storage stable through execution.
    m_PayloadBlocks.SetCapacity( 256 );
}

//=========================================================================

FrameGraphBuilder::~FrameGraphBuilder( void )
{
    Reset();

    for( s32 i = 0; i < m_FreeResources.GetCount(); ++i )
    {
        delete m_FreeResources[i];
    }

    for( s32 i = 0; i < m_FreePasses.GetCount(); ++i )
    {
        delete m_FreePasses[i];
    }

    m_FreeResources.Clear();
    m_FreePasses.Clear();
}

//=========================================================================

void FrameGraphBuilder::BeginFrame( RenderFrameContext& Frame )
{
    Reset();
    ++m_Generation;

    if( m_Generation == 0 )
    {
        ++m_Generation;
    }

    m_pFrame         = &Frame;
    Frame.pResources = &m_Resources;
    Frame.pGraph     = this;

    if( Frame.Desc.Views.ViewCount != 1 || Frame.Desc.Views.ViewMask != 1 )
    {
        Fail( "frame graph supports only a single view" );
    }
}

//=========================================================================

void FrameGraphBuilder::Reset( void )
{
    shader_SetResourceAccessValidator( NULL, NULL );
    rtarget_SetGraphPassOwner( NULL );
    rtarget_EndPass();

    for( s32 i = 0; i < m_PayloadBlocks.GetCount(); ++i )
    {
        m_PayloadBlocks[i].Release();
    }

    for( s32 i = 0; i < m_GraphResources.GetCount(); ++i )
    {
        m_FreeResources.Append() = m_GraphResources[i];
    }

    for( s32 i = 0; i < m_Passes.GetCount(); ++i )
    {
        m_Passes[i]->pPayload = NULL;

        m_FreePasses.Append() = m_Passes[i];
    }

    m_PayloadBlocks.SetCount( 0 );
    m_CompositeRequirements.SetCount( 0 );
    for( s32 i = 0; i < m_ExternalImports.GetCount(); ++i )
    {
        m_ExternalImports[i].pResource = NULL;
    }
    m_ExternalImportCount = 0;
    m_GraphResources.SetCount( 0 );
    m_LiveResources.SetCount( 0 );
    m_TransientPhysicalUses.SetCount( 0 );
    m_TransientAliasBuckets.SetCount( 0 );
    m_Passes.SetCount( 0 );
    m_PendingTextureUploads.SetCount( 0 );
    m_ExecutionOrder.SetCount( 0 );
    m_LiveQueue.SetCount( 0 );
    m_ReadyQueue.SetCount( 0 );
    m_Incoming.SetCount( 0 );
    m_Output           = RenderResourceHandle();
    m_pFrame           = NULL;
    m_pLastError       = NULL;
    m_pFailurePass     = NULL;
    m_pFailureResource = NULL;
    m_isCompiled       = FALSE;
    m_isExecuted       = FALSE;
}

//=========================================================================

void FrameGraphBuilder::ReleasePayload( RenderPassPayload* pPayload )
{
    if( !pPayload )
    {
        return;
    }

    for( s32 i = 0; i < m_PayloadBlocks.GetCount(); ++i )
    {
        if( m_PayloadBlocks[i].pPayload == pPayload )
        {
            m_PayloadBlocks[i].Release();
            return;
        }
    }
}

//=========================================================================

xbool FrameGraphBuilder::CanDeclare( const char* pOperation )
{
    if( !m_pFrame )
    {
        Fail( "graph declaration requires an active frame" );
        return FALSE;
    }

    if( m_isCompiled || m_isExecuted )
    {
        RENDER_THROW( "FrameGraph", "%s is not allowed after graph seal", pOperation );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  RESOURCE DECLARATION
//=========================================================================

FrameGraphBuilder::Resource* FrameGraphBuilder::AllocateResource( void )
{
    Resource* pResource = NULL;

    if( m_FreeResources.GetCount() > 0 )
    {
        const s32 FreeIndex = m_FreeResources.GetCount() - 1;
        pResource = m_FreeResources[FreeIndex];
        m_FreeResources.SetCount( FreeIndex );
        pResource->Reset();
    }
    else
    {
        pResource = new Resource;
    }

    return pResource;
}

//=========================================================================

RenderResourceHandle FrameGraphBuilder::CreateResource( const RenderResourceDesc& Desc )
{
    if( !CanDeclare( "CreateResource" ) )
    {
        return RenderResourceHandle();
    }

    if( !IsResourceDescValid( Desc ) )
    {
        Fail( "invalid graph resource descriptor" );
        return RenderResourceHandle();
    }

    Resource* pResource = AllocateResource();

    if( !pResource )
    {
        Fail( "failed to allocate graph resource bookkeeping" );
        return RenderResourceHandle();
    }

    pResource->Handle = RenderResourceHandle( static_cast<u32>( m_GraphResources.GetCount() ), m_Generation );
    pResource->Desc   = Desc;
    pResource->HasInitialContents =
        Desc.Lifetime == RENDER_RESOURCE_PERSISTENT && m_Resources.HasPersistentResource( Desc.PersistentKey );
    m_GraphResources.Append() = pResource;

    return pResource->Handle;
}

//=========================================================================

xbool FrameGraphBuilder::RetainResource( RenderResourceHandle Handle, u64 PersistentKey )
{
    if( !CanDeclare( "RetainResource" ) )
    {
        return FALSE;
    }

    Resource* pResource = IsHandleValid( Handle ) ? m_GraphResources[Handle.Index] : NULL;

    if( !pResource || !PersistentKey || pResource->Desc.Lifetime != RENDER_RESOURCE_TRANSIENT ||
        pResource->Desc.SampleCount != 1 )
    {
        Fail( "only single-sample transient resources can be retained" );
        return FALSE;
    }

    pResource->Desc.Lifetime         = RENDER_RESOURCE_PERSISTENT;
    pResource->Desc.PersistentKey    = PersistentKey;
    pResource->Desc.PreserveContents = TRUE;

    return TRUE;
}

//=========================================================================

namespace
{

u32 HashExternalImport( const void* pBackend, u32 Kind )
{
    u64 Hash = static_cast<u64>( reinterpret_cast<uaddr>( pBackend ) ) ^ Kind;
    Hash ^= Hash >> 30;
    Hash *= 0xbf58476d1ce4e5b9ull;
    Hash ^= Hash >> 27;
    Hash *= 0x94d049bb133111ebull;
    Hash ^= Hash >> 31;
    return static_cast<u32>( Hash ^ ( Hash >> 32 ) );
}

} // namespace

//=========================================================================

FrameGraphBuilder::Resource* FrameGraphBuilder::FindExternalImport( const void* pBackend, u32 Kind ) const
{
    if( !m_ExternalImports.GetCount() )
    {
        return NULL;
    }

    const u32 Mask = static_cast<u32>( m_ExternalImports.GetCount() ) - 1u;
    u32 Slot = HashExternalImport( pBackend, Kind ) & Mask;
    while( m_ExternalImports[Slot].pResource )
    {
        const ExternalImport& Entry = m_ExternalImports[Slot];
        if( Entry.pBackend == pBackend && Entry.Kind == Kind )
        {
            return Entry.pResource;
        }
        Slot = ( Slot + 1u ) & Mask;
    }
    return NULL;
}

//=========================================================================

void FrameGraphBuilder::IndexExternalImport( Resource& ResourceData )
{
    if( !m_ExternalImports.GetCount() ||
        ( m_ExternalImportCount + 1u ) * 2u > static_cast<u32>( m_ExternalImports.GetCount() ) )
    {
        u32 Capacity = 16;
        while( Capacity < static_cast<u32>( m_GraphResources.GetCount() ) * 2u )
        {
            if( Capacity >= 0x40000000u )
            {
                Fail( "external import lookup exceeds array capacity" );
                return;
            }
            Capacity *= 2u;
        }
        m_ExternalImports.SetCount( Capacity );
        for( u32 i = 0; i < Capacity; ++i )
        {
            m_ExternalImports[i].pResource = NULL;
        }
        m_ExternalImportCount = 0;
        for( s32 i = 0; i < m_GraphResources.GetCount(); ++i )
        {
            Resource& Existing = *m_GraphResources[i];
            if( Existing.pExternalBackend || Existing.pExternalBufferBackend || Existing.pExternalTextureBackend )
            {
                IndexExternalImport( Existing );
            }
        }
        return;
    }

    const u32 Kind = ResourceData.pExternalBufferBackend ? EXTERNAL_IMPORT_BUFFER :
                     ResourceData.pExternalTextureBackend ? EXTERNAL_IMPORT_TEXTURE : EXTERNAL_IMPORT_TARGET;
    const void* pBackend = ResourceData.pExternalBackend;
    if( ResourceData.pExternalBufferBackend )
    {
        pBackend = ResourceData.pExternalBufferBackend;
    }
    else if( ResourceData.pExternalTextureBackend )
    {
        pBackend = ResourceData.pExternalTextureBackend;
    }
    const u32 Mask = static_cast<u32>( m_ExternalImports.GetCount() ) - 1u;
    u32 Slot = HashExternalImport( pBackend, Kind ) & Mask;
    while( m_ExternalImports[Slot].pResource )
    {
        Slot = ( Slot + 1u ) & Mask;
    }
    ExternalImport& Entry = m_ExternalImports[Slot];
    Entry.pBackend = pBackend;
    Entry.pResource = &ResourceData;
    Entry.Kind = Kind;
    ++m_ExternalImportCount;
}

//=========================================================================

RenderResourceHandle FrameGraphBuilder::ImportExternal( const rtarget& Target,
                                                        const char*    pDebugName,
                                                        u32            ExternalUsage )
{
    if( !CanDeclare( "ImportExternal" ) )
    {
        return RenderResourceHandle();
    }

    u32 UsageData = RENDER_RESOURCE_USAGE_NONE;

    if( !GetExternalUsage( Target, ExternalUsage, UsageData ) )
    {
        Fail( "external target capabilities are incompatible" );
        return RenderResourceHandle();
    }

    Resource* pExisting = FindExternalImport( Target.pBackend, EXTERNAL_IMPORT_TARGET );
    if( pExisting )
    {
        Resource& ResourceData = *pExisting;
        const rtarget* pTarget = m_Resources.Get( ResourceData.Physical );

        if( !pTarget || ResourceData.Desc.Extent.Width != Target.Desc.Width ||
            ResourceData.Desc.Extent.Height != Target.Desc.Height || ResourceData.Desc.Format != Target.Desc.Format ||
            ResourceData.Desc.SampleCount != Target.Desc.SampleCount ||
            ResourceData.Desc.MipCount != Target.Desc.MipCount )
        {
            Fail( "external imports disagree on the target descriptor" );
            return RenderResourceHandle();
        }

        ResourceData.Desc.Usage |= UsageData;
        return ResourceData.Handle;
    }

    RenderResourceHandle Physical = m_Resources.ImportExternal( Target, pDebugName, ExternalUsage );

    if( !Physical.IsValid() )
    {
        Fail( "failed to import external graph resource" );
        return RenderResourceHandle();
    }

    Resource* pResource = AllocateResource();

    if( !pResource )
    {
        Fail( "failed to allocate external graph resource bookkeeping" );
        return RenderResourceHandle();
    }

    pResource->Handle           = RenderResourceHandle( static_cast<u32>( m_GraphResources.GetCount() ), m_Generation );
    pResource->Physical         = Physical;
    pResource->pExternalBackend = Target.pBackend;
    pResource->Desc.Extent      = RenderExtent( Target.Desc.Width, Target.Desc.Height );
    pResource->Desc.Format      = Target.Desc.Format;
    pResource->Desc.MipCount    = Target.Desc.MipCount;
    pResource->Desc.SampleCount = Target.Desc.SampleCount;
    pResource->Desc.ArrayLayers = 1;
    pResource->Desc.ViewMask    = 1;
    pResource->Desc.Usage       = UsageData;
    pResource->Desc.Lifetime    = RENDER_RESOURCE_EXTERNAL;
    pResource->Desc.pDebugName  = pDebugName;
    pResource->HasInitialContents = TRUE;

    m_GraphResources.Append() = pResource;
    IndexExternalImport( *pResource );

    return pResource->Handle;
}

//=========================================================================

RenderResourceHandle FrameGraphBuilder::ImportExternalMutable( rtarget&    Target,
                                                               const char* pDebugName,
                                                               u32         ExternalUsage )
{
    const RenderResourceHandle Logical = ImportExternal( Target, pDebugName, ExternalUsage );

    if( !Logical.IsValid() || !m_Resources.MarkExternalMutable( m_GraphResources[Logical.Index]->Physical, Target ) )
    {
        Fail( "failed to import mutable external graph resource" );
        return RenderResourceHandle();
    }

    return Logical;
}

//=========================================================================
//  PASS DECLARATION
//=========================================================================

RenderResourceHandle FrameGraphBuilder::ImportExternal( const rbuffer& Buffer, const char* pDebugName,
                                                        xbool HasInitialContents )
{
    if( !CanDeclare( "ImportExternal" ) || !Buffer || !Buffer.Desc.Size || !pDebugName )
    {
        Fail( "invalid external buffer" );
        return RenderResourceHandle();
    }

    const Resource* pExisting = FindExternalImport( Buffer.pBackend, EXTERNAL_IMPORT_BUFFER );
    if( pExisting )
    {
        if( pExisting->Desc.Buffer.Size != Buffer.Desc.Size ||
            pExisting->Desc.Buffer.Stride != Buffer.Desc.Stride ||
            pExisting->Desc.Buffer.UsageFlags != Buffer.Desc.UsageFlags )
        {
            Fail( "external imports disagree on the buffer descriptor" );
            return RenderResourceHandle();
        }
        return pExisting->Handle;
    }

    Resource* pResource = AllocateResource();
    if( !pResource )
    {
        Fail( "failed to allocate external buffer bookkeeping" );
        return RenderResourceHandle();
    }

    pResource->Handle                 = RenderResourceHandle( static_cast<u32>( m_GraphResources.GetCount() ), m_Generation );
    pResource->Desc.Buffer            = Buffer.Desc;
    pResource->Desc.Type              = RENDER_RESOURCE_BUFFER;
    pResource->Desc.Usage             = RENDER_RESOURCE_USAGE_NONE;
    pResource->Desc.Lifetime          = RENDER_RESOURCE_EXTERNAL;
    pResource->Desc.pDebugName        = pDebugName;
    pResource->pExternalBuffer        = &Buffer;
    pResource->pExternalBufferBackend = Buffer.pBackend;
    pResource->HasInitialContents     = HasInitialContents;

    m_GraphResources.Append() = pResource;
    IndexExternalImport( *pResource );
    return pResource->Handle;
}

//=========================================================================

RenderResourceHandle FrameGraphBuilder::ImportExternal( const vram_texture& Texture, const char* pDebugName )
{
    if( !CanDeclare( "ImportExternal" ) || !Texture || !pDebugName )
    {
        Fail( "invalid external texture" );
        return RenderResourceHandle();
    }

    const Resource* pExisting = FindExternalImport( Texture.pBackend, EXTERNAL_IMPORT_TEXTURE );
    if( pExisting )
    {
        return pExisting->Handle;
    }

    Resource* pResource = AllocateResource();
    if( !pResource )
    {
        Fail( "failed to allocate external texture bookkeeping" );
        return RenderResourceHandle();
    }

    const u32 Usage = Texture.Desc.UsageFlags;
    pResource->Handle = RenderResourceHandle( static_cast<u32>( m_GraphResources.GetCount() ), m_Generation );
    pResource->Desc.Extent = RenderExtent( Texture.Desc.Width, Texture.Desc.Height );
    pResource->Desc.SampleCount = Texture.Desc.SampleCount;
    pResource->Desc.MipCount = Texture.Desc.MipCount;
    pResource->Desc.ArrayLayers = Texture.Desc.LayerCount;
    pResource->Desc.Lifetime = RENDER_RESOURCE_EXTERNAL;
    pResource->Desc.pDebugName = pDebugName;
    pResource->Desc.Usage = RENDER_RESOURCE_USAGE_COPY_SRC | RENDER_RESOURCE_USAGE_COPY_DST |
        ( Usage & VRAM_TEXTURE_USAGE_SAMPLED ? RENDER_RESOURCE_USAGE_SAMPLED : 0 ) |
        ( Usage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ ? RENDER_RESOURCE_USAGE_STORAGE_READ : 0 ) |
        ( Usage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE ? RENDER_RESOURCE_USAGE_STORAGE_WRITE : 0 ) |
        ( Usage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE ? RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE : 0 );
    pResource->pExternalTexture = &Texture;
    pResource->pExternalTextureBackend = Texture.pBackend;
    pResource->ExternalTextureDesc = Texture.Desc;
    pResource->HasInitialContents = TRUE;
    m_GraphResources.Append() = pResource;
    IndexExternalImport( *pResource );
    return pResource->Handle;
}

//=========================================================================

RenderResourceHandle FrameGraphBuilder::ImportExternal( const shader_resource& ResourceData, const char* pDebugName )
{
    if( const rbuffer* pBuffer = shader_GetResourceBuffer( ResourceData ) )
    {
        return ImportExternal( *pBuffer, pDebugName );
    }
    if( const rtarget* pTarget = shader_GetResourceTarget( ResourceData ) )
    {
        return ImportExternal( *pTarget, pDebugName );
    }
    if( const vram_texture* pTexture = shader_GetResourceTexture( ResourceData ) )
    {
        return ImportExternal( *pTexture, pDebugName );
    }

    Fail( "shader resource has no backing resource owner" );
    return RenderResourceHandle();
}

//=========================================================================

RenderPassId FrameGraphBuilder::AddPassPayload( const RenderPassDesc& Desc, RenderPassPayload* pPayload )
{
    if( !CanDeclare( "AddPass" ) )
    {
        return INVALID_RENDER_PASS;
    }

    if( !pPayload || !Desc.pDebugName )
    {
        Fail( "pass requires a name and execute callback" );
        return INVALID_RENDER_PASS;
    }

    if( Desc.bOptional && ( Desc.Color.IsValid() || Desc.Color1.IsValid() || Desc.Color2.IsValid() ||
                            Desc.ColorResolve.IsValid() || Desc.Color1Resolve.IsValid() ||
                            Desc.Color2Resolve.IsValid() || Desc.Depth.IsValid() || Desc.Destination.IsValid() ) )
    {
        Fail( "optional pass must not declare graph writes; provide a fallback branch" );
        return INVALID_RENDER_PASS;
    }

    Pass* pPass = NULL;

    if( m_FreePasses.GetCount() > 0 )
    {
        const s32 FreeIndex = m_FreePasses.GetCount() - 1;
        pPass               = m_FreePasses[FreeIndex];
        m_FreePasses.SetCount( FreeIndex );
        pPass->Reset();
    }
    else
    {
        pPass = new Pass;
    }

    if( !pPass )
    {
        Fail( "failed to allocate render pass bookkeeping" );
        return INVALID_RENDER_PASS;
    }

    pPass->Desc     = Desc;
    pPass->pPayload = pPayload;

    m_Passes.Append() = pPass;

    const RenderPassId PassData = static_cast<RenderPassId>( m_Passes.GetCount() - 1 );

    if( Desc.Type == RENDER_PASS_COMPUTE )
    {
        if( Desc.ComputeTextureCount > ARRAYSIZE( Desc.ComputeTextures ) ||
            Desc.ComputeBufferCount > ARRAYSIZE( Desc.ComputeBuffers ) )
        {
            Fail( "compute pass exceeds writable binding capacity" );
            return INVALID_RENDER_PASS;
        }

        for( u32 Kind = 0; Kind < 2; ++Kind )
        {
            const RenderComputeBinding* pBindings = Kind ? Desc.ComputeBuffers : Desc.ComputeTextures;
            const u32 Count = Kind ? Desc.ComputeBufferCount : Desc.ComputeTextureCount;
            for( u32 i = 0; i < Count; ++i )
            {
                AddWriteAccess( PassData, pBindings[i].Resource, RENDER_RESOURCE_ACCESS_STORAGE_WRITE );
                if( pBindings[i].ReadWrite )
                {
                    AddReadAccess( PassData, pBindings[i].Resource, RENDER_RESOURCE_ACCESS_STORAGE_READ_WRITE );
                }
                if( pBindings[i].PreserveContents )
                {
                    AddReadAccess( PassData, pBindings[i].Resource, RENDER_RESOURCE_ACCESS_PRESERVE );
                }
            }
        }
    }

    if( Desc.Color.IsValid() )
    {
        AddWriteAccess( PassData, Desc.Color,
                        RENDER_RESOURCE_ACCESS_COLOR_WRITE |
                            ( Desc.ColorResolve.IsValid() ? RENDER_RESOURCE_ACCESS_RESOLVE_SOURCE : 0 ) );
    }

    if( Desc.Color1.IsValid() )
    {
        AddWriteAccess( PassData, Desc.Color1,
                        RENDER_RESOURCE_ACCESS_COLOR_WRITE |
                            ( Desc.Color1Resolve.IsValid() ? RENDER_RESOURCE_ACCESS_RESOLVE_SOURCE : 0 ) );
    }

    if( Desc.Color2.IsValid() )
    {
        AddWriteAccess( PassData, Desc.Color2,
                        RENDER_RESOURCE_ACCESS_COLOR_WRITE |
                            ( Desc.Color2Resolve.IsValid() ? RENDER_RESOURCE_ACCESS_RESOLVE_SOURCE : 0 ) );
    }

    if( Desc.ColorResolve.IsValid() )
    {
        AddWriteAccess( PassData, Desc.ColorResolve, RENDER_RESOURCE_ACCESS_RESOLVE_DEST );
    }

    if( Desc.Color1Resolve.IsValid() )
    {
        AddWriteAccess( PassData, Desc.Color1Resolve, RENDER_RESOURCE_ACCESS_RESOLVE_DEST );
    }

    if( Desc.Color2Resolve.IsValid() )
    {
        AddWriteAccess( PassData, Desc.Color2Resolve, RENDER_RESOURCE_ACCESS_RESOLVE_DEST );
    }

    if( Desc.Depth.IsValid() )
    {
        AddWriteAccess( PassData, Desc.Depth, RENDER_RESOURCE_ACCESS_DEPTH_WRITE );
    }

    if( Desc.Destination.IsValid() )
    {
        AddWriteAccess( PassData, Desc.Destination, RENDER_RESOURCE_ACCESS_COPY_DEST );
    }

    if( Desc.Source.IsValid() )
    {
        AddReadAccess( PassData, Desc.Source,
                       Desc.Type == RENDER_PASS_COPY ? RENDER_RESOURCE_ACCESS_COPY_SOURCE
                                                     : RENDER_RESOURCE_ACCESS_SAMPLED_READ );
    }

    return PassData;
}

//=========================================================================

xbool FrameGraphBuilder::DeclareCompositePipeline( RenderPassId         PassId,
                                                   composite_blend_mode Blend,
                                                   const shader*        pPixelShader )
{
    if( !CanDeclare( "DeclareCompositePipeline" ) )
    {
        return FALSE;
    }

    if( PassId >= static_cast<u32>( m_Passes.GetCount() ) )
    {
        Fail( "invalid pass in composite pipeline declaration" );
        return FALSE;
    }

    if( m_Passes[PassId]->Desc.Type == RENDER_PASS_COPY || m_Passes[PassId]->Desc.Type == RENDER_PASS_COMPUTE )
    {
        Fail( "composite pipeline requires a graphics pass" );
        return FALSE;
    }

    if( m_Passes[PassId]->Desc.CompositePipelineCount >= RENDER_PASS_MAX_COMPOSITE_PIPELINES )
    {
        Fail( "pass declares too many composite pipelines" );
        return FALSE;
    }

    RenderPassDesc::CompositePipelineRequest& Request =
        m_Passes[PassId]->Desc.CompositePipelines[m_Passes[PassId]->Desc.CompositePipelineCount++];
    Request.Blend        = Blend;
    Request.pPixelShader = pPixelShader;

    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::IsHandleValid( RenderResourceHandle ResourceHandle ) const
{
    return ResourceHandle.IsValid() && ( ResourceHandle.Generation == m_Generation ) &&
           ( ResourceHandle.Index < static_cast<u32>( m_GraphResources.GetCount() ) );
}

//=========================================================================

xbool FrameGraphBuilder::PassDeclaresRead( RenderPassId PassId, RenderResourceHandle ResourceHandle ) const
{
    if( ( PassId >= static_cast<u32>( m_Passes.GetCount() ) ) || !IsHandleValid( ResourceHandle ) )
    {
        return FALSE;
    }

    const Pass& PassData = *m_Passes[PassId];

    for( s32 i = 0; i < PassData.Reads.GetCount(); ++i )
    {
        if( PassData.Reads[i] == ResourceHandle.Index )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

xbool FrameGraphBuilder::PassDeclaresWrite( RenderPassId PassId, RenderResourceHandle ResourceHandle ) const
{
    if( ( PassId >= static_cast<u32>( m_Passes.GetCount() ) ) || !IsHandleValid( ResourceHandle ) )
    {
        return FALSE;
    }

    const Pass& PassData = *m_Passes[PassId];

    for( s32 i = 0; i < PassData.Writes.GetCount(); ++i )
    {
        if( PassData.Writes[i] == ResourceHandle.Index )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

xbool FrameGraphBuilder::PassDeclaresAccess( RenderPassId         PassId,
                                             RenderResourceHandle ResourceHandle,
                                             u32                  Access ) const
{
    if( ( PassId >= static_cast<u32>( m_Passes.GetCount() ) ) || !IsHandleValid( ResourceHandle ) || !Access )
    {
        return FALSE;
    }

    const Pass& PassData = *m_Passes[PassId];

    for( s32 i = 0; i < PassData.Accesses.GetCount(); ++i )
    {
        const PassAccess& PassAccessData = PassData.Accesses[i];

        if( ( PassAccessData.Resource == ResourceHandle.Index ) && ( ( PassAccessData.Flags & Access ) == Access ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================
//  RESOURCE ACCESS
//=========================================================================

void FrameGraphBuilder::AddReadAccess( RenderPassId PassId, RenderResourceHandle ResourceHandle, u32 Access )
{
    if( ( PassId >= static_cast<u32>( m_Passes.GetCount() ) ) || !IsHandleValid( ResourceHandle ) || !Access )
    {
        Fail( "invalid pass or resource in read declaration" );
        return;
    }

    Pass&     PassData     = *m_Passes[PassId];
    Resource& ResourceData = *m_GraphResources[ResourceHandle.Index];

    if( ResourceData.LastReadPass == PassId )
    {
        PassData.Accesses[ResourceData.LastReadAccessIndex].Flags |= Access;
        return;
    }

    if( !PassDeclaresRead( PassId, ResourceHandle ) )
    {
        PassData.Reads.Append() = ResourceHandle.Index;
    }

    for( s32 i = 0; i < PassData.Accesses.GetCount(); ++i )
    {
        if( PassData.Accesses[i].Resource == ResourceHandle.Index )
        {
            PassData.Accesses[i].Flags |= Access;
            ResourceData.LastReadPass        = PassId;
            ResourceData.LastReadAccessIndex = i;
            return;
        }
    }

    ResourceData.LastReadAccessIndex = PassData.Accesses.GetCount();
    PassData.Accesses.Append() = PassAccess( ResourceHandle.Index, Access );
    ResourceData.LastReadPass = PassId;
}

//=========================================================================

void FrameGraphBuilder::AddWriteAccess( RenderPassId PassId, RenderResourceHandle ResourceHandle, u32 Access )
{
    if( ( PassId >= static_cast<u32>( m_Passes.GetCount() ) ) || !IsHandleValid( ResourceHandle ) )
    {
        Fail( "invalid pass or resource in write declaration" );
        return;
    }

    Pass& PassData = *m_Passes[PassId];

    if( !PassDeclaresWrite( PassId, ResourceHandle ) )
    {
        PassData.Writes.Append() = ResourceHandle.Index;
    }

    if( !Access )
    {
        return;
    }

    for( s32 i = 0; i < PassData.Accesses.GetCount(); ++i )
    {
        if( PassData.Accesses[i].Resource == ResourceHandle.Index )
        {
            PassData.Accesses[i].Flags |= Access;
            return;
        }
    }

    PassData.Accesses.Append() = PassAccess( ResourceHandle.Index, Access );
}

//=========================================================================

void FrameGraphBuilder::Read( RenderPassId PassId, RenderResourceHandle ResourceHandle )
{
    if( !CanDeclare( "Read" ) )
    {
        return;
    }

    if( ( PassId >= static_cast<u32>( m_Passes.GetCount() ) ) || !IsHandleValid( ResourceHandle ) )
    {
        Fail( "invalid pass or resource in read declaration" );
        return;
    }

    Read( PassId, ResourceHandle, RENDER_RESOURCE_ACCESS_SAMPLED_READ );
}

//=========================================================================

void FrameGraphBuilder::Read( RenderPassId PassId, RenderResourceHandle ResourceHandle, u32 Access )
{
    if( !CanDeclare( "Read" ) )
    {
        return;
    }

    const u32 ReadAccess = RENDER_RESOURCE_ACCESS_SAMPLED_READ | RENDER_RESOURCE_ACCESS_STORAGE_READ |
                          RENDER_RESOURCE_ACCESS_VERTEX_READ | RENDER_RESOURCE_ACCESS_INDEX_READ |
                          RENDER_RESOURCE_ACCESS_INDIRECT_READ;
    if( !Access || ( Access & ~ReadAccess ) )
    {
        Fail( "invalid read access" );
        return;
    }

    AddReadAccess( PassId, ResourceHandle, Access );
}

//=========================================================================

void FrameGraphBuilder::Read( RenderPassId PassId, const rbuffer& Buffer, u32 Access )
{
    Read( PassId, ImportExternal( Buffer, Buffer.Desc.pDebugName ? Buffer.Desc.pDebugName : "ExternalBuffer" ), Access );
}

//=========================================================================

void FrameGraphBuilder::Read( RenderPassId PassId, const shader_resource* pResource, u32 Access )
{
    if( pResource && *pResource )
    {
        Read( PassId, ImportExternal( *pResource, "ShaderResource" ), Access );
    }
}

//=========================================================================

u32 FrameGraphBuilder::GetPassCount( void ) const
{
    return static_cast<u32>( m_Passes.GetCount() );
}

//=========================================================================

void FrameGraphBuilder::SetPassResourceValidation( RenderPassId PassId, xbool Enabled )
{
    if( !CanDeclare( "SetPassResourceValidation" ) || PassId >= GetPassCount() )
    {
        Fail( "invalid pass resource validation declaration" );
        return;
    }
    m_Passes[PassId]->Desc.ValidateResources = Enabled;
}

//=========================================================================

void FrameGraphBuilder::Write( RenderPassId PassId, RenderResourceHandle ResourceHandle )
{
    if( !CanDeclare( "Write" ) )
    {
        return;
    }

    if( ( PassId >= static_cast<u32>( m_Passes.GetCount() ) ) || !IsHandleValid( ResourceHandle ) )
    {
        Fail( "invalid pass or resource in write declaration" );
        return;
    }

    AddWriteAccess( PassId, ResourceHandle, 0 );
}

//=========================================================================

xbool FrameGraphBuilder::AddDependency( Pass& Target, RenderPassId Dependency )
{
    for( s32 i = 0; i < Target.Dependencies.GetCount(); ++i )
    {
        if( Target.Dependencies[i] == Dependency )
        {
            return TRUE;
        }
    }

    Target.Dependencies.Append() = Dependency;
    return TRUE;
}

//=========================================================================

void FrameGraphBuilder::KeepAlive( RenderPassId PassId )
{
    if( !CanDeclare( "KeepAlive" ) )
    {
        return;
    }

    if( PassId >= static_cast<u32>( m_Passes.GetCount() ) )
    {
        Fail( "invalid pass in keep-alive declaration" );
        return;
    }

    m_Passes[PassId]->KeepAlive = TRUE;
}

//=========================================================================

void FrameGraphBuilder::SetOutput( RenderResourceHandle Resource )
{
    if( !CanDeclare( "SetOutput" ) )
    {
        return;
    }

    if( !IsHandleValid( Resource ) )
    {
        Fail( "invalid final output resource" );
        return;
    }

    m_Output = Resource;
}

//=========================================================================
//  DIAGNOSTICS
//=========================================================================

void FrameGraphBuilder::Dump( void ) const
{
#ifndef X_RETAIL
    RenderView const* pView = m_pFrame ? m_pFrame->GetView( 0 ) : NULL;
    x_DebugMsg(
        "FrameGraph: scope=%d view=%llu reset=%llu resources=%d passes=%d compiled=%d executed=%d output=%u:%u\n",
        m_pFrame ? m_pFrame->Desc.ScopeId : -1, static_cast<unsigned long long>( pView ? pView->ViewId : 0 ),
        static_cast<unsigned long long>( m_pFrame ? m_pFrame->Desc.TemporalResetToken : 0 ),
        m_GraphResources.GetCount(), m_Passes.GetCount(), m_isCompiled, m_isExecuted, m_Output.Index,
        m_Output.Generation );

    for( s32 i = 0; i < m_GraphResources.GetCount(); ++i )
    {
        const Resource& ResourceData = *m_GraphResources[i];
        const char*     pLifetime    = ResourceData.Desc.Lifetime == RENDER_RESOURCE_TRANSIENT    ? "transient"
                                       : ResourceData.Desc.Lifetime == RENDER_RESOURCE_PERSISTENT ? "persistent"
                                                                                                  : "external";
        if( ResourceData.Desc.IsBuffer() )
        {
            x_DebugMsg( "  buffer[%d] %s lifetime=%s size=%u stride=%u usage=0x%x live=%d interval=%u..%u\n",
                        i, ResourceData.Desc.pDebugName, pLifetime, ResourceData.Desc.Buffer.Size,
                        ResourceData.Desc.Buffer.Stride, ResourceData.Desc.Buffer.UsageFlags,
                        ResourceData.FirstUse != 0xffffffffu, ResourceData.FirstUse, ResourceData.LastUse );
            continue;
        }

        x_DebugMsg( "  resource[%d] %s lifetime=%s extent=%ux%u format=%d mips=%u samples=%u usage=0x%x key=0x%llx "
                    "live=%d interval=%s%u..%u physical=%s%u:%u\n",
                    i, ResourceData.Desc.pDebugName ? ResourceData.Desc.pDebugName : "<unnamed>", pLifetime,
                    ResourceData.Desc.Extent.Width, ResourceData.Desc.Extent.Height, ResourceData.Desc.Format,
                    ResourceData.Desc.MipCount, ResourceData.Desc.SampleCount, ResourceData.Desc.Usage,
                    static_cast<unsigned long long>( ResourceData.Desc.PersistentKey ),
                    ResourceData.FirstUse != 0xffffffffu,
                    ResourceData.FirstUse == 0xffffffffu ? "culled (no live use) " : "",
                    ResourceData.FirstUse == 0xffffffffu ? 0 : ResourceData.FirstUse,
                    ResourceData.FirstUse == 0xffffffffu ? 0 : ResourceData.LastUse,
                    ResourceData.Physical.IsValid() ? "" : "none ", ResourceData.Physical.Index,
                    ResourceData.Physical.Generation );
    }

    for( s32 i = 0; i < m_Passes.GetCount(); ++i )
    {
        const Pass& PassData = *m_Passes[i];
        x_DebugMsg( "  pass[%d] %s type=%d live=%d optional=%d reads=%d writes=%d dependencies=%d "
                    "load/store=(%d,%d)/(%d,%d) resolve=%d/%d compositePipelines=%u cull=%s\n",
                    i, PassData.Desc.pDebugName ? PassData.Desc.pDebugName : "<unnamed>", PassData.Desc.Type,
                    PassData.Live, PassData.Desc.bOptional, PassData.Reads.GetCount(), PassData.Writes.GetCount(),
                    PassData.Dependencies.GetCount(), PassData.Desc.ColorLoadOp, PassData.Desc.ColorStoreOp,
                    PassData.Desc.DepthLoadOp, PassData.Desc.DepthStoreOp, PassData.Desc.ColorResolve.IsValid(),
                    PassData.Desc.Color1Resolve.IsValid(), PassData.Desc.CompositePipelineCount,
                    PassData.Live ? "<none>" : "not reachable from output/keep-alive roots" );

        if( PassData.Dependencies.GetCount() )
        {
            x_DebugMsg( "    dependencies:" );

            for( s32 D = 0; D < PassData.Dependencies.GetCount(); ++D )
            {
                const Pass& Dependency = *m_Passes[PassData.Dependencies[D]];
                x_DebugMsg( " %s", Dependency.Desc.pDebugName ? Dependency.Desc.pDebugName : "<unnamed>" );
            }

            x_DebugMsg( "\n" );
        }
    }

    if( m_ExecutionOrder.GetCount() )
    {
        x_DebugMsg( "  execution order:" );

        for( s32 i = 0; i < m_ExecutionOrder.GetCount(); ++i )
        {
            x_DebugMsg( " %s", m_Passes[m_ExecutionOrder[i]]->Desc.pDebugName );
        }

        x_DebugMsg( "\n" );
    }
#endif
}

//=========================================================================

const rtarget* FrameGraphBuilder::Get( RenderResourceHandle ResourceHandle ) const
{
    if( !IsHandleValid( ResourceHandle ) )
    {
        return NULL;
    }

    return m_Resources.Get( m_GraphResources[ResourceHandle.Index]->Physical );
}

//=========================================================================

const RenderResourceDesc* FrameGraphBuilder::GetDesc( RenderResourceHandle ResourceHandle ) const
{
    return IsHandleValid( ResourceHandle ) ? &m_GraphResources[ResourceHandle.Index]->Desc : NULL;
}

//=========================================================================

const rbuffer* FrameGraphBuilder::GetBuffer( RenderResourceHandle ResourceHandle ) const
{
    if( !IsHandleValid( ResourceHandle ) )
    {
        return NULL;
    }

    const Resource& ResourceData = *m_GraphResources[ResourceHandle.Index];
    if( !ResourceData.Desc.IsBuffer() )
    {
        return NULL;
    }

    if( ResourceData.Desc.Lifetime == RENDER_RESOURCE_EXTERNAL )
    {
        const rbuffer* pBuffer = ResourceData.pExternalBuffer;
        return pBuffer && pBuffer->pBackend == ResourceData.pExternalBufferBackend &&
               pBuffer->Desc.Size == ResourceData.Desc.Buffer.Size &&
               pBuffer->Desc.Stride == ResourceData.Desc.Buffer.Stride &&
               pBuffer->Desc.UsageFlags == ResourceData.Desc.Buffer.UsageFlags ? pBuffer : NULL;
    }

    return m_Resources.GetBuffer( ResourceData.Physical );
}

//=========================================================================

const shader_resource* FrameGraphBuilder::GetShaderResource( RenderResourceHandle ResourceHandle ) const
{
    if( !IsHandleValid( ResourceHandle ) )
    {
        return NULL;
    }

    const Resource& Data = *m_GraphResources[ResourceHandle.Index];
    if( Data.pExternalTexture )
    {
        const vram_texture& Texture = *Data.pExternalTexture;
        const vram_texture_desc& Desc = Data.ExternalTextureDesc;
        if( Texture.pBackend != Data.pExternalTextureBackend || Texture.Desc.Width != Desc.Width ||
            Texture.Desc.Height != Desc.Height || Texture.Desc.Depth != Desc.Depth ||
            Texture.Desc.Type != Desc.Type || Texture.Desc.Format != Desc.Format ||
            Texture.Desc.MipCount != Desc.MipCount || Texture.Desc.LayerCount != Desc.LayerCount ||
            Texture.Desc.SampleCount != Desc.SampleCount || Texture.Desc.UsageFlags != Desc.UsageFlags )
        {
            return NULL;
        }
        return vram_GetShaderResource( Texture );
    }
    if( Data.Desc.IsBuffer() )
    {
        const rbuffer* pBuffer = GetBuffer( ResourceHandle );
        return pBuffer ? rbuffer_GetResource( *pBuffer ) : NULL;
    }

    const rtarget* pTarget = Get( ResourceHandle );
    return pTarget ? rtarget_GetShaderResource( *pTarget ) : NULL;
}

//=========================================================================

rtarget* FrameGraphBuilder::GetMutable( RenderResourceHandle ResourceHandle )
{
    if( !IsHandleValid( ResourceHandle ) )
    {
        return NULL;
    }

    return m_Resources.GetMutable( m_GraphResources[ResourceHandle.Index]->Physical );
}

//=========================================================================

void FrameGraphBuilder::Fail( const char* pError )
{
    if( !m_pLastError )
    {
        m_pLastError = pError;
        RENDER_THROW( "FrameGraph", "scope=%d pass=%s resource=%s: %s", m_pFrame ? m_pFrame->Desc.ScopeId : -1,
                      m_pFailurePass ? m_pFailurePass : "<declaration>",
                      m_pFailureResource ? m_pFailureResource : "<none>", pError );
    }
}

} // namespace render
