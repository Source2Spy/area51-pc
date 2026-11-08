//=========================================================================
//
//  FrameGraph.hpp
//
//=========================================================================

#ifndef FRAME_GRAPH_HPP
#define FRAME_GRAPH_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "e_RenderDraw.hpp"
#include "e_RenderTarget.hpp"
#include "e_Composite.hpp"
#include "e_RenderBuffer.hpp"
#include "e_VRAM.hpp"
#include "x_array.hpp"
#include "x_time.hpp"
#include "x_math.hpp"
#include "RenderFailure.hpp"
#include <new>
#include "../Pipeline/RenderFeature.hpp"
#include "../RenderFog.hpp"
#include "../RenderScale.hpp"
#include "../RenderScope.hpp"
#include "e_View.hpp"

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class FrameResourcePool;
class FrameGraphBuilder;

//=========================================================================
//  TYPES
//=========================================================================

struct RenderExtent
{
    u32 Width;
    u32 Height;

    RenderExtent( void );
    RenderExtent( u32 WidthIn, u32 HeightIn );

    xbool IsValid( void ) const;
};

//-------------------------------------------------------------------------

struct RenderResourceHandle
{
    u32 Index;
    u32 Generation;

    RenderResourceHandle( void );
    RenderResourceHandle( u32 IndexIn, u32 GenerationIn );

    xbool IsValid( void ) const;
};

//-------------------------------------------------------------------------

struct RenderTargetBinding
{
    const rtarget* pColor;
    const rtarget* pDepth;
    rtarget_format ColorFormat;
    rtarget_format DepthFormat;
    RenderExtent   Extent;
    u32            SampleCount;
    u32            ArrayLayers;
    u32            ViewMask;

    RenderTargetBinding( void );

    // Color uses output extent; depth may use the scaled scene extent.
    xbool IsValid( void ) const;
};

//-------------------------------------------------------------------------

struct RenderView
{
    matrix4        View;
    matrix4        Projection;
    ::view         SourceView;
    rdraw_viewport Viewport;
    u32            ViewIndex;
    u32            LayerIndex;
    u64            ViewId;
    xbool          HasSourceView;

    RenderView( void );

    const ::view* GetSourceView( void ) const;
};

//-------------------------------------------------------------------------

struct RenderViewFamily
{
    RenderView* pViews;
    u32         ViewCount;
    u32         ViewMask;

    RenderViewFamily( void );
};

//-------------------------------------------------------------------------

struct RenderFormatPolicy
{
    rtarget_format SceneColorFormat;
    rtarget_format BloomFormat;
    xbool          BloomCompositeToLDR;

    RenderFormatPolicy( void );
};

//-------------------------------------------------------------------------

struct RenderCapabilities
{
    u32   SupportedSceneTextures;
    u32   MaxColorAttachments;
    xbool SupportsMultiview;

    RenderCapabilities( void );
};

//-------------------------------------------------------------------------

struct RenderQualityProfile
{
    RenderFormatPolicy Formats;
    f32                RenderScale;
    u32                SampleCount;

    RenderQualityProfile( void );
    static RenderQualityProfile PlatformDefault( void );
};

// Check sample-count support on the active GPU.

//-------------------------------------------------------------------------

enum SceneTextureFlags
{
    SCENE_TEXTURE_NONE           = 0,
    SCENE_TEXTURE_DEPTH          = 1u << 0,
    SCENE_TEXTURE_DEPTH_SAMPLES  = 1u << 1,
    SCENE_TEXTURE_HARDWARE_DEPTH = 1u << 2,
    SCENE_TEXTURE_NORMALS        = 1u << 3,
};

//-------------------------------------------------------------------------

struct RenderFrameDesc
{
    RenderViewFamily    Views;
    RenderTargetBinding Output;
    f32                 RenderScale;
    u32                 SampleCount;
    u32                 SceneTextureRequirements;
    s32                 ScopeId;
    u64                 TemporalResetToken;
    render_view_purpose Purpose;
    xbool               EnableShadows;
    xbool               EnablePostProcessing;
    // Disable history for one-shot views without a stable ViewId.
    xbool EnableTemporalHistory;

    RenderFormatPolicy Formats;
    RenderCapabilities Capabilities;

    RenderFrameDesc( void );
};

//-------------------------------------------------------------------------

enum RenderResourceLifetime
{
    RENDER_RESOURCE_TRANSIENT,
    RENDER_RESOURCE_PERSISTENT,
    RENDER_RESOURCE_EXTERNAL,
};

//-------------------------------------------------------------------------

enum RenderResourceUsage
{
    RENDER_RESOURCE_USAGE_NONE               = 0,
    RENDER_RESOURCE_USAGE_SAMPLED            = 1u << 0,
    RENDER_RESOURCE_USAGE_COLOR              = 1u << 1,
    RENDER_RESOURCE_USAGE_DEPTH              = 1u << 2,
    RENDER_RESOURCE_USAGE_COPY_SRC           = 1u << 3,
    RENDER_RESOURCE_USAGE_COPY_DST           = 1u << 4,
    RENDER_RESOURCE_USAGE_STORAGE_READ       = 1u << 5,
    RENDER_RESOURCE_USAGE_STORAGE_WRITE      = 1u << 6,
    RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE = 1u << 7,
};

//-------------------------------------------------------------------------

// Track pass access separately from resource capabilities.
enum RenderResourceAccess
{
    RENDER_RESOURCE_ACCESS_SAMPLED_READ       = 1u << 0,
    RENDER_RESOURCE_ACCESS_COPY_SOURCE        = 1u << 1,
    RENDER_RESOURCE_ACCESS_COLOR_WRITE        = 1u << 2,
    RENDER_RESOURCE_ACCESS_DEPTH_WRITE        = 1u << 3,
    RENDER_RESOURCE_ACCESS_COPY_DEST          = 1u << 4,
    RENDER_RESOURCE_ACCESS_RESOLVE_SOURCE     = 1u << 5,
    RENDER_RESOURCE_ACCESS_RESOLVE_DEST       = 1u << 6,
    RENDER_RESOURCE_ACCESS_STORAGE_READ       = 1u << 7,
    RENDER_RESOURCE_ACCESS_STORAGE_WRITE      = 1u << 8,
    RENDER_RESOURCE_ACCESS_VERTEX_READ        = 1u << 9,
    RENDER_RESOURCE_ACCESS_INDEX_READ         = 1u << 10,
    RENDER_RESOURCE_ACCESS_INDIRECT_READ      = 1u << 11,
    RENDER_RESOURCE_ACCESS_STORAGE_READ_WRITE = 1u << 12,
    RENDER_RESOURCE_ACCESS_PRESERVE           = 1u << 13,
};

//-------------------------------------------------------------------------

enum RenderResourceType
{
    RENDER_RESOURCE_TEXTURE,
    RENDER_RESOURCE_BUFFER,
};

//-------------------------------------------------------------------------

struct RenderResourceDesc
{
    RenderResourceType     Type;
    rbuffer_desc           Buffer;
    RenderExtent           Extent;
    rtarget_format         Format;
    u32                    MipCount;
    u32                    SampleCount;
    u32                    ArrayLayers;
    u32                    ViewMask;
    u32                    Usage;
    RenderResourceLifetime Lifetime;
    u64                    PersistentKey;
    // Retained attachments use one backing texture across ordered submissions.
    xbool       PreserveContents;
    f32         ClearColor[4];
    f32         ClearDepth;
    u8          ClearStencil;
    const char* pDebugName;

    RenderResourceDesc( void );

    xbool IsBuffer( void ) const
    {
        return Type == RENDER_RESOURCE_BUFFER;
    }
};

//-------------------------------------------------------------------------

struct RenderFrameContext
{

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    RenderFrameDesc              Desc;
    FrameResourcePool*           pResources;
    FrameGraphBuilder*           pGraph;
    RenderResourceHandle         SceneColor;
    RenderResourceHandle         ScenePostColor;
    RenderResourceHandle         SceneColorMSAA;
    xarray<RenderResourceHandle> SceneColorCopies;
    RenderResourceHandle         SceneDepth;
    RenderResourceHandle         SceneDepthTarget;
    RenderResourceHandle         SceneDepthMSAA;
    RenderResourceHandle         SceneAmbientVisibility;
    RenderResourceHandle         SceneNormals;
    RenderResourceHandle         FirstPersonDepth;
    RenderResourceHandle         FirstPersonLinearDepth;
    RenderResourceHandle         FirstPersonNormals;
    RenderResourceHandle         FirstPersonAmbientVisibility;
    RenderResourceHandle         AntiAliasingSource;
    RenderResourceHandle         FinalColor;
    RenderResourceHandle         OutputColor;
    // Set after preparation so all features see the same scene state.
    xbool SceneRendered;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderFrameContext( void );

    const rtarget*       Get( RenderResourceHandle Resource ) const;
    RenderResourceHandle GetSceneColorTarget( void ) const;
    RenderResourceHandle GetSceneDepthTarget( void ) const;
    RenderResourceHandle GetSceneLinearDepthTarget( void ) const;
    xbool                GetViewport( rdraw_viewport& Viewport, const rtarget& Target, u32 ViewIndex ) const;
    const RenderView*    GetView( u32 ViewIndex ) const;
};

// Build a stable temporal-resource key for one feature and view.

typedef u32 RenderPassId;

//=========================================================================
//  CONSTANTS
//=========================================================================

static const u32 INVALID_RENDER_PASS = 0xffffffffu;

static const u32 RENDER_PASS_MAX_COMPOSITE_PIPELINES = 4;

//-------------------------------------------------------------------------

enum RenderContributionFlags
{
    RENDER_CONTRIBUTION_NONE           = 0,
    RENDER_CONTRIBUTION_SCENE_COLOR    = 1u << 0,
    RENDER_CONTRIBUTION_BLOOM          = 1u << 1,
    RENDER_CONTRIBUTION_DISTORTION     = 1u << 2,
    RENDER_CONTRIBUTION_SHADOW         = 1u << 3,
    RENDER_CONTRIBUTION_CUSTOM_FEATURE = 1u << 4,
};

typedef u32 RenderContributionMask;

//-------------------------------------------------------------------------

enum RenderPassType
{
    RENDER_PASS_GRAPHICS,
    RENDER_PASS_COMPUTE,
    RENDER_PASS_TRANSFER,
    RENDER_PASS_COPY,
    RENDER_PASS_PRESENT,
};

//-------------------------------------------------------------------------

struct RenderComputeBinding
{
    RenderResourceHandle Resource;
    u32                  MipLevel;
    u32                  Layer;
    xbool                ReadWrite;
    xbool                PreserveContents;
    xbool                Cycle;

    RenderComputeBinding( void )
        : MipLevel( 0 ),
          Layer( 0 ),
          ReadWrite( FALSE ),
          PreserveContents( FALSE ),
          Cycle( FALSE )
    {
    }
};

//-------------------------------------------------------------------------

struct RenderPassDesc
{
    const char*          pDebugName;
    RenderPassType       Type;
    const shader*        pComputeShader;
    RenderComputeBinding ComputeTextures[8];
    RenderComputeBinding ComputeBuffers[8];
    u32                  ComputeTextureCount;
    u32                  ComputeBufferCount;
    RenderResourceHandle Color;
    RenderResourceHandle Color1;
    RenderResourceHandle Color2;
    RenderResourceHandle ColorResolve;
    RenderResourceHandle Color1Resolve;
    RenderResourceHandle Color2Resolve;
    RenderResourceHandle Depth;
    RenderResourceHandle Source;
    RenderResourceHandle Destination;
    rtarget_load_op      ColorLoadOp;
    rtarget_store_op     ColorStoreOp;
    rtarget_load_op      DepthLoadOp;
    rtarget_store_op     DepthStoreOp;
    u32                  ViewMask;
    u32                  ViewIndex;
    // Optional passes may fail only when they have no graph writes.
    xbool bOptional;
    xbool ValidateResources;
    u32   CompositePipelineCount;

    //-------------------------------------------------------------------------

    struct CompositePipelineRequest
    {
        composite_blend_mode Blend;
        const shader*        pPixelShader;

        CompositePipelineRequest( void );
    } CompositePipelines[RENDER_PASS_MAX_COMPOSITE_PIPELINES];

    RenderPassDesc( void );
};

//-------------------------------------------------------------------------

class RenderPassContext
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    const rtarget*            Read( RenderResourceHandle Resource ) const;
    const rtarget*            Write( RenderResourceHandle Resource ) const;
    const rbuffer*            ReadBuffer( RenderResourceHandle Resource ) const;
    const shader_resource*    ReadStorageTexture( RenderResourceHandle Resource ) const;
    xbool                     Dispatch( u32 GroupCountX, u32 GroupCountY = 1, u32 GroupCountZ = 1 );
    xbool                     DispatchIndirect( RenderResourceHandle Resource, u32 Offset = 0 );
    rtarget*                  WriteMutable( RenderResourceHandle Resource ) const;
    xbool                     Copy( RenderResourceHandle Destination, RenderResourceHandle Source ) const;
    xbool CopyRegion( RenderResourceHandle Destination,
                      RenderResourceHandle Source,
                      u32                  SourceX,
                      u32                  SourceY,
                      u32                  Width,
                      u32                  Height ) const;
    const RenderFrameContext& GetFrame( void ) const;

    const RenderPassDesc& GetPass( void ) const;

    const RenderView* GetView( void ) const;

private:

    friend class FrameGraphBuilder;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderPassContext( FrameGraphBuilder&        Graph,
                       const RenderFrameContext& Frame,
                       const RenderPassDesc&     Pass,
                       RenderPassId              PassId );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    FrameGraphBuilder&        m_Graph;
    const RenderFrameContext& m_Frame;
    const RenderPassDesc&     m_Pass;
    RenderPassId              m_PassId;
    xbool                     m_isDispatched;
};

//-------------------------------------------------------------------------

class RenderPassPayload
{
public:

    virtual ~RenderPassPayload( void );
    virtual xbool Execute( RenderPassContext& Context ) = 0;
};

//-------------------------------------------------------------------------

template <typename TPayload>
struct RenderTypedPassPayload : public RenderPassPayload
{
    typedef xbool ( *ExecuteFn )( RenderPassContext& Context, TPayload Payload );

    ExecuteFn Callback;
    TPayload  Payload;

    RenderTypedPassPayload( ExecuteFn ExecuteIn, const TPayload& PayloadIn );

    virtual xbool Execute( RenderPassContext& Context );
};

//=========================================================================

class FrameResourcePool
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    FrameResourcePool( void );
    ~FrameResourcePool( void );

    void Init( void );
    void Kill( void );
    void BeginEngineFrame( u32 FrameSlot, u64 FrameSerial, u32 FrameSlotCount );
    void BeginScope( void );
    void Reset( void );

    u64   GetResetEpoch( void ) const;
    xbool HasPersistentResource( u64 Key ) const;

    RenderResourceHandle Acquire( const RenderResourceDesc& Desc );
    // COLOR/DEPTH/SAMPLED are inferred from Target. COPY_* capabilities are
    // opt-in and must be supplied explicitly when the caller guarantees them.
    RenderResourceHandle ImportExternal( const rtarget& Target,
                                         const char*    pDebugName,
                                         u32            ExternalUsage = RENDER_RESOURCE_USAGE_NONE );
    const rtarget*       Get( RenderResourceHandle Resource ) const;
    const rbuffer*       GetBuffer( RenderResourceHandle Resource ) const;
    rtarget*             GetMutable( RenderResourceHandle Resource );

private:

    friend class FrameGraphBuilder;

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct Resource;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool MarkExternalMutable( RenderResourceHandle Resource, rtarget& Target );
    u32   AddResource( Resource* pResource );
    void  RebuildDescriptorLookup( void );
    s32   FindPersistentResource( u64 Key ) const;
    s32   FindReusableDescriptor( const RenderResourceDesc& Desc ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<Resource*> m_Resources;
    xarray<Resource*> m_FreeResources;
    xarray<s32>       m_FreeResourceSlots;
    xarray<s32>       m_DescriptorLookup;
    xarray<s32>       m_PersistentLookup;
    u32               m_DescriptorLookupMask;
    u32               m_nDescriptorLookups;
    u32               m_Generation;
    u64               m_ResetEpoch;
    u32               m_ActiveFrameSlot;
    u32               m_nFrameSlots;
    u64               m_ActiveFrameSerial;
    xtick             m_ActiveFrameTime;
    u64               m_LastFrameSerialBySlot[3];
    xbool             m_isInitialized;
};

//=========================================================================

class FrameGraphBuilder
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    explicit FrameGraphBuilder( FrameResourcePool& Resources );
    ~FrameGraphBuilder( void );

    void BeginFrame( RenderFrameContext& Frame );
    void Reset( void );

    RenderResourceHandle CreateResource( const RenderResourceDesc& Desc );
    xbool                RetainResource( RenderResourceHandle Resource, u64 PersistentKey );
    // COLOR/DEPTH/SAMPLED are inferred from Target. COPY_* capabilities are
    // opt-in and must be supplied explicitly when the caller guarantees them.
    RenderResourceHandle ImportExternal( const rtarget& Target,
                                         const char*    pDebugName,
                                         u32            ExternalUsage = RENDER_RESOURCE_USAGE_NONE );
    RenderResourceHandle ImportExternalMutable( rtarget&    Target,
                                                const char* pDebugName,
                                                u32         ExternalUsage = RENDER_RESOURCE_USAGE_NONE );
    RenderResourceHandle ImportExternal( const rbuffer& Buffer, const char* pDebugName,
                                         xbool HasInitialContents = TRUE );
    RenderResourceHandle ImportExternal( const vram_texture& Texture, const char* pDebugName );
    RenderResourceHandle ImportExternal( const shader_resource& Resource, const char* pDebugName );

    xbool QueueUpload( rbuffer& Buffer, const rbuffer_upload_desc& Upload );
    xbool QueueUpload( rbuffer& Buffer, const void* pData, u32 Size, u32 Offset = 0, xbool Cycle = FALSE );
    xbool QueueUploadBatch( const rbuffer_upload_batch_item* pItems, u32 Count );
    xbool QueueUpload( vram_texture& Texture, const vram_texture_upload_desc& Upload, xbool* pPending = NULL );
    xbool QueueUploadBatch( const vram_texture_upload_batch_item* pItems, u32 Count,
                            xbool* const* ppPending = NULL );
    xbool QueueGenerateMipmaps( vram_texture& Texture );

    template <typename TPayload>
    RenderPassId AddPass( const RenderPassDesc&                                Desc,
                              typename RenderTypedPassPayload<TPayload>::ExecuteFn Execute,
                              const TPayload&                                      Payload );
    xbool DeclareCompositePipeline( RenderPassId Pass, composite_blend_mode Blend, const shader* pPixelShader = NULL );
    void  Read( RenderPassId Pass, RenderResourceHandle Resource );
    void  Read( RenderPassId Pass, RenderResourceHandle Resource, u32 Access );
    void  Read( RenderPassId Pass, const rbuffer& Buffer, u32 Access );
    void  Read( RenderPassId Pass, const shader_resource* pResource,
                u32 Access = RENDER_RESOURCE_ACCESS_SAMPLED_READ );
    u32   GetPassCount( void ) const;
    void  SetPassResourceValidation( RenderPassId Pass, xbool Enabled );
    void  Write( RenderPassId Pass, RenderResourceHandle Resource );
    void  KeepAlive( RenderPassId Pass );
    void  SetOutput( RenderResourceHandle Resource );

    xbool                     Compile( void );
    xbool                     Execute( void );
    void                      Dump( void ) const;
    const rtarget*            Get( RenderResourceHandle Resource ) const;
    const rbuffer*            GetBuffer( RenderResourceHandle Resource ) const;
    const shader_resource*    GetShaderResource( RenderResourceHandle Resource ) const;
    const RenderResourceDesc* GetDesc( RenderResourceHandle Resource ) const;

    const char* GetLastError( void ) const;

private:

    friend class RenderPassContext;

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct Resource;
    struct PassAccess;
    struct Pass;
    struct TransferData;

    enum ExternalImportKind
    {
        EXTERNAL_IMPORT_TARGET,
        EXTERNAL_IMPORT_BUFFER,
        EXTERNAL_IMPORT_TEXTURE
    };

    struct ExternalImport
    {
        const void* pBackend;
        Resource*   pResource;
        u32         Kind;
    };

    struct ResourceValidation
    {
        shader_resource_backend* pResource;
        u32                      Access;
    };

    //-------------------------------------------------------------------------

    struct CompositeRequirement
    {
        shader_backend*      pPixelShader;
        composite_blend_mode Blend;
        rtarget_format       ColorFormat;
        rtarget_format       DepthFormat;
        u32                  Samples;
        render_pipeline*     pPipeline;
    };

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct TransientPhysicalUse
    {
        RenderResourceHandle Physical;
        RenderResourceDesc   Desc;
        u32                  LastUse;
        u64                  CompatibilityKey;
        s32                  NextWithKey;
    };

    //-------------------------------------------------------------------------

    enum
    {
        PAYLOAD_STORAGE_SIZE = 256
    };

    typedef void ( *PayloadDestroyFn )( void* );

    //-------------------------------------------------------------------------

    struct PayloadBlock
    {
        union
        {
            u64 Alignment;
            u8  Storage[PAYLOAD_STORAGE_SIZE];
        };

        RenderPassPayload* pPayload;
        PayloadDestroyFn   Destroy;

        PayloadBlock( void );

        void Release( void );
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    template <typename TPayload>
    static void DestroyTypedPayload( void* pStorage );

    template <typename TPayload>
    RenderTypedPassPayload<TPayload>* CreateTypedPayload(
        typename RenderTypedPassPayload<TPayload>::ExecuteFn Execute,
        const TPayload& Payload );

    void ReleasePayload( RenderPassPayload* pPayload );

    xbool            IsHandleValid( RenderResourceHandle Resource ) const;
    Resource*        AllocateResource( void );
    Resource*        FindExternalImport( const void* pBackend, u32 Kind ) const;
    void             IndexExternalImport( Resource& ResourceData );
    xbool            PassDeclaresRead( RenderPassId Pass, RenderResourceHandle Resource ) const;
    xbool            PassDeclaresWrite( RenderPassId Pass, RenderResourceHandle Resource ) const;
    xbool            PassDeclaresAccess( RenderPassId Pass, RenderResourceHandle Resource, u32 Access ) const;
    RenderPassId     AddPassPayload( const RenderPassDesc& Desc, RenderPassPayload* pPayload );
    xbool            AddDependency( Pass& Target, RenderPassId Dependency );
    void             AddReadAccess( RenderPassId Pass, RenderResourceHandle Resource, u32 Access );
    void             AddWriteAccess( RenderPassId Pass, RenderResourceHandle Resource, u32 Access );
    xbool            CanDeclare( const char* pOperation );
    static s32       CompareResourceFirstUse( const void* pA, const void* pB );
    static xbool     StoresResource( const Pass& Pass, RenderResourceHandle Resource );
    xbool            CanMergePasses( const Pass& Previous, const Pass& Next ) const;
    rtarget_store_op GetAttachmentStoreOp( RenderResourceHandle Resource,
                                           rtarget_store_op     StoreOp,
                                           u32                  LastPassIndex ) const;
    xbool            BeginPass( const Pass& PassToBegin, u32 LastPassIndex );
    xbool            PrewarmCompositePipelines( void );
    rtarget*         GetMutable( RenderResourceHandle Resource );
    void             Fail( const char* pError );
    void             BeginResourceValidation( const Pass& PassData );
    static xbool     ValidateResourceAccess( const shader_resource& Resource,
                                             shader_resource_access Access, void* pContext );
    static xbool     ExecuteTransfer( RenderPassContext& Context, TransferData Data );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<CompositeRequirement> m_CompositeRequirements;
    FrameResourcePool&           m_Resources;
    RenderFrameContext*          m_pFrame;
    xarray<ExternalImport>       m_ExternalImports;
    u32                         m_ExternalImportCount;
    xarray<Resource*>            m_GraphResources;
    xarray<Resource*>            m_LiveResources;
    xarray<TransientPhysicalUse> m_TransientPhysicalUses;
    xarray<s32>                  m_TransientAliasBuckets;
    xarray<Pass*>                m_Passes;
    xarray<Resource*>            m_FreeResources;
    xarray<Pass*>                m_FreePasses;
    xarray<PayloadBlock>         m_PayloadBlocks;
    xarray<RenderPassId>         m_ExecutionOrder;
    xarray<RenderPassId>         m_LiveQueue;
    xarray<RenderPassId>         m_ReadyQueue;
    xarray<u32>                  m_Incoming;
    xarray<ResourceValidation>   m_ResourceValidation;
    xarray<xbool*>               m_PendingTextureUploads;
    RenderResourceHandle         m_Output;
    u32                          m_Generation;
    const char*                  m_pLastError;
    const char*                  m_pFailurePass;
    const char*                  m_pFailureResource;
    xbool                        m_isCompiled;
    xbool                        m_isExecuted;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool IsSampleCountSupportedForDevice( u32 SampleCount );
u32   SelectSupportedSampleCount( u32 RequestedSampleCount );
u64   RenderHistoryKey( u64 FeatureKey, const RenderFrameContext& Frame );
u64   RenderHistoryKeyForIdentity( u64 FeatureKey, s32 ScopeId, u64 ViewId );

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
RenderExtent::RenderExtent( void )
    : Width( 0 ),
      Height( 0 )
{
}

//=========================================================================

inline
RenderExtent::RenderExtent( u32 WidthIn, u32 HeightIn )
    : Width( WidthIn ),
      Height( HeightIn )
{
}

//=========================================================================

inline
xbool RenderExtent::IsValid( void ) const
{
    return ( Width > 0 ) && ( Height > 0 );
}

//=========================================================================

inline
RenderResourceHandle::RenderResourceHandle( void )
    : Index( 0xffffffffu ),
      Generation( 0 )
{
}

//=========================================================================

inline
RenderResourceHandle::RenderResourceHandle( u32 IndexIn, u32 GenerationIn )
    : Index( IndexIn ),
      Generation( GenerationIn )
{
}

//=========================================================================

inline
xbool RenderResourceHandle::IsValid( void ) const
{
    return Index != 0xffffffffu;
}

//=========================================================================

inline
RenderTargetBinding::RenderTargetBinding( void )
    : pColor( NULL ),
      pDepth( NULL ),
      ColorFormat( RTARGET_FORMAT_COUNT ),
      DepthFormat( RTARGET_FORMAT_COUNT ),
      Extent(),
      SampleCount( 1 ),
      ArrayLayers( 1 ),
      ViewMask( 1 )
{
}

//=========================================================================

inline
RenderView::RenderView( void )
    : View(),
      Projection(),
      SourceView(),
      Viewport(),
      ViewIndex( 0 ),
      LayerIndex( 0 ),
      ViewId( 0 ),
      HasSourceView( FALSE )
{
}

//=========================================================================

inline
const ::view* RenderView::GetSourceView( void ) const
{
    return HasSourceView ? &SourceView : NULL;
}

//=========================================================================

inline
RenderViewFamily::RenderViewFamily( void )
    : pViews( NULL ),
      ViewCount( 0 ),
      ViewMask( 0 )
{
}

//=========================================================================

inline
RenderFrameDesc::RenderFrameDesc( void )
    : Views(),
      Output(),
      RenderScale( RENDER_SCALE_DEFAULT ),
      SampleCount( 1 ),
      SceneTextureRequirements( SCENE_TEXTURE_DEPTH ),
      ScopeId( -1 ),
      TemporalResetToken( 0 ),
      Purpose( RENDER_VIEW_GAMEPLAY ),
      EnableShadows( TRUE ),
      EnablePostProcessing( TRUE ),
      EnableTemporalHistory( TRUE ),
      Formats()
{
}

//=========================================================================

inline
RenderResourceHandle RenderFrameContext::GetSceneColorTarget( void ) const
{
    return SceneColorMSAA.IsValid() ? SceneColorMSAA : SceneColor;
}

//=========================================================================

inline
RenderResourceHandle RenderFrameContext::GetSceneDepthTarget( void ) const
{
    return SceneDepthTarget;
}

//=========================================================================

inline
RenderResourceHandle RenderFrameContext::GetSceneLinearDepthTarget( void ) const
{
    return SceneDepthMSAA.IsValid() ? SceneDepthMSAA : SceneDepth;
}

//=========================================================================

inline
RenderPassDesc::CompositePipelineRequest::CompositePipelineRequest( void )
    : Blend( COMPOSITE_BLEND_COPY ),
      pPixelShader( NULL )
{
}

//=========================================================================

inline
const RenderFrameContext& RenderPassContext::GetFrame( void ) const
{
    return m_Frame;
}

//=========================================================================

inline
const RenderPassDesc& RenderPassContext::GetPass( void ) const
{
    return m_Pass;
}

//=========================================================================

inline
const RenderView* RenderPassContext::GetView( void ) const
{
    return m_Frame.GetView( m_Pass.ViewIndex );
}

//=========================================================================

inline
RenderPassContext::RenderPassContext( FrameGraphBuilder&        Graph,
                                      const RenderFrameContext& Frame,
                                      const RenderPassDesc&     Pass,
                                      RenderPassId              PassId )
    : m_Graph( Graph ),
      m_Frame( Frame ),
      m_Pass( Pass ),
      m_PassId( PassId ),
      m_isDispatched( FALSE )
{
}

//=========================================================================

inline
RenderPassPayload::~RenderPassPayload( void )
{
}

//=========================================================================

template <typename TPayload>
inline
RenderTypedPassPayload<TPayload>::RenderTypedPassPayload( ExecuteFn ExecuteIn, const TPayload& PayloadIn )
    : Callback( ExecuteIn ),
      Payload( PayloadIn )
{
}

//=========================================================================

template <typename TPayload>
inline
xbool RenderTypedPassPayload<TPayload>::Execute( RenderPassContext& Context )
{
    return Callback && Callback( Context, Payload );
}

//=========================================================================

inline
u64 FrameResourcePool::GetResetEpoch( void ) const
{
    return m_ResetEpoch;
}

//=========================================================================

template <typename TPayload>
inline
RenderPassId FrameGraphBuilder::AddPass( const RenderPassDesc&                                Desc,
                                         typename RenderTypedPassPayload<TPayload>::ExecuteFn Execute,
                                         const TPayload&                                      Payload )
{
    if( !Execute )
    {
        Fail( "pass requires an execute callback" );
        return INVALID_RENDER_PASS;
    }

    RenderTypedPassPayload<TPayload>* pPayload = CreateTypedPayload( Execute, Payload );

    if( !pPayload )
    {
        Fail( "failed to allocate typed render pass payload storage" );
        return INVALID_RENDER_PASS;
    }

    const RenderPassId PassData = AddPassPayload( Desc, pPayload );

    if( PassData == INVALID_RENDER_PASS )
    {
        ReleasePayload( pPayload );
        return PassData;
    }

    return PassData;
}

//=========================================================================

inline
const char* FrameGraphBuilder::GetLastError( void ) const
{
    return m_pLastError;
}

//=========================================================================

inline
FrameGraphBuilder::PayloadBlock::PayloadBlock( void )
    : Alignment( 0 ),
      pPayload( NULL ),
      Destroy( NULL )
{
}

//=========================================================================

inline
void FrameGraphBuilder::PayloadBlock::Release( void )
{
    if( pPayload && Destroy )
    {
        Destroy( Storage );
    }

    pPayload = NULL;
    Destroy  = NULL;
}

//=========================================================================

template <typename TPayload>
inline
void FrameGraphBuilder::DestroyTypedPayload( void* pStorage )
{
    typedef RenderTypedPassPayload<TPayload> TypedPayload;
    static_cast<TypedPayload*>( pStorage )->~TypedPayload();
}

//=========================================================================

template <typename TPayload>
inline
RenderTypedPassPayload<TPayload>* FrameGraphBuilder::CreateTypedPayload(
    typename RenderTypedPassPayload<TPayload>::ExecuteFn Execute,
    const TPayload&                                      Payload )
{
    typedef RenderTypedPassPayload<TPayload> TypedPayload;
    static_assert( sizeof( TypedPayload ) <= PAYLOAD_STORAGE_SIZE,
                   "render pass payload exceeds frame graph inline storage" );

    if( m_PayloadBlocks.GetCount() >= m_PayloadBlocks.GetCapacity() )
    {
        return NULL;
    }

    PayloadBlock& Block = m_PayloadBlocks.Append();
    Block.pPayload      = new ( Block.Storage ) TypedPayload( Execute, Payload );
    Block.Destroy       = DestroyTypedPayload<TPayload>;

    return static_cast<TypedPayload*>( Block.pPayload );
}

} // namespace render

//=========================================================================
#endif // FRAME_GRAPH_HPP
//=========================================================================
