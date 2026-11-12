//=========================================================================
//
//  Render.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Font/FontMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "Entropy.hpp"
#include "GeomDraw.hpp"
#include "Render.hpp"
#include "ProjectionTextureRegistry.hpp"
#include "Backend/Resource/GeometryRepository.hpp"
#include "FrameGraph/RenderFailure.hpp"
#include "Pipeline/RenderPipeline.hpp"
#include "../UI/ui_renderer.hpp"

#define RENDER_PRIVATE
#include "MaterialArray.hpp"
#undef RENDER_PRIVATE

//=========================================================================
//  TYPES
//=========================================================================

enum class GeomType
{
    Rigid = 0,
    Skin
};

//-------------------------------------------------------------------------

struct PrivateInst
{
    geom*    m_pGeom;
    GeomType m_Type;
    xhandle  m_GeomHandle;
};

//-------------------------------------------------------------------------

struct PrivateGeom
{
    geom*           m_pGeom;
    GeomType        m_Type;
    s32             m_nReferenceCount;
    xhandle         m_RenderGeomHandle;
    xarray<xhandle> m_MaterialHandles;
};

//=========================================================================
//  GLOBAL DATA
//=========================================================================

//-------------------------------------------------------------------------
//  LOAD STATISTICS
//-------------------------------------------------------------------------

#ifndef X_RETAIL
static s32 s_LoadedGeomCount = 0;
static s32 s_LoadedBoneCount = 0;
static s32 s_LoadedMeshCount = 0;
static s32 s_LoadedSubmeshCount = 0;
static s32 s_LoadedMaterialCount = 0;
static s32 s_LoadedTextureCount = 0;
static s32 s_LoadedUVKeyCount = 0;
static s32 s_LoadedVirtualMaterialCount = 0;
#endif

//-------------------------------------------------------------------------

static const s32 kMaxRegisteredGeoms = 512;
static s32 const kMaxRegisteredInsts = 12800;
static s32 const kMaxRegisteredMaterials = 640;
static s32 const kMaxTexAnims = 2048;
static s32 const kMaxTexAnimInsts = 1024;
static s32 const kMaxRegisteredTexAnims = 1024;
static s32 const kMaxRenderedInsts = RENDER_MAX_GEOM_SUBMISSIONS;

static xbool CanAppendDynamicStorage( s32 CurrentCount, s32 AddCount, u32 ElementSize )
{
    if( ( CurrentCount < 0 ) || ( AddCount <= 0 ) || ( ElementSize == 0 ) )
        return FALSE;

    const u64 RequiredCount = static_cast<u64>( CurrentCount ) + static_cast<u64>( AddCount );
    return ( RequiredCount <= ( 0x7fffffffu / ElementSize ) ) &&
           ( RequiredCount * ElementSize <= render::FrameStorage::MAX_STREAM_BYTES );
}

//-------------------------------------------------------------------------

template<typename T>
static xbool EnsureDynamicStorageCapacity( xarray<T>& Storage, s32 RequiredCount, char const* pName )
{
    if( RequiredCount <= 0 )
        return TRUE;

    u64 const MaximumCount64 = render::FrameStorage::MAX_STREAM_BYTES / sizeof( T );
    if( ( MaximumCount64 == 0 ) || ( MaximumCount64 > 0x7fffffffu ) ||
        ( static_cast<u64>( RequiredCount ) > MaximumCount64 ) )
    {
        x_DebugMsg( "Render: %s exceeds bounded dynamic storage\n", pName );
        return FALSE;
    }

    if( Storage.GetCapacity() >= RequiredCount )
        return TRUE;

    s32 const MaximumCount = static_cast<s32>( MaximumCount64 );
    s32       Capacity    = MAX( Storage.GetCapacity(), 1 );
    while( Capacity < RequiredCount )
    {
        if( Capacity > ( 0x7fffffff / 2 ) )
        {
            Capacity = RequiredCount;
            break;
        }
        Capacity *= 2;
    }
    Capacity = MIN( Capacity, MaximumCount );
    if( Capacity < RequiredCount )
        return FALSE;

    Storage.SetCapacity( Capacity );
    return TRUE;
}

//-------------------------------------------------------------------------
//  REGISTERED DATA
//-------------------------------------------------------------------------

static xharray<PrivateGeom> s_RegisteredGeom;
static xharray<PrivateInst> s_RegisteredInsts;
static MaterialArray        s_RegisteredMaterials;

//-------------------------------------------------------------------------
//  SCENE COLLECTION STATE
//-------------------------------------------------------------------------

// Track geometry collection separately from graph execution.
static xbool s_isSceneRenderedThisFrame = FALSE;

//-------------------------------------------------------------------------
//  MISCELLANEOUS DATA
//-------------------------------------------------------------------------

static cubemap* s_pCurrentCubeMap = NULL;
static f32      s_PulseTime;

//-------------------------------------------------------------------------
//  DEBUGGING OPTIONS
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
//  FILTER LIGHT DATA
//-------------------------------------------------------------------------

static s32    s_isLightFilteringEnabled = FALSE;
static xcolor s_FilterLightColor( xcolor( 30, 0, 0, 255 ) );

//-------------------------------------------------------------------------
//  SHADOW SOURCE DATA
//-------------------------------------------------------------------------

static xbool s_isRenderFailureActive = FALSE;
static xbool s_isRecoverableFailureActive = FALSE;
static xbool s_isRecoverableFailureScope = FALSE;
static char const* s_pRecoverableFailurePass = NULL;
static render::RenderScopeDesc const* s_pActiveRenderScopeDesc = NULL;
static render::RenderScopeDesc s_DefaultRenderScopeDesc;
static s32 s_NextRenderScopeIdentity = -2;

//=========================================================================
//  RENDER PLATFORM ADAPTER
//=========================================================================

#include "LightSystem.hpp"
#include "PrimitiveBatch.hpp"
#include "Platform/RenderPlatform.hpp"

static xhandle GetRegisteredMaterialHandle( xhandle hGeom, s32 iVirtualMaterial );

using namespace render;
using namespace render::platform;

//=========================================================================

render::RenderScopeDesc const& render::GetActiveRenderScopeDesc( void )
{
    return s_pActiveRenderScopeDesc ? *s_pActiveRenderScopeDesc : s_DefaultRenderScopeDesc;
}

//=========================================================================

void render::ResetActiveRenderScopeCounters( void )
{
    s_DefaultRenderScopeDesc.SpriteCount = 0;
}

//=========================================================================

void render::AddActiveRenderScopeSprites( s32 Count )
{
    GetActiveRenderScopeDesc().SpriteCount += Count;
}

//=========================================================================

render::ScopedRenderView::ScopedRenderView( RenderScopeDesc const& Desc )
    : m_pPrevious( s_pActiveRenderScopeDesc ),
      m_wasFrameActive( GetRenderPipeline().GetFrame() != NULL )
{
    Desc.SpriteCount = 0;
    s_pActiveRenderScopeDesc = &Desc;
}

//=========================================================================

render::ScopedRenderView::~ScopedRenderView( void )
{
    if ( !m_wasFrameActive && GetRenderPipeline().GetFrame() )
        platform_ResetAfterException();
    s_pActiveRenderScopeDesc = m_pPrevious;
}

//=========================================================================

xbool render::RenderTargetSurface::Create( s32 TargetWidth, s32 TargetHeight )
{
    Destroy();
    if ( ( TargetWidth <= 0 ) || ( TargetHeight <= 0 ) )
        return FALSE;

    Width  = TargetWidth;
    Height = TargetHeight;
    ScopeId = s_NextRenderScopeIdentity;
    s_NextRenderScopeIdentity = ( s_NextRenderScopeIdentity == ( -2147483647 - 1 ) )
                              ? -2
                              : s_NextRenderScopeIdentity - 1;

    rtarget_desc ColorDesc;
    ColorDesc.Width          = static_cast<u32>( Width );
    ColorDesc.Height         = static_cast<u32>( Height );
    ColorDesc.Format         = RTARGET_FORMAT_RGBA8;
    ColorDesc.SampleCount    = 1;
    ColorDesc.SampleQuality  = 0;
    ColorDesc.bBindAsTexture = TRUE;

    if ( !rtarget_Create( ColorTarget, ColorDesc ) )
    {
        Destroy();
        return FALSE;
    }

    rtarget_desc DepthDesc = ColorDesc;
    DepthDesc.Format         = RTARGET_FORMAT_DEPTH_STENCIL;
    DepthDesc.bBindAsTexture = TRUE;
    f32 const RenderScale = GetRenderPipeline().GetRenderScale();
    DepthDesc.Width = MAX( 1u, static_cast<u32>( x_ceil( static_cast<f32>( DepthDesc.Width ) * RenderScale ) ) );
    DepthDesc.Height = MAX( 1u, static_cast<u32>( x_ceil( static_cast<f32>( DepthDesc.Height ) * RenderScale ) ) );

    if ( !rtarget_Create( DepthTarget, DepthDesc ) )
    {
        Destroy();
        return FALSE;
    }

    bValid = TRUE;
    return TRUE;
}

//=========================================================================

void render::RenderTargetSurface::Destroy( void )
{
    if ( rtarget_HasTexture( ColorTarget ) )
        rtarget_Destroy( ColorTarget );
    if ( rtarget_HasTexture( DepthTarget ) )
        rtarget_Destroy( DepthTarget );
    Width = 0;
    Height = 0;
    ScopeId = -1;
    bValid = FALSE;
}

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
RenderSubmission& FrameSubmission( void )
{
    return GetRenderPipeline().GetSubmission();
}

//=========================================================================

static
RenderShadowSubmission& ShadowSubmission( void )
{
    return GetRenderPipeline().GetShadowSubmission();
}

//=========================================================================

// Derive collection state from the submission object.
static
xbool IsSceneGeomCollectionActive( void )
{
    if ( !GetRenderPipeline().GetFrame() )
        return FALSE;

    RenderPipeline const& renderer = GetRenderPipeline();
    RenderSubmission const& submission = renderer.GetSubmission();

    return submission.IsOpen() && !submission.IsSceneSubmitted();
}

//=========================================================================

void render::render_ResetFailureState( void )
{
    s_isRenderFailureActive = FALSE;
    s_isRecoverableFailureActive = FALSE;
    s_isRecoverableFailureScope = FALSE;
    s_pRecoverableFailurePass = NULL;
}

//=========================================================================

void render::render_BeginFailureScope( xbool bRecoverable, char const* pPassName )
{
    s_isRecoverableFailureScope = bRecoverable;
    s_pRecoverableFailurePass = pPassName;
    s_isRecoverableFailureActive = FALSE;
}

//=========================================================================

void render::render_EndFailureScope( void )
{
    s_isRecoverableFailureScope = FALSE;
    s_pRecoverableFailurePass = NULL;
}

//=========================================================================

void render::render_ClearRecoverableFailure( void )
{
    s_isRecoverableFailureActive = FALSE;
}

//=========================================================================

xbool render::render_HasRecoverableFailure( void )
{
    return s_isRecoverableFailureActive;
}

//=========================================================================

void render::render_ReportFailure( char const* pFile, s32 Line, char const* pSubsystem,
                           char const* pFormat, ... )
{
    if ( s_isRecoverableFailureScope )
    {
        if ( s_isRecoverableFailureActive )
            return;

        x_va_list args;
        x_va_start( args, pFormat );
        xstring detail;
        detail.FormatV( pFormat, args );
        x_va_end( args );
        x_DebugMsg( "[RENDER][RECOVERABLE][%s] pass=%s %s (%s:%d)\n",
                    pSubsystem ? pSubsystem : "Unknown",
                    s_pRecoverableFailurePass ? s_pRecoverableFailurePass : "<unnamed>",
                    (char const*)detail, pFile ? pFile : "<unknown>", Line );
        s_isRecoverableFailureActive = TRUE;
        return;
    }

    // Preserve the first render failure if execution continues in the debugger.
    if ( s_isRenderFailureActive )
    {
        return;
    }

    x_va_list args;
    x_va_start( args, pFormat );
    xstring detail;
    detail.FormatV( pFormat, args );
    x_va_end( args );

    xfs error( "[RENDER][%s] %s (%s:%d)", pSubsystem ? pSubsystem : "Unknown",
               (char const*)detail, pFile ? pFile : "<unknown>", Line );
    s_isRenderFailureActive = TRUE;
    if ( GetRenderPipeline().GetFrame() )
    {
        platform_ResetFrameState();
    }
    ASSERTS( FALSE, error );
    x_throw( (char const*)error );
}

//=========================================================================

void render::render_ReportWarning( char const* pSubsystem, char const* pFormat, ... )
{
    x_va_list args;
    x_va_start( args, pFormat );
    xstring detail;
    detail.FormatV( pFormat, args );
    x_va_end( args );
    x_DebugMsg( "[RENDER][WARNING][%s] %s\n", pSubsystem ? pSubsystem : "Unknown", (char const*)detail );
}

//=========================================================================
//  INTERNAL FUNCTIONS
//=========================================================================

static
xhandle FindMaterial( material const& SourceMaterial )
{
    for ( s32 i = 0; i < s_RegisteredMaterials.GetCount(); i++ )
    {
        material& m = s_RegisteredMaterials[i];

        if ( m == SourceMaterial )
        {
            return s_RegisteredMaterials.GetHandleByIndex( i );
        }
    }

    return HNULL;
}

//=========================================================================

static
xhandle FindRegisteredGeom( geom const& geom )
{
    for ( s32 i = 0; i < s_RegisteredGeom.GetCount(); ++i )
    {
        if ( s_RegisteredGeom[i].m_pGeom == &geom )
        {
            return s_RegisteredGeom.GetHandleByIndex( i );
        }
    }

    return HNULL;
}

//=========================================================================

static
xhandle GetRegisteredMaterialHandle( xhandle hGeom, s32 iVirtualMaterial )
{
    PrivateGeom const& RegisteredGeom = s_RegisteredGeom( hGeom );
    ASSERT( iVirtualMaterial >= 0 );
    ASSERT( iVirtualMaterial < RegisteredGeom.m_MaterialHandles.GetCount() );
    return RegisteredGeom.m_MaterialHandles[iVirtualMaterial];
}

//=========================================================================

static
void RegisterMaterials( PrivateGeom& RegisteredGeom )
{
    geom& geom = *RegisteredGeom.m_pGeom;

    RegisteredGeom.m_MaterialHandles.SetCount( geom.m_nVirtualMaterials );
    for ( s32 i = 0; i < RegisteredGeom.m_MaterialHandles.GetCount(); ++i )
    {
        RegisteredGeom.m_MaterialHandles[i].Handle = HNULL;
    }

    for ( s32 iMat = 0; iMat < geom.m_nMaterials; iMat++ )
    {
        geom::material const& GeomMat = geom.m_pMaterial[iMat];
        if( !geom.PrepareMaterials() ) x_throw( "Invalid geometry material description" );
        for( s32 iVMat=0; iVMat<GeomMat.nVirtualMats; ++iVMat )
        {
            material const& mat = geom.GetPreparedMaterial( GeomMat.iVirtualMat + iVMat );

            xhandle handle = FindMaterial( mat );
            if ( handle == HNULL )
            {
                ASSERT( s_RegisteredMaterials.GetCount() < kMaxRegisteredMaterials );
                material& NewMat = s_RegisteredMaterials.Add( handle );
                NewMat = mat;
            }

            material& FinalMat = s_RegisteredMaterials( handle );
            FinalMat.AddRef();

            s32 const iRegisteredMaterial = GeomMat.iVirtualMat + iVMat;
            ASSERT( iRegisteredMaterial >= 0 );
            ASSERT( iRegisteredMaterial < RegisteredGeom.m_MaterialHandles.GetCount() );
            RegisteredGeom.m_MaterialHandles[iRegisteredMaterial] = handle;
        }
    }
}

//=========================================================================

static
void UnregisterMaterials( PrivateGeom& RegisteredGeom )
{
    for ( s32 i = 0; i < RegisteredGeom.m_MaterialHandles.GetCount(); ++i )
    {
        xhandle const handle = RegisteredGeom.m_MaterialHandles[i];
        if ( handle.IsNull() )
        {
            continue;
        }

        material& mat = s_RegisteredMaterials( handle );
        mat.Release();
        if ( mat.GetRefCount() == 0 )
        {
            s_RegisteredMaterials.DeleteByHandle( handle );
        }
    }

    RegisteredGeom.m_MaterialHandles.Clear();
}

//=========================================================================

static
xhandle RegisterGeom( geom& geom, GeomType type )
{
    ASSERT( FindRegisteredGeom( geom ).IsNull() );
    ASSERT( s_RegisteredGeom.GetCount() < kMaxRegisteredGeoms );

    xhandle          hGeom;
    PrivateGeom& RegisteredGeom = s_RegisteredGeom.Add( hGeom );
    RegisteredGeom.m_pGeom = &geom;
    RegisteredGeom.m_Type = type;
    RegisteredGeom.m_nReferenceCount = 0;
    RegisteredGeom.m_RenderGeomHandle.Handle = HNULL;
    RegisteredGeom.m_MaterialHandles.Clear();

    x_try;

    RegisterMaterials( RegisteredGeom );

    x_catch_begin;

    UnregisterMaterials( RegisteredGeom );
    s_RegisteredGeom.DeleteByHandle( hGeom );

    x_catch_end_ret;

#ifndef X_RETAIL
    s_LoadedGeomCount += 1;
    s_LoadedBoneCount += geom.m_nBones;
    s_LoadedMeshCount += geom.m_nMeshes;
    s_LoadedSubmeshCount += geom.m_nSubMeshes;
    s_LoadedMaterialCount += geom.m_nMaterials;
    s_LoadedTextureCount += geom.m_nTextures;
    s_LoadedUVKeyCount += geom.m_nUVKeys;
    s_LoadedVirtualMaterialCount += geom.m_nVirtualMaterials;
#endif

    return hGeom;
}

//=========================================================================

static
void UnregisterGeom( xhandle hGeom )
{
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    geom&            geom = *RegisteredGeom.m_pGeom;

    ASSERT( RegisteredGeom.m_nReferenceCount == 0 );
    ASSERT( RegisteredGeom.m_RenderGeomHandle.IsNull() );

    UnregisterMaterials( RegisteredGeom );

#ifndef X_RETAIL
    s_LoadedGeomCount -= 1;
    s_LoadedBoneCount -= geom.m_nBones;
    s_LoadedMeshCount -= geom.m_nMeshes;
    s_LoadedSubmeshCount -= geom.m_nSubMeshes;
    s_LoadedMaterialCount -= geom.m_nMaterials;
    s_LoadedTextureCount -= geom.m_nTextures;
    s_LoadedUVKeyCount -= geom.m_nUVKeys;
    s_LoadedVirtualMaterialCount -= geom.m_nVirtualMaterials;
#endif

    s_RegisteredGeom.DeleteByHandle( hGeom );
}

//=========================================================================

static
xhandle RegisterRigidGeom( rigid_geom& geom )
{
    xhandle const    hGeom = RegisterGeom( geom, GeomType::Rigid );
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );

    x_try;

    RegisteredGeom.m_RenderGeomHandle = platform_RegisterRigidGeom( geom );

    x_catch_begin;

    RegisteredGeom.m_RenderGeomHandle.Handle = HNULL;
    UnregisterGeom( hGeom );

    x_catch_end_ret;

    return hGeom;
}

//=========================================================================

static
void UnregisterRigidGeom( xhandle hGeom )
{
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    ASSERT( RegisteredGeom.m_Type == GeomType::Rigid );

    platform_UnregisterRigidGeom( RegisteredGeom.m_RenderGeomHandle );
    RegisteredGeom.m_RenderGeomHandle.Handle = HNULL;
    UnregisterGeom( hGeom );
}

//=========================================================================

static
xhandle RegisterSkinGeom( skin_geom& geom )
{
    xhandle const    hGeom = RegisterGeom( geom, GeomType::Skin );
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );

    x_try;

    RegisteredGeom.m_RenderGeomHandle = platform_RegisterSkinGeom( geom );

    x_catch_begin;

    RegisteredGeom.m_RenderGeomHandle.Handle = HNULL;
    UnregisterGeom( hGeom );

    x_catch_end_ret;

    return hGeom;
}

//=========================================================================

static
void UnregisterSkinGeom( xhandle hGeom )
{
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    ASSERT( RegisteredGeom.m_Type == GeomType::Skin );

    platform_UnregisterSkinGeom( RegisteredGeom.m_RenderGeomHandle );
    RegisteredGeom.m_RenderGeomHandle.Handle = HNULL;
    UnregisterGeom( hGeom );
}

//=========================================================================

static
render::GeometryInstanceHandle AddPrivateInst( xhandle hGeom, GeomType type )
{
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    ASSERT( RegisteredGeom.m_Type == type );
    RegisteredGeom.m_nReferenceCount++;

    render::GeometryInstanceHandle handle;
    ASSERT( s_RegisteredInsts.GetCount() < kMaxRegisteredInsts );
    PrivateInst& inst = s_RegisteredInsts.Add( handle );
    inst.m_pGeom = RegisteredGeom.m_pGeom;
    inst.m_Type = type;
    inst.m_GeomHandle = hGeom;

    return handle;
}

//=========================================================================

static
void RemovePrivateInst( render::GeometryInstanceHandle hInst )
{
    PrivateInst& inst = s_RegisteredInsts( hInst );
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( inst.m_GeomHandle );

    ASSERT( RegisteredGeom.m_nReferenceCount > 0 );
    RegisteredGeom.m_nReferenceCount--;
    s_RegisteredInsts.DeleteByHandle( hInst );
}

//=========================================================================

static
s32 ShadowCompareFn( void const* p1, void const* p2 )
{
    geom_draw_item const& a = **(geom_draw_item const* const*)p1;
    geom_draw_item const& b = **(geom_draw_item const* const*)p2;

    if ( a.ShadowSourceIndex < b.ShadowSourceIndex )
    {
        return -1;
    }
    if ( a.ShadowSourceIndex > b.ShadowSourceIndex )
    {
        return 1;
    }
    if ( a.Type < b.Type )
    {
        return -1;
    }
    if ( a.Type > b.Type )
    {
        return 1;
    }
    if ( a.hRenderGeom.Handle < b.hRenderGeom.Handle )
    {
        return -1;
    }
    if ( a.hRenderGeom.Handle > b.hRenderGeom.Handle )
    {
        return 1;
    }
    if ( a.iSurface < b.iSurface )
    {
        return -1;
    }
    if ( a.iSurface > b.iSurface )
    {
        return 1;
    }
    if ( a.Sequence < b.Sequence )
    {
        return -1;
    }
    if ( a.Sequence > b.Sequence )
    {
        return 1;
    }
    return 0;
}

//=========================================================================

geom_render_pass render::ClassifyGeomPass( material const& material, u32 flags, xbool FadingAlpha )
{
    if ( material.IsDistortion() )
    {
        return GEOM_PASS_DISTORTION;
    }

    if ( FadingAlpha )
    {
        return GEOM_PASS_FADING;
    }

    if ( flags & render::FORCE_LAST )
    {
        return GEOM_PASS_FORCE_LAST;
    }

    if ( material.RequiresForwardPass() )
    {
        return material.IsPostEffectBlend() ? GEOM_PASS_ADDITIVE : GEOM_PASS_TRANSPARENT;
    }

    return GEOM_PASS_FORWARD_OPAQUE;
}

//=========================================================================

static
xbool ComputeGeomDepth( f32& depth, bbox const& LocalBBox, matrix4 const* pLocalToWorld )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    vector3 center = LocalBBox.GetCenter();
    if ( pLocalToWorld )
    {
        center = *pLocalToWorld * center;
    }

    depth = ( pView->GetW2V() * center ).GetZ();
    return x_isvalid( depth );
}

//=========================================================================

static xbool IntersectsView( view const& v, bbox const& bBox );

//=========================================================================

static
xbool ShouldSubmitGeom( u32 flags, render::geom_inst_features InstFeatures,
                             bbox const& WorldBBox )
{
    // Some models probably have incorrect BBOX. So, its better to disable this.
    /*
    if ( ( flags & render::CLIPPED ) != 0 )
    {
        view const* pView = eng_GetView();
        if ( pView && !IntersectsView( *pView, WorldBBox ) )
        {
            return FALSE;
        }
    }
    */

#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderClippedOnly && !( flags & render::CLIPPED ) )
    {
        return FALSE;
    }

    if ( GetRenderPipeline().GetDebugOptions().RenderShadowedOnly &&
         !( InstFeatures & render::GEOM_INST_PROJ_SHADOW ) )
    {
        return FALSE;
    }
#else
    static_cast<void>( flags );
    static_cast<void>( InstFeatures );
    static_cast<void>( WorldBBox );
#endif

    return TRUE;
}

//=========================================================================

// Resolve derived per-instance features from submission flags and frame state.
static
render::geom_inst_features ResolveInstFeatures( u32 flags,
                                                 render::geom_inst_features ProjFeatures,
                                                 material const* pMaterial = NULL,
                                                 geom_render_pass pass = GEOM_PASS_FORWARD_OPAQUE )
{
    render::geom_inst_features features = ProjFeatures;

    if ( pMaterial &&
         ( ( pass != GEOM_PASS_FORWARD_OPAQUE ) && ( pass != GEOM_PASS_TRANSPARENT ) &&
           ( pass != GEOM_PASS_ADDITIVE ) ) )
    {
        features &= ~( render::GEOM_INST_PROJ_LIGHT | render::GEOM_INST_PROJ_SHADOW );
    }
    else if ( pMaterial && !pMaterial->ReceivesProjection() )
    {
        features &= ~( render::GEOM_INST_PROJ_LIGHT | render::GEOM_INST_PROJ_SHADOW );
    }

    if ( flags & render::GLOWING )
    {
        features |= render::GEOM_INST_FORCED_GLOW;
    }

    if ( render::IsFilterLightEnabled() && !( flags & render::DISABLE_FILTER_LIGHT ) )
    {
        features |= render::GEOM_INST_FILTER_LIGHT;
    }

    return features;
}

//=========================================================================

static
void AppendZPrimeDraw( geom_draw_item const& source )
{
    ASSERT( FrameSubmission().Geom().GetCount() < kMaxRenderedInsts );
    if ( FrameSubmission().Geom().GetCount() >= kMaxRenderedInsts )
    {
        return;
    }

    geom_draw_item  ZPrime = source;
    geom_draw_item& item = FrameSubmission().Geom().Append();
    item = ZPrime;
    item.Pass = GEOM_PASS_ZPRIME;
    item.pMaterial = NULL;
    item.MaterialOrder = 0;
    item.RenderFlags = source.RenderFlags & render::CLIPPED;
    item.InstFeatures = 0;
    item.Alpha = 0x80;
    item.MaterialOverride = TRUE;
    item.Sequence = FrameSubmission().NextGeomSequence()++;
}

//=========================================================================

static
void GetUVOffset( u8& uOffset, u8& vOffset, geom* pGeom, material& mat )
{
    if ( mat.m_UVAnim.nFrames == 0 )
    {
        uOffset = vOffset = 0;
        return;
    }

    s32 iKey = mat.m_UVAnim.iKey + mat.m_UVAnim.iFrame;
    uOffset = pGeom->m_pUVKey[iKey].OffsetU;
    vOffset = pGeom->m_pUVKey[iKey].OffsetV;
}

//=========================================================================

static
void AppendRigidGeomDraw( PrivateInst const& RegisteredInst, rigid_geom& geom, xhandle hMaterial,
                              s32 iSurface, matrix4 const* pL2W, u32 const* pColorInfo,
                              RenderLightingHandle Lighting,
                              u32 flags, render::geom_inst_features ProjFeatures,
                              bbox const& WorldBBox,
                              u8 alpha, xbool FadingAlpha,
                              render::RenderContributionPolicy ContributionPolicy )
{
    ASSERT( FrameSubmission().Geom().GetCount() < kMaxRenderedInsts );
    if ( FrameSubmission().Geom().GetCount() >= kMaxRenderedInsts )
    {
        return;
    }

    PrivateGeom&           RegisteredGeom = s_RegisteredGeom( RegisteredInst.m_GeomHandle );
    material&                  material = s_RegisteredMaterials( hMaterial );
    geom_render_pass const pass = render::ClassifyGeomPass( material, flags, FadingAlpha );

    render::geom_inst_features const InstFeatures =
        ResolveInstFeatures( flags, ProjFeatures, &material, pass );
    if ( !ShouldSubmitGeom( flags, InstFeatures, WorldBBox ) )
    {
        return;
    }

    f32 SortDepth = 0.0f;
    if ( ( pass == GEOM_PASS_TRANSPARENT ) || ( pass == GEOM_PASS_ADDITIVE ) ||
         ( pass == GEOM_PASS_FORCE_LAST ) || ( pass == GEOM_PASS_FADING ) ||
         ( pass == GEOM_PASS_DISTORTION ) )
    {
        render::GeometryRepository const& Geometry = render::GetRenderPipeline().GetBackend().GetGeometryRepository();
        render::GeometryRepository::RigidMeshView Surface;
        xhandle const hSurface = Geometry.GetRigidSurface( RegisteredGeom.m_RenderGeomHandle, iSurface );

        if ( !Geometry.GetRigidMeshView( hSurface, Surface ) || !Surface.HasLocalBounds )
        {
            RENDER_THROW( "Render", "rigid surface %d has no bounds for transparency sorting", iSurface );
            return;
        }

        if ( !ComputeGeomDepth( SortDepth, Surface.LocalBounds, pL2W ) )
        {
            return;
        }
    }

    geom_draw_item& item = FrameSubmission().Geom().Append();
    item.Pass = pass;
    item.Type = GEOM_DRAW_RIGID;
    item.hRenderGeom = RegisteredGeom.m_RenderGeomHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_RegisteredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.RenderFlags = flags;
    item.InstFeatures = InstFeatures;
    item.Lighting = Lighting;
    item.Data.Rigid.pGeom = &geom;
    item.Data.Rigid.pL2W = pL2W;
    item.Data.Rigid.pColorInfo = pColorInfo;
    item.SortDepth = SortDepth;
    item.Sequence = FrameSubmission().NextGeomSequence()++;
    item.ShadowSourceIndex = -1;
    item.Alpha = alpha;
    item.MaterialOverride = FALSE;
    item.ContributionPolicy = ContributionPolicy;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );

    if ( FadingAlpha )
    {
        AppendZPrimeDraw( item );
    }
}

//=========================================================================

static
void AppendSkinGeomDraw( PrivateInst const& RegisteredInst, skin_geom& geom, xhandle hMaterial,
                             s32 iSurface, matrix4 const* pBones, s32 BoneCount, RenderLightingHandle Lighting, u32 flags,
                             render::geom_inst_features ProjFeatures, bbox const& WorldBBox,
                             u8 alpha, xbool FadingAlpha,
                             render::RenderContributionPolicy ContributionPolicy )
{
    ASSERT( FrameSubmission().Geom().GetCount() < kMaxRenderedInsts );
    if ( FrameSubmission().Geom().GetCount() >= kMaxRenderedInsts )
    {
        return;
    }

    PrivateGeom&           RegisteredGeom = s_RegisteredGeom( RegisteredInst.m_GeomHandle );
    material&                  material = s_RegisteredMaterials( hMaterial );
    geom_render_pass const pass = render::ClassifyGeomPass( material, flags, FadingAlpha );

    render::geom_inst_features const InstFeatures =
        ResolveInstFeatures( flags, ProjFeatures, &material, pass );
    if ( !ShouldSubmitGeom( flags, InstFeatures, WorldBBox ) )
    {
        return;
    }

    f32 SortDepth = 0.0f;
    if ( ( ( pass == GEOM_PASS_TRANSPARENT ) || ( pass == GEOM_PASS_ADDITIVE ) ||
           ( pass == GEOM_PASS_FORCE_LAST ) || ( pass == GEOM_PASS_FADING ) ||
           ( pass == GEOM_PASS_DISTORTION ) ) &&
         !ComputeGeomDepth( SortDepth, geom.m_BBox, pBones ) )
    {
        return;
    }

    geom_draw_item& item = FrameSubmission().Geom().Append();
    item.Pass = pass;
    item.Type = GEOM_DRAW_SKIN;
    item.hRenderGeom = RegisteredGeom.m_RenderGeomHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_RegisteredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.RenderFlags = flags;
    item.InstFeatures = InstFeatures;
    item.Lighting = Lighting;
    item.Data.Skin.pGeom = &geom;
    item.Data.Skin.pBones = pBones;
    item.Data.Skin.BoneCount = BoneCount;
    item.SortDepth = SortDepth;
    item.Sequence = FrameSubmission().NextGeomSequence()++;
    item.ShadowSourceIndex = -1;
    item.Alpha = alpha;
    item.MaterialOverride = FALSE;
    item.ContributionPolicy = ContributionPolicy;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );

    if ( FadingAlpha )
    {
        AppendZPrimeDraw( item );
    }
}

//=========================================================================

static
void AppendRigidShadowDraw( PrivateInst const& RegisteredInst, rigid_geom& geom, xhandle hMaterial,
                            s32 iSurface, matrix4 const* pL2W, s32 ShadowSourceIndex )
{
    ASSERT( ShadowSubmission().Draws().GetCount() < kMaxRenderedInsts );
    if ( ShadowSubmission().Draws().GetCount() >= kMaxRenderedInsts )
    {
        return;
    }

    PrivateGeom& RegisteredGeom = s_RegisteredGeom( RegisteredInst.m_GeomHandle );
    material&        material = s_RegisteredMaterials( hMaterial );

    geom_draw_item& item = ShadowSubmission().Draws().Append();
    item.Pass = GEOM_PASS_SHADOW_CAST;
    item.Type = GEOM_DRAW_RIGID;
    item.hRenderGeom = RegisteredGeom.m_RenderGeomHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_RegisteredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.RenderFlags = 0;
    item.Lighting = RenderLightingHandle();
    item.Data.Rigid.pGeom = &geom;
    item.Data.Rigid.pL2W = pL2W;
    item.Data.Rigid.pColorInfo = NULL;
    item.SortDepth = 0.0f;
    item.ShadowSourceIndex = ShadowSourceIndex;
    item.Sequence = ShadowSubmission().NextSequence()++;
    item.Alpha = 255;
    item.MaterialOverride = FALSE;
    item.ContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );
}

//=========================================================================

static
void AppendSkinShadowDraw( PrivateInst const& RegisteredInst, skin_geom& geom, xhandle hMaterial,
                           s32 iSurface, matrix4 const* pBones, s32 BoneCount, s32 ShadowSourceIndex )
{
    ASSERT( ShadowSubmission().Draws().GetCount() < kMaxRenderedInsts );
    if ( ShadowSubmission().Draws().GetCount() >= kMaxRenderedInsts )
    {
        return;
    }

    PrivateGeom& RegisteredGeom = s_RegisteredGeom( RegisteredInst.m_GeomHandle );
    material&        material = s_RegisteredMaterials( hMaterial );

    geom_draw_item& item = ShadowSubmission().Draws().Append();
    item.Pass = GEOM_PASS_SHADOW_CAST;
    item.Type = GEOM_DRAW_SKIN;
    item.hRenderGeom = RegisteredGeom.m_RenderGeomHandle;
    item.pMaterial = &material;
    item.MaterialOrder = s_RegisteredMaterials.GetIndexByHandle( hMaterial );
    item.iSurface = iSurface;
    item.RenderFlags = 0;
    item.Lighting = RenderLightingHandle();
    item.Data.Skin.pGeom = &geom;
    item.Data.Skin.pBones = pBones;
    item.Data.Skin.BoneCount = BoneCount;
    item.SortDepth = 0.0f;
    item.ShadowSourceIndex = ShadowSourceIndex;
    item.Sequence = ShadowSubmission().NextSequence()++;
    item.Alpha = 255;
    item.MaterialOverride = FALSE;
    item.ContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT;
    item.DistortionNormalRot.Zero();
    GetUVOffset( item.UOffset, item.VOffset, &geom, material );
}

//=========================================================================

static
xbool IntersectsView( view const& v, bbox const& bBox )
{
    return ( v.BBoxInView( bBox ) != view::VISIBLE_NONE );
}

//=========================================================================

static
void CalcVMatOffsets( s32* pVMatOffsets, geom const* pGeom, u32 vTextureMask )
{
    x_memset( pVMatOffsets, 0, sizeof( s32 ) * 32 );

    s32 i, j;
    for ( i = 0; i < pGeom->m_nVirtualTextures; i++ )
    {
        s32 offset = vTextureMask & 0xf;
        vTextureMask >>= 4;

        geom::virtual_texture& vTexture = pGeom->m_pVirtualTextures[i];
        for ( j = 0; j < pGeom->m_nMaterials; j++ )
        {
            ASSERT( j < 32 );
            if ( vTexture.MaterialMask & ( 1 << j ) )
            {
                offset = MINMAX( 0, offset, pGeom->m_pMaterial[j].nVirtualMats - 1 );
                pVMatOffsets[j] = offset;
            }
        }
    }
}

//=========================================================================
//  PUBLIC FUNCTIONS
//=========================================================================

s32 render::GetHardwareBufferSize( void )
{
    return 80;
}

//=========================================================================

void render::Init( void )
{
    if( !g_FontMgr.Init( "icudt78l.dat" ) )
    {
        x_throw( "Font system initialization failed" );
    }

    s_PulseTime = 0.0f;

    s_RegisteredGeom.Clear();
    s_RegisteredGeom.GrowListBy( kMaxRegisteredGeoms );
    s_RegisteredInsts.Clear();
    s_RegisteredInsts.GrowListBy( kMaxRegisteredInsts );
    s_RegisteredMaterials.Clear();
    s_RegisteredMaterials.GrowListBy( kMaxRegisteredMaterials );
    ShadowSubmission().ClearDynamicDraws();
    ShadowSubmission().DynamicDraws().SetCapacity( kMaxRenderedInsts );
    ShadowSubmission().Draws().Clear();
    ShadowSubmission().Draws().SetCapacity( kMaxRenderedInsts );
    ShadowSubmission().OrderedDraws().Clear();
    ShadowSubmission().OrderedDraws().SetCapacity( kMaxRenderedInsts );
    ShadowSubmission().NextSequence() = 0;

    if ( !platform_Init() )
    {
        x_throw( "render initialization failed" );
    }
}

//=========================================================================

void render::Kill( void )
{
    eng_Sync();
    g_FontMgr.Kill();
    platform_Kill();

    ASSERT( s_RegisteredGeom.GetCount() == 0 );
    ASSERT( s_RegisteredInsts.GetCount() == 0 );
    ASSERT( s_RegisteredMaterials.GetCount() == 0 );
    s_RegisteredGeom.Clear();
    s_RegisteredInsts.Clear();
    s_RegisteredMaterials.Clear();
    ShadowSubmission().ClearDynamicDraws();
    ShadowSubmission().Draws().Clear();
    ShadowSubmission().OrderedDraws().Clear();
}

//=========================================================================

void render::Update( f32 DeltaTime )
{
    s_PulseTime += DeltaTime;
    platform_UpdatePostProcessing( DeltaTime );

    // update all uv animations
    s_RegisteredMaterials.Update( DeltaTime );
}

//=========================================================================

xbool render::SubmitPrimitives( primitive_draw_desc const& desc, matrix4 const& LocalToWorld,
                                primitive_vertex const* pVertices, s32 nVertices, u16 const* pIndices, s32 nIndices )
{
    RenderFrameContext const* pFrame = GetRenderPipeline().GetFrame();
    if ( !pFrame || !GetRenderPipeline().GetSubmission().IsOpen() )
    {
        RENDER_THROW( "Render", "primitive submitted outside an open frame submission" );
        return FALSE;
    }

    return platform_SubmitPrimitives( desc, LocalToWorld, pVertices, nVertices, pIndices, nIndices );
}

//=========================================================================

xbool render::SubmitDecalBatch( decal_draw_desc const& desc, decal_vertex const* pVertices, s32 nVertices,
                                u16 const* pIndices, s32 nIndices )
{
    ASSERTS( IsSceneGeomCollectionActive(), "Decal geometry submitted outside scene geometry collection" );
    if ( !IsSceneGeomCollectionActive() )
    {
        RENDER_THROW( "Render", "decal submitted outside an active scene" );
        return FALSE;
    }

    return platform_SubmitDecalBatch( desc, s_pCurrentCubeMap, pVertices, nVertices, pIndices, nIndices );
}

//=========================================================================

void render::ReserveDecalSubmissionCapacity( s32 nVertices, s32 nIndices, s32 nDraws )
{
    platform_ReserveDecalSubmissionCapacity( nVertices, nIndices, nDraws );
}

//=========================================================================

xbool render::SetDepthRect( irect const& rect, f32 depth )
{
    RenderFrameContext const* pFrame = GetRenderPipeline().GetFrame();
    if ( !pFrame || !GetRenderPipeline().GetSubmission().IsOpen() )
    {
        RENDER_THROW( "Render", "depth rectangle submitted outside an open frame submission" );
        return FALSE;
    }

    return platform_SetDepthRect( rect, depth );
}

//=========================================================================

render::GeometryInstanceHandle render::RegisterRigidInstance( rigid_geom& geom )
{
    ASSERT( !IsSceneGeomCollectionActive() && !ShadowSubmission().IsActive() );

    xhandle hGeom = FindRegisteredGeom( geom );
    if ( hGeom.IsNull() )
    {
        hGeom = RegisterRigidGeom( geom );
    }

    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    ASSERT( RegisteredGeom.m_pGeom == &geom );
    ASSERT( RegisteredGeom.m_Type == GeomType::Rigid );

    render::GeometryInstanceHandle handle = AddPrivateInst( hGeom, GeomType::Rigid );

    return handle;
}

//=========================================================================

void render::UnregisterRigidInstance( GeometryInstanceHandle hInst )
{
    ASSERT( !IsSceneGeomCollectionActive() && !ShadowSubmission().IsActive() );

    PrivateInst& inst = s_RegisteredInsts( hInst );
    ASSERT( inst.m_Type == GeomType::Rigid );
    xhandle const    hGeom = inst.m_GeomHandle;
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    xbool const      bUnregisterGeom = ( RegisteredGeom.m_nReferenceCount == 1 );

    RemovePrivateInst( hInst );

    if ( bUnregisterGeom )
    {
        UnregisterRigidGeom( hGeom );
    }
}

//=========================================================================

render::GeometryInstanceHandle render::RegisterSkinInstance( skin_geom& geom )
{
    ASSERT( !IsSceneGeomCollectionActive() && !ShadowSubmission().IsActive() );

    xhandle hGeom = FindRegisteredGeom( geom );
    if ( hGeom.IsNull() )
    {
        hGeom = RegisterSkinGeom( geom );
    }

    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    ASSERT( RegisteredGeom.m_pGeom == &geom );
    ASSERT( RegisteredGeom.m_Type == GeomType::Skin );

    render::GeometryInstanceHandle handle = AddPrivateInst( hGeom, GeomType::Skin );

    return handle;
}

//=========================================================================

void render::UnregisterSkinInstance( GeometryInstanceHandle hInst )
{
    ASSERT( !IsSceneGeomCollectionActive() && !ShadowSubmission().IsActive() );

    PrivateInst& inst = s_RegisteredInsts( hInst );
    ASSERT( inst.m_Type == GeomType::Skin );
    xhandle const    hGeom = inst.m_GeomHandle;
    PrivateGeom& RegisteredGeom = s_RegisteredGeom( hGeom );
    xbool const      bUnregisterGeom = ( RegisteredGeom.m_nReferenceCount == 1 );

    RemovePrivateInst( hInst );

    if ( bUnregisterGeom )
    {
        UnregisterSkinGeom( hGeom );
    }
}

//=========================================================================

geom const* render::GetGeom( GeometryInstanceHandle hInst )
{
    if ( hInst.IsNull() )
    {
        return NULL;
    }

    PrivateInst& inst = s_RegisteredInsts( hInst );
    return inst.m_pGeom;
}

//=========================================================================

void render::SetCustomFogPalette( texture::handle const& texture, xbool ImmediateSwitch, s32 PaletteIndex )
{
    platform_SetCustomFogPalette( texture, ImmediateSwitch, PaletteIndex );
}

//=========================================================================

xcolor render::GetFogValue( vector3 const& WorldPos, s32 PaletteIndex )
{
    return platform_GetFogValue( WorldPos, PaletteIndex );
}

//=========================================================================

void render::BeginFrame( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::BeginFrame" );

    render::render_ResetFailureState();
    s_isSceneRenderedThisFrame = FALSE;
    ResetActiveRenderScopeCounters();
    platform_BeginEngineFrame();
    g_UIRenderer.BeginFrame();
}

//=========================================================================

xbool render::BeginRenderScope( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::BeginRenderScope" );

    // sort the materials
    s_RegisteredMaterials.Sort();

    // safety check
    ASSERT( eng_InBeginEnd() );
    ASSERT( !IsSceneGeomCollectionActive() && !ShadowSubmission().IsActive() );
    if ( !eng_InBeginEnd() )
    {
        RENDER_THROW( "Render", "BeginRenderScope requires an active engine render scope" );
        return FALSE;
    }
    if ( GetRenderPipeline().GetFrame() || ShadowSubmission().IsActive() )
    {
        RENDER_THROW( "Render", "BeginRenderScope overlaps an active render scope" );
        return FALSE;
    }

    if ( !platform_BeginRenderFrame() )
    {
        return FALSE;
    }
    if ( !GetRenderPipeline().GetFrame() || !GetRenderPipeline().GetSubmission().IsOpen() )
    {
        RENDER_THROW( "Render", "platform opened an invalid scene frame" );
        return FALSE;
    }
    ShadowSubmission().ClearDynamicDraws();
    return TRUE;
}

//=========================================================================

xbool render::SubmitRenderScope( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "render::SubmitRenderScope" );

    // safety check
    ASSERT( eng_InBeginEnd() );
    ASSERT( IsSceneGeomCollectionActive() );
    if ( !eng_InBeginEnd() )
    {
        RENDER_THROW( "Render", "SubmitRenderScope requires an active engine render scope" );
        return FALSE;
    }
    if ( !IsSceneGeomCollectionActive() )
    {
        RENDER_THROW( "Render", "SubmitRenderScope called without an active scene" );
        return FALSE;
    }
    if ( platform_IsFrameAborted() )
    {
        return FALSE;
    }
    static xprofile_counter GeometryDraws = x_GetProfiler().RegisterCounter( "RenderInstances", "RenderCounter" );
    GeometryDraws.Add( FrameSubmission().Geom().GetCount() );

    FrameSubmission().SetEnvironmentCubeMap( s_pCurrentCubeMap );
    FrameSubmission().MarkSceneSubmitted();
    return GetRenderPipeline().GetFrameCompiler().PrepareOrderedDraws( FrameSubmission().GetGeom() );
}

//=========================================================================

xbool render::EndRenderScope( void )
{
    if ( s_isRenderFailureActive )
    {
        return FALSE;
    }

    if ( platform_IsFrameAborted() )
    {
        return FALSE;
    }

    if ( IsSceneGeomCollectionActive() )
    {
        RENDER_THROW( "Render", "scene geometry was not submitted before EndRenderScope" );
        return FALSE;
    }

    if ( !GetRenderPipeline().GetFrame() )
    {
        RENDER_THROW( "Render", "EndRenderScope called without an active scene frame" );
        return FALSE;
    }

    xbool const bSceneCapture = GetActiveRenderScopeDesc().Purpose == RENDER_VIEW_SCENE_CAPTURE;
    if ( !bSceneCapture && !g_UIRenderer.Prepare() )
    {
        return FALSE;
    }

    if ( !platform_EndRenderFrame() )
    {
        return FALSE;
    }

    if ( !bSceneCapture )
        s_isSceneRenderedThisFrame = TRUE;
    if ( !bSceneCapture )
        g_UIRenderer.BeginFrame();
    return TRUE;
}

//=========================================================================

xbool render::EndFrame( xbool RenderEmptyFrame )
{
    if ( s_isRenderFailureActive )
    {
        return FALSE;
    }

    if ( platform_IsFrameAborted() )
    {
        return FALSE;
    }

    if ( IsSceneGeomCollectionActive() || GetRenderPipeline().GetFrame() )
    {
        RENDER_THROW( "Render", "engine frame ended while a scene frame is still active" );
        return FALSE;
    }

    xbool const HasUI = g_UIRenderer.HasPendingCommands();
    if ( !g_UIRenderer.Prepare() )
    {
        return FALSE;
    }

    // Build an overlay scope only for new UI or an explicitly requested empty frame.
    if ( !HasUI && ( s_isSceneRenderedThisFrame || !RenderEmptyFrame ) )
    {
        return TRUE;
    }

    if ( !GetRenderPipeline().BeginUIOnlyFrame() )
    {
        platform_AbortFrame( "failed to begin explicit UI frame" );
        return FALSE;
    }
    if ( !GetRenderPipeline().FinishRenderScope() )
    {
        platform_AbortFrame( "failed to finish explicit UI frame" );
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

void render::ResetAfterException( void )
{
    platform_ResetAfterException();
    ShadowSubmission().Reset();
}

//=========================================================================

void RenderSubmission::AddDynamicGeom( dynamic_geom_draw const& draw )
{
    if( draw.pGpuMesh && !geom_IsGpuMeshValid( *draw.pGpuMesh ) )
    {
        return;
    }

    ASSERT( IsSceneGeomCollectionActive() );
    ASSERT( draw.pVertices );
    ASSERT( draw.pIndices );
    ASSERT( draw.pDiffuseTexture );
    ASSERT( draw.pDamageMask );
    ASSERT( draw.pDamageTexture );
    ASSERT( draw.pDamageUploadPending );
    ASSERT( ( draw.VertexCount > 0 ) && ( draw.VertexCount <= 65535 ) );
    ASSERT( ( draw.IndexCount > 0 ) && ( ( draw.IndexCount % 3 ) == 0 ) );

    if ( !IsSceneGeomCollectionActive() || !draw.pVertices || !draw.pIndices || !draw.pDiffuseTexture || !draw.pDamageMask ||
        !draw.pDamageTexture || !draw.pDamageUploadPending ||
        ( draw.VertexCount <= 0 ) || ( draw.VertexCount > 65535 ) || ( draw.IndexCount <= 0 ) ||
        ( ( draw.IndexCount % 3 ) != 0 ) || ( (*this).DynamicGeom().GetCount() >= kMaxRenderedInsts ) )
    {
        return;
    }

    for ( s32 i = 0; i < draw.IndexCount; ++i )
    {
        if ( draw.pIndices[i] >= draw.VertexCount )
            return;
    }
    u32 PendingDamageSize = 0;
    if ( *draw.pDamageUploadPending &&
         ( !draw.pDamageUpload || !geom_CalculateDamageUploadSize( draw.DamageUploadWidth,
                                                                    draw.DamageUploadHeight,
                                                                    PendingDamageSize ) ) )
    {
        return;
    }

    geom_inst_features const ProjFeatures =
        GetRenderPipeline().GetProjectionTextureRegistry().CollectProjFeatures( draw.RenderFlags, draw.Bounds );
    geom_inst_features const InstFeatures = ResolveInstFeatures( draw.RenderFlags, ProjFeatures );
    if ( !ShouldSubmitGeom( draw.RenderFlags, InstFeatures, draw.Bounds ) )
    {
        return;
    }

    s32 DamageSize = 0;
    if ( draw.pDamageUpload && ( draw.DamageUploadWidth > 0 ) && ( draw.DamageUploadHeight > 0 ) )
    {
        if ( !geom_CalculateDamageUploadSize( draw.DamageUploadWidth, draw.DamageUploadHeight,
                                              PendingDamageSize ) )
            return;
        DamageSize = static_cast<s32>( PendingDamageSize );
    }

    if ( !CanAppendDynamicStorage( m_DynamicVertexStorage.GetCount(), draw.VertexCount,
                                   sizeof( dynamic_geom_vertex ) ) ||
         !CanAppendDynamicStorage( m_DynamicIndexStorage.GetCount(), draw.IndexCount,
                                   sizeof( u16 ) ) )
    {
        return;
    }

    s32 DamageOffset = -1;
    if ( DamageSize > 0 )
    {
        DamageOffset = m_DynamicDamageStorage.GetCount();
        if ( !CanAppendDynamicStorage( DamageOffset, DamageSize, sizeof( u8 ) ) )
            return;
    }

    s32 const VertexOffset = m_DynamicVertexStorage.GetCount();
    s32 const RequiredVertexCount = VertexOffset + draw.VertexCount;
    s32 const IndexOffset = m_DynamicIndexStorage.GetCount();
    s32 const RequiredIndexCount = IndexOffset + draw.IndexCount;
    if( !EnsureDynamicStorageCapacity( m_DynamicVertexStorage, RequiredVertexCount, "DynamicVertices" ) ||
        !EnsureDynamicStorageCapacity( m_DynamicIndexStorage, RequiredIndexCount, "DynamicIndices" ) ||
        ( ( DamageSize > 0 ) &&
          !EnsureDynamicStorageCapacity( m_DynamicDamageStorage, DamageOffset + DamageSize, "DynamicDamage" ) ) )
    {
        return;
    }

    m_DynamicVertexStorage.SetCount( VertexOffset + draw.VertexCount );
    x_memcpy( &m_DynamicVertexStorage[VertexOffset], draw.pVertices,
              sizeof( dynamic_geom_vertex ) * draw.VertexCount );

    m_DynamicIndexStorage.SetCount( IndexOffset + draw.IndexCount );
    x_memcpy( &m_DynamicIndexStorage[IndexOffset], draw.pIndices, sizeof( u16 ) * draw.IndexCount );

    if ( DamageSize > 0 )
    {
        m_DynamicDamageStorage.SetCount( DamageOffset + DamageSize );
        x_memcpy( &m_DynamicDamageStorage[DamageOffset], draw.pDamageUpload, DamageSize );
    }

    dynamic_geom_draw& queued = (*this).DynamicGeom().Append();
    queued = draw;
    queued.pDamageUpload = NULL;
    queued.InstFeatures = InstFeatures;
    queued.Lighting = platform_CalculateClothLighting( draw.Bounds, draw.Ambient );
    queued.Sequence = (*this).NextGeomSequence()++;

    m_DynamicVertexOffsets.Append() = VertexOffset;
    m_DynamicIndexOffsets.Append() = IndexOffset;
    m_DynamicDamageOffsets.Append() = DamageOffset;
}

//=========================================================================

void RenderShadowSubmission::AddDynamicCaster( dynamic_geom_shadow_draw const& draw )
{
    if( draw.pGpuMesh && !geom_IsGpuMeshValid( *draw.pGpuMesh ) )
    {
        return;
    }

    ASSERT( ShadowSubmission().IsActive() );
    if ( !ShadowSubmission().IsActive() || !draw.pVertices || !draw.pIndices || !draw.pDiffuse || !draw.pDamageMask ||
        !draw.pDamageTexture || !draw.pDamageUploadPending ||
        ( draw.VertexCount <= 0 ) || ( draw.VertexCount > 65535 ) || ( draw.IndexCount <= 0 ) ||
        ( ( draw.IndexCount % 3 ) != 0 ) || ( draw.ShadowSourceMask == 0 ) ||
        ( ShadowSubmission().DynamicDraws().GetCount() >= kMaxRenderedInsts ) )
    {
        return;
    }

    for ( s32 i = 0; i < draw.IndexCount; ++i )
    {
        if ( draw.pIndices[i] >= draw.VertexCount )
            return;
    }
    u32 PendingDamageSize = 0;
    if ( *draw.pDamageUploadPending &&
         ( !draw.pDamageUpload || !geom_CalculateDamageUploadSize( draw.DamageUploadWidth,
                                                                    draw.DamageUploadHeight,
                                                                    PendingDamageSize ) ) )
    {
        return;
    }

    if ( !CanAppendDynamicStorage( m_DynamicVertexStorage.GetCount(), draw.VertexCount,
                                   sizeof( dynamic_geom_vertex ) ) ||
         !CanAppendDynamicStorage( m_DynamicIndexStorage.GetCount(), draw.IndexCount,
                                   sizeof( u16 ) ) )
    {
        return;
    }

    s32 DamageSize = 0;
    if ( draw.pDamageUpload && ( draw.DamageUploadWidth > 0 ) && ( draw.DamageUploadHeight > 0 ) )
    {
        if ( !geom_CalculateDamageUploadSize( draw.DamageUploadWidth, draw.DamageUploadHeight,
                                              PendingDamageSize ) )
            return;
        DamageSize = static_cast<s32>( PendingDamageSize );
    }

    s32 DamageOffset = -1;
    if ( DamageSize > 0 )
    {
        DamageOffset = m_DynamicDamageStorage.GetCount();
        if ( !CanAppendDynamicStorage( DamageOffset, DamageSize, sizeof( u8 ) ) )
            return;
    }

    s32 const VertexOffset = m_DynamicVertexStorage.GetCount();
    s32 const RequiredVertexCount = VertexOffset + draw.VertexCount;
    s32 const IndexOffset = m_DynamicIndexStorage.GetCount();
    s32 const RequiredIndexCount = IndexOffset + draw.IndexCount;
    if( !EnsureDynamicStorageCapacity( m_DynamicVertexStorage, RequiredVertexCount, "ShadowDynamicVertices" ) ||
        !EnsureDynamicStorageCapacity( m_DynamicIndexStorage, RequiredIndexCount, "ShadowDynamicIndices" ) ||
        ( ( DamageSize > 0 ) &&
          !EnsureDynamicStorageCapacity( m_DynamicDamageStorage, DamageOffset + DamageSize, "ShadowDynamicDamage" ) ) )
    {
        return;
    }

    m_DynamicVertexStorage.SetCount( VertexOffset + draw.VertexCount );
    x_memcpy( &m_DynamicVertexStorage[VertexOffset], draw.pVertices,
              sizeof( dynamic_geom_vertex ) * draw.VertexCount );

    m_DynamicIndexStorage.SetCount( IndexOffset + draw.IndexCount );
    x_memcpy( &m_DynamicIndexStorage[IndexOffset], draw.pIndices, sizeof( u16 ) * draw.IndexCount );

    if ( DamageSize > 0 )
    {
        m_DynamicDamageStorage.SetCount( DamageOffset + DamageSize );
        x_memcpy( &m_DynamicDamageStorage[DamageOffset], draw.pDamageUpload, DamageSize );
    }

    dynamic_geom_shadow_draw& queued = ShadowSubmission().DynamicDraws().Append();
    queued = draw;
    queued.pDamageUpload = NULL;
    m_DynamicVertexOffsets.Append() = VertexOffset;
    m_DynamicIndexOffsets.Append() = IndexOffset;
    m_DynamicDamageOffsets.Append() = DamageOffset;
}

//=========================================================================

matrix4* RenderShadowSubmission::StoreTransform( matrix4 const& Transform )
{
    if ( !m_isActive || ( m_TransformStorage.GetCount() >= m_TransformStorage.GetCapacity() ) )
        return NULL;
    m_TransformStorage.Append() = Transform;
    return &m_TransformStorage[m_TransformStorage.GetCount() - 1];
}

//=========================================================================

void RenderSubmission::AddRigidInstanceSimple( xhandle hInst, u32 const* pCol, matrix4 const* pL2W,
                                     bbox const& WorldBBox, u32 flags,
                                     render::RenderContributionPolicy ContributionPolicy, xcolor Ambient )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderSkinOnly )
    {
        return;
    }
#endif

    // safety check
    ASSERT( IsSceneGeomCollectionActive() );
    ASSERT( pL2W->IsValid() );

    // grab the useful pointers out
    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( RegisteredInst.m_pGeom );

    // calculate lighting
    RenderLightingHandle Lighting = platform_CalculateRigidLighting( *pL2W, WorldBBox,
                                                                     pCol ? XCOLOR_BLACK : Ambient );

    // collect texture projections
    geom_inst_features const ProjFeatures =
        GetRenderPipeline().GetProjectionTextureRegistry().CollectProjFeatures( flags, WorldBBox );

    // add each of the submeshes to the render list
    for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
    {
        geom::mesh& mesh = pGeom->m_pMesh[iMesh];
        for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];
            xhandle hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle, GeomMaterial.iVirtualMat );
            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            AppendRigidGeomDraw( RegisteredInst, *pGeom, hMat, iSubMesh, pL2W, pCol, Lighting, flags, ProjFeatures,
                                     WorldBBox,
                                     255, FALSE, ContributionPolicy );
        }
    }
}

//=========================================================================

void RenderSubmission::AddRigidInstance( xhandle hInst, u32 const* pCol, matrix4 const* pL2W, u64 mask, u32 flags,
                               s32 alpha, render::RenderContributionPolicy ContributionPolicy, xcolor Ambient )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderSkinOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "RenderSubmission::AddRigidInstance" );

    // safety check
    ASSERT( IsSceneGeomCollectionActive() );
    ASSERT( pL2W->IsValid() );

    // grab the useful pointers out
    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( RegisteredInst.m_pGeom );

    // calculate lighting
    bbox WorldBBox( pGeom->m_BBox );
    WorldBBox.Transform( *pL2W );
    RenderLightingHandle Lighting = platform_CalculateRigidLighting( *pL2W, WorldBBox,
                                                                     pCol ? XCOLOR_BLACK : Ambient );

    // collect texture projections
    geom_inst_features const ProjFeatures =
        GetRenderPipeline().GetProjectionTextureRegistry().CollectProjFeatures( flags, WorldBBox );

    xbool const bFadingAlpha = ( ( flags & render::FADING_ALPHA ) != 0 ) || ( alpha != 255 );
    if ( bFadingAlpha )
    {
        flags |= render::FADING_ALPHA;
    }

    // add the meshes and submeshes to the render list
    s32         iMesh = 0;
    geom::mesh* pMesh = pGeom->m_pMesh;
    geom::mesh* pEndMesh = pMesh + pGeom->m_nMeshes;
    while ( pMesh < pEndMesh )
    {
        // skip this mesh?
        if ( ( mask & 1 ) == 0 )
        {
            pMesh++;
            iMesh++;
            mask >>= 1;
            continue;
        }

        // add each of the submeshes to the render list
        for ( s32 iSubMesh = pMesh->iSubMesh; iSubMesh < pMesh->iSubMesh + pMesh->nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle, GeomMaterial.iVirtualMat );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            s32      iBone = pGeom->m_pSection[SubMesh.iSection].iBone;
            matrix4* pMat = FrameSubmission().StoreTransform( pL2W[iBone] );
            if ( !pMat )
                continue;
            ASSERT( pMat->IsValid() );

            AppendRigidGeomDraw( RegisteredInst, *pGeom, hMat, iSubMesh, pMat, pCol, Lighting, flags, ProjFeatures,
                                     WorldBBox,
                                     static_cast<u8>( alpha ), bFadingAlpha, ContributionPolicy );
        }
        // next mesh
        iMesh++;
        pMesh++;
        mask >>= 1;
    }
}

//=========================================================================

void RenderSubmission::AddRigidInstance( xhandle hInst, u32 const* pCol, matrix4 const* pL2W, u64 mask,
                               u32 vTextureMask, u32 flags, s32 alpha,
                               render::RenderContributionPolicy ContributionPolicy, xcolor Ambient )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderSkinOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "RenderSubmission::AddRigidInstance" );

    // safety check
    ASSERT( IsSceneGeomCollectionActive() );
    ASSERT( pL2W->IsValid() );

    // grab the useful pointers out
    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( RegisteredInst.m_pGeom );

    // calculate lighting
    bbox WorldBBox( pGeom->m_BBox );
    WorldBBox.Transform( *pL2W );
    RenderLightingHandle Lighting = platform_CalculateRigidLighting( *pL2W, WorldBBox,
                                                                     pCol ? XCOLOR_BLACK : Ambient );

    // collect texture projections
    geom_inst_features const ProjFeatures =
        GetRenderPipeline().GetProjectionTextureRegistry().CollectProjFeatures( flags, WorldBBox );

    xbool const bFadingAlpha = ( ( flags & render::FADING_ALPHA ) != 0 ) || ( alpha != 255 );
    if ( bFadingAlpha )
    {
        flags |= render::FADING_ALPHA;
    }

    // calculate the virtual mesh offsets
    s32 vMatOffsets[32];
    CalcVMatOffsets( vMatOffsets, pGeom, vTextureMask );

    // add the meshes and submeshes to the render list
    s32         iMesh = 0;
    geom::mesh* pMesh = pGeom->m_pMesh;
    geom::mesh* pEndMesh = pMesh + pGeom->m_nMeshes;
    while ( pMesh < pEndMesh )
    {
        // skip this mesh?
        if ( ( mask & 1 ) == 0 )
        {
            pMesh++;
            iMesh++;
            mask >>= 1;
            continue;
        }

        // add each of the submeshes to the render list
        for ( s32 iSubMesh = pMesh->iSubMesh; iSubMesh < pMesh->iSubMesh + pMesh->nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle,
                                                        GeomMaterial.iVirtualMat + vMatOffsets[SubMesh.iMaterial] );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            s32      iBone = pGeom->m_pSection[SubMesh.iSection].iBone;
            matrix4* pMat = FrameSubmission().StoreTransform( pL2W[iBone] );
            if ( !pMat )
                continue;
            ASSERT( pMat->IsValid() );

            AppendRigidGeomDraw( RegisteredInst, *pGeom, hMat, iSubMesh, pMat, pCol, Lighting, flags, ProjFeatures,
                                     WorldBBox,
                                     static_cast<u8>( alpha ), bFadingAlpha, ContributionPolicy );
        }
        // next mesh
        iMesh++;
        pMesh++;
        mask >>= 1;
    }
}

//=========================================================================

void RenderSubmission::AddSkinInstance( xhandle hInst, matrix4 const* pBone, s32 BoneCount, u64 mask, u32 vTextureMask, u32 flags,
                              xcolor const& ambient, render::RenderContributionPolicy ContributionPolicy )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderRigidOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "RenderSubmission::AddSkinInstance" );

    // safety check
    ASSERT( IsSceneGeomCollectionActive() );

    // grab the useful pointers out
    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Skin );
    skin_geom* pGeom = static_cast<skin_geom*>( RegisteredInst.m_pGeom );

    if( !pBone || ( BoneCount <= 0 ) || ( BoneCount > pGeom->m_nBones ) )
    {
        RENDER_THROW( "RenderSubmission", "invalid skin pose bone count %d (geometry: %d)", BoneCount, pGeom->m_nBones );
        return;
    }

    // calculate lighting
    RenderLightingHandle Lighting = platform_CalculateSkinLighting( flags, pBone[0], pGeom->m_BBox, ambient );
    xbool const bGlowing = ( flags & render::GLOWING ) != 0;
    xbool const bFadingAlpha = ( ( flags & render::FADING_ALPHA ) != 0 ) || ( !bGlowing && ( ambient.A != 255 ) );
    if ( bFadingAlpha )
    {
        flags |= render::FADING_ALPHA;
    }

    // collect texture projections
    bbox WorldBBox( pGeom->m_BBox );
    WorldBBox.Transform( pBone[0] );
    geom_inst_features const ProjFeatures =
        GetRenderPipeline().GetProjectionTextureRegistry().CollectProjFeatures( flags, WorldBBox );

    // calculate the virtual mesh offsets
    s32 vMatOffsets[32];
    CalcVMatOffsets( vMatOffsets, pGeom, vTextureMask );

    // add the meshes and submeshes to the render list
    for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
    {
        // skip this mesh?
        if ( ( mask & ( u64{ 1 } << iMesh ) ) == 0 )
        {
            continue;
        }

        // add each of the submeshes to the render list
        geom::mesh& mesh = pGeom->m_pMesh[iMesh];
        if( mesh.nBones > BoneCount )
        {
            RENDER_THROW( "RenderSubmission", "skin mesh %d requires %d bones, pose has %d", iMesh, mesh.nBones, BoneCount );
            return;
        }

        for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle,
                                                        GeomMaterial.iVirtualMat + vMatOffsets[SubMesh.iMaterial] );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            AppendSkinGeomDraw( RegisteredInst, *pGeom, hMat, iSubMesh, pBone, BoneCount, Lighting, flags, ProjFeatures,
                                WorldBBox,
                                ambient.A, bFadingAlpha, ContributionPolicy );
        }
    }
}

//=========================================================================

void RenderSubmission::AddSkinInstanceDistorted( xhandle hInst, matrix4 const* pBone, s32 BoneCount, u64 mask, u32 flags,
                                       radian3 const& NormalRot, xcolor ambient )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderRigidOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "RenderSubmission::AddSkinInstance" );

    // safety check
    ASSERT( IsSceneGeomCollectionActive() );

    // grab the useful pointers out
    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Skin );
    skin_geom* pGeom = static_cast<skin_geom*>( RegisteredInst.m_pGeom );

    if( !pBone || ( BoneCount <= 0 ) || ( BoneCount > pGeom->m_nBones ) )
    {
        RENDER_THROW( "RenderSubmission", "invalid skin pose bone count %d (geometry: %d)", BoneCount, pGeom->m_nBones );
        return;
    }

    // calculate lighting
    // TODO: Ignore dynamic lights for "cloaked" objects
    RenderLightingHandle Lighting = platform_CalculateSkinLighting( flags, pBone[0], pGeom->m_BBox, ambient );
    bbox WorldBBox( pGeom->m_BBox );
    WorldBBox.Transform( pBone[0] );
    // add the meshes and submeshes to the render list
    for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
    {
        // skip this mesh?
        if ( ( mask & ( u64{ 1 } << iMesh ) ) == 0 )
        {
            continue;
        }

        // add each of the submeshes to the render list
        geom::mesh& mesh = pGeom->m_pMesh[iMesh];
        if( mesh.nBones > BoneCount )
        {
            RENDER_THROW( "RenderSubmission", "skin mesh %d requires %d bones, pose has %d", iMesh, mesh.nBones, BoneCount );
            return;
        }

        for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];

            // get the material handle info
            xhandle hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle, GeomMaterial.iVirtualMat );

            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

            material& RenderMaterial = s_RegisteredMaterials( hMat );
            render::geom_inst_features const InstFeatures =
                ResolveInstFeatures( flags, 0, &RenderMaterial, GEOM_PASS_DISTORTION );
            if ( !ShouldSubmitGeom( flags, InstFeatures, WorldBBox ) )
            {
                continue;
            }

            f32 SortDepth;
            if ( !ComputeGeomDepth( SortDepth, pGeom->m_BBox, pBone ) )
            {
                continue;
            }

            ASSERT( (*this).Geom().GetCount() < kMaxRenderedInsts );
            if ( (*this).Geom().GetCount() >= kMaxRenderedInsts )
            {
                continue;
            }

            PrivateGeom& RegisteredGeom = s_RegisteredGeom( RegisteredInst.m_GeomHandle );
            geom_draw_item& item = (*this).Geom().Append();
            item.Pass = GEOM_PASS_DISTORTION;
            item.Type = GEOM_DRAW_SKIN;
            item.hRenderGeom = RegisteredGeom.m_RenderGeomHandle;
            item.pMaterial = RenderMaterial.IsDistortion() ? &RenderMaterial : NULL;
            item.MaterialOrder = RenderMaterial.IsDistortion() ? s_RegisteredMaterials.GetIndexByHandle( hMat ) : 0;
            item.iSurface = iSubMesh;
            item.RenderFlags = flags;
            item.InstFeatures = InstFeatures;
            item.Lighting = Lighting;
            item.Data.Skin.pGeom = pGeom;
            item.Data.Skin.pBones = pBone;
            item.Data.Skin.BoneCount = BoneCount;
            item.DistortionNormalRot = NormalRot;
            item.SortDepth = SortDepth;
            item.Sequence = (*this).NextGeomSequence()++;
            item.ShadowSourceIndex = -1;
            item.Alpha = ambient.A;
            item.MaterialOverride = FALSE;
            item.ContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT;
            GetUVOffset( item.UOffset, item.VOffset, pGeom, RenderMaterial );
        }
    }
}

//=========================================================================

void render::SetSceneColorGrade( SceneColorGradeParams const& Params )
{
    platform_SetSceneColorGrade( Params );
}

//=========================================================================

render::SceneColorGradeParams render::GetSceneColorGrade( void )
{
    return platform_GetSceneColorGrade();
}

//=========================================================================

void render::ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff )
{
    platform_ApplySelfIllumGlows( MotionBlurIntensity, GlowCutoff );
}

//=========================================================================

void render::ZFogFilter( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2 )
{
    platform_ZFogFilter( fn, color, param1, param2 );
}

//=========================================================================

void render::ZFogFilter( render::post_falloff_fn fn, s32 PaletteIndex )
{
    platform_ZFogFilter( fn, PaletteIndex );
}

//=========================================================================

void render::AddScreenWarp( vector3 const& WorldPos, f32 radius, f32 WarpAmount )
{
    platform_AddScreenWarp( WorldPos, radius, WarpAmount );
}

//=========================================================================

void render::MotionBlur( f32 intensity )
{
    platform_MotionBlur( intensity );
}

//=========================================================================

void render::MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2,
                        s32 PaletteIndex )
{
    platform_MipFilter( nFilters, offset, fn, color, param1, param2, PaletteIndex );
}

//=========================================================================

void render::MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, texture::handle const& texture,
                        s32 PaletteIndex )
{
    platform_MipFilter( nFilters, offset, fn, texture, PaletteIndex );
}

//=========================================================================

void render::RadialBlur( f32 zoom, radian angle, f32 AlphaSub, f32 AlphaScale )
{
    platform_RadialBlur( zoom, angle, AlphaSub, AlphaScale );
}

//=========================================================================

void render::NoiseFilter( xcolor color )
{
    platform_NoiseFilter( color );
}

//=========================================================================

void render::ScreenFade( xcolor color )
{
    platform_ScreenFade( color );
}

//=========================================================================

material& render::GetMaterial( GeometryInstanceHandle hInst, s32 iSubMesh )
{
    // grab the useful pointers out
    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );

    geom* pGeom = RegisteredInst.m_pGeom;
    ASSERT( pGeom );

    // get the internal registered material from the geometry material
    geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
    geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];
    xhandle         hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle, GeomMaterial.iVirtualMat );
    ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );

    return s_RegisteredMaterials( hMat );
}

//=========================================================================

texture* render::GetVTexture( geom const* pGeom, s32 iMaterial, s32 vTextureMask )
{
    // assume no offset to start with
    s32 vMatOffset = 0;

    // find any virtual textures that might affect this material, and if so
    // the material offset will come directly from the mask
    s32 i;
    for ( i = 0; i < pGeom->m_nVirtualTextures; i++ )
    {
        // grab the associated bits for this vtexture from the texture mask
        s32 offset = vTextureMask & 0xf;
        vTextureMask >>= 4;

        // does this virtual texture affect this material?
        geom::virtual_texture const& vTexture = pGeom->m_pVirtualTextures[i];
        if ( vTexture.MaterialMask & ( 1 << iMaterial ) )
        {
            vMatOffset = offset;
            break;
        }
    }

    // now, using the offset, get the texture (requires a good bit of
    // redirection, but all comes out in the end)
    xhandle hGeom = FindRegisteredGeom( *pGeom );
    ASSERT( !hGeom.IsNull() );

    geom::material const& GeomMat = pGeom->m_pMaterial[iMaterial];
    xhandle               hMat = GetRegisteredMaterialHandle( hGeom, GeomMat.iVirtualMat + vMatOffset );
    material&             mat = s_RegisteredMaterials( hMat );

    return mat.m_DiffuseMap.GetPointer();
}

//=========================================================================

void render::SetAreaCubeMap( cubemap::handle const& CubeMap )
{
    s_pCurrentCubeMap = CubeMap.GetPointer();
}

//=========================================================================

void render::EnableFilterLight( xbool bEnable )
{
    s_isLightFilteringEnabled = bEnable;
}

//=========================================================================

xbool render::IsFilterLightEnabled( void )
{
    return s_isLightFilteringEnabled;
}

//=========================================================================

void render::SetFilterLightColor( xcolor color )
{
    s_FilterLightColor = color;
}

//=========================================================================

xcolor render::GetFilterLightColor( void )
{
    return s_FilterLightColor;
}

//=========================================================================

void RenderShadowSubmission::Reset( void )
{
    m_isActive = FALSE;
    m_SourceCount = 0;
    m_NextSequence = 0;
    m_ShadowDraws.SetCount( 0 );
    m_OrderedShadowDraws.SetCount( 0 );
    ClearDynamicDraws();
    m_TransformStorage.SetCount( 0 );
}

//=========================================================================

void RenderShadowSubmission::Begin( void )
{
    ASSERT( !IsSceneGeomCollectionActive() && !m_isActive );

    m_isActive = TRUE;

    ShadowSubmission().Draws().SetCount( 0 );
    ShadowSubmission().ClearDynamicDraws();
    ShadowSubmission().OrderedDraws().SetCount( 0 );
    m_TransformStorage.SetCount( 0 );
    ShadowSubmission().NextSequence() = 0;

    // clear out any current shadow-map sources
    platform_ClearShadowSourceList();
    ShadowSubmission().SourceCount() = 0;
}

//=========================================================================

void RenderShadowSubmission::End( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "RenderShadowSubmission::End" );

    // safety check
    ASSERT( eng_InBeginEnd() );
    ASSERT( m_isActive );
    m_isActive = FALSE;

    static xprofile_counter ShadowCasterDraws = x_GetProfiler().RegisterCounter( "ShadowCasterDraws", "RenderCounter" );
    static xprofile_counter ShadowSources = x_GetProfiler().RegisterCounter( "ShadowSources", "RenderCounter" );
    ShadowCasterDraws.Add( ShadowSubmission().Draws().GetCount() + ShadowSubmission().DynamicDraws().GetCount() );
    ShadowSources.Add( ShadowSubmission().SourceCount() );

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/FinalizeSources" );
        platform_FinalizeShadowSourceList();
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/SortCasters" );
        for ( s32 i = 0; i < ShadowSubmission().Draws().GetCount(); ++i )
        {
            ShadowSubmission().OrderedDraws().Append() = &ShadowSubmission().Draws()[i];
        }

        if ( ShadowSubmission().OrderedDraws().GetCount() > 1 )
        {
            x_qsort( ShadowSubmission().OrderedDraws().GetPtr(), ShadowSubmission().OrderedDraws().GetCount(),
                     sizeof( geom_draw_item const* ), ShadowCompareFn );
        }
    }

    xbool CastersPrepared;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/RenderCasters" );
        ShadowSubmission().RebindDynamicPointers();
        CastersPrepared = platform_RenderShadowCasters( ShadowSubmission().OrderedDraws(), ShadowSubmission().DynamicDraws() );
    }
    ShadowSubmission().OrderedDraws().SetCount( 0 );
    ShadowSubmission().Draws().SetCount( 0 );
    ShadowSubmission().ClearDynamicDraws();
    if ( !CastersPrepared )
        RENDER_THROW( "ShadowRenderer", "caster preparation failed; submission was discarded" );
}

//=========================================================================

void RenderShadowSubmission::AddPointSource( matrix4 const& L2W, radian fov, f32 LightRadius, f32 LightFalloff,
                             s32 ShadowMapResolution, s32 ShadowPriority, f32 ShadowScore,
                             s32 DynamicLightIndex )
{
    ASSERT( ShadowSubmission().IsActive() );
    platform_AddPointShadowMapSource( L2W, fov, LightRadius, LightFalloff, ShadowMapResolution, ShadowPriority,
                                      ShadowScore, DynamicLightIndex );
    // Keep the legacy source count synchronized with the atlas.
    ShadowSubmission().SourceCount() = GetRenderPipeline().GetShadowMapAtlas().GetSourceCount();
}

//=========================================================================

void RenderShadowSubmission::AddSpotSource( matrix4 const& L2W, radian fov, f32 LightRadius, f32 LightFalloff,
                             s32 ShadowMapResolution, s32 ShadowPriority, f32 ShadowScore,
                             s32 DynamicLightIndex )
{
    ASSERT( ShadowSubmission().IsActive() );
    platform_AddSpotShadowMapSource( L2W, fov, LightRadius, LightFalloff, ShadowMapResolution, ShadowPriority,
                                     ShadowScore, DynamicLightIndex );
    ShadowSubmission().SourceCount() = GetRenderPipeline().GetShadowMapAtlas().GetSourceCount();
}

//=========================================================================

void RenderShadowSubmission::AddRigidCasterSimple( xhandle hInst,
                             matrix4 const*                 pL2W, // will be DMA ref'd to!
                             u64                            ShadowSourceMask )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderSkinOnly )
    {
        return;
    }
#endif

    ASSERT( ShadowSubmission().IsActive() );
    ASSERT( pL2W );
    ASSERT( pL2W->IsValid() );

    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( RegisteredInst.m_pGeom );

    for ( s32 iShadowSource = 0; iShadowSource < ShadowSubmission().SourceCount(); iShadowSource++ )
    {
        if ( ( ShadowSourceMask & ( u64{ 1 } << iShadowSource ) ) == 0 )
        {
            continue;
        }

        for ( s32 iSubMesh = 0; iSubMesh < pGeom->m_nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];
            xhandle hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle, GeomMaterial.iVirtualMat );
            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );
            if ( s_RegisteredMaterials( hMat ).IsAlpha() )
            {
                continue;
            }

            AppendRigidShadowDraw( RegisteredInst, *pGeom, hMat, iSubMesh, pL2W, iShadowSource );
        }
    }
}

//=========================================================================

void RenderShadowSubmission::AddRigidCaster( xhandle hInst, matrix4 const* pL2W, u64 mask, u64 ShadowSourceMask )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderSkinOnly )
    {
        return;
    }
#endif

    ASSERT( ShadowSubmission().IsActive() );
    ASSERT( pL2W );
    ASSERT( pL2W->IsValid() );

    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Rigid );
    rigid_geom* pGeom = static_cast<rigid_geom*>( RegisteredInst.m_pGeom );

    geom::mesh* pMesh = pGeom->m_pMesh;
    geom::mesh* pEndMesh = pMesh + pGeom->m_nMeshes;
    while ( pMesh < pEndMesh )
    {
        if ( ( mask & 1 ) == 0 )
        {
            pMesh++;
            mask >>= 1;
            continue;
        }

        for ( s32 iSubMesh = pMesh->iSubMesh; iSubMesh < pMesh->iSubMesh + pMesh->nSubMeshes; iSubMesh++ )
        {
            geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
            geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];
            xhandle hMat = GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle, GeomMaterial.iVirtualMat );
            ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );
            if ( s_RegisteredMaterials( hMat ).IsAlpha() )
            {
                continue;
            }

            ASSERT( ( RegisteredInst.m_GeomHandle >= 0 ) &&
                    ( RegisteredInst.m_GeomHandle < kMaxRegisteredGeoms ) );
            ASSERT( ( iSubMesh >= 0 ) && ( iSubMesh < 256 ) );

            s32 iBone = pGeom->m_pSection[SubMesh.iSection].iBone;

            matrix4* pMat = ShadowSubmission().StoreTransform( *( pL2W + iBone ) );
            if ( !pMat )
                continue;
            ASSERT( pMat->IsValid() );

            for ( s32 iShadowSource = 0; iShadowSource < ShadowSubmission().SourceCount(); iShadowSource++ )
            {
                if ( ( ShadowSourceMask & ( u64{ 1 } << iShadowSource ) ) == 0 )
                {
                    continue;
                }

                AppendRigidShadowDraw( RegisteredInst, *pGeom, hMat, iSubMesh, pMat, iShadowSource );
            }
        }

        pMesh++;
        mask >>= 1;
    }
}

//=========================================================================

void RenderShadowSubmission::AddSkinCaster( xhandle hInst, matrix4 const* pBone, s32 nBone, u64 mask,
                             u64 ShadowSourceMask )
{
#ifndef X_RETAIL
    if ( GetRenderPipeline().GetDebugOptions().RenderRigidOnly )
    {
        return;
    }
#endif

    X_PROFILE_SCOPE_CATEGORY( "Context", "RenderShadowSubmission::AddSkinCaster" );

    // safety check
    ASSERT( ShadowSubmission().IsActive() );

    // grab the useful pointers out
    PrivateInst& RegisteredInst = s_RegisteredInsts( hInst );
    ASSERT( RegisteredInst.m_Type == GeomType::Skin );
    skin_geom* pGeom = static_cast<skin_geom*>( RegisteredInst.m_pGeom );
    ASSERT( pBone );
    ASSERT( nBone > 0 );
    if ( !pBone || ( nBone <= 0 ) )
    {
        return;
    }

    // for each shadow source, add the meshes and submeshes to the render list
    for ( s32 iShadowSource = 0; iShadowSource < ShadowSubmission().SourceCount(); iShadowSource++ )
    {
        if ( ( ShadowSourceMask & ( u64{ 1 } << iShadowSource ) ) == 0 )
        {
            continue;
        }

        for ( s32 iMesh = 0; iMesh < pGeom->m_nMeshes; iMesh++ )
        {
            // skip this mesh?
            if ( ( mask & ( u64{ 1 } << iMesh ) ) == 0 )
            {
                continue;
            }

            // add each of the submeshes to the render list
            geom::mesh& mesh = pGeom->m_pMesh[iMesh];
            for ( s32 iSubMesh = mesh.iSubMesh; iSubMesh < mesh.iSubMesh + mesh.nSubMeshes; iSubMesh++ )
            {
                // range safety check for the sort key
                ASSERT( ( RegisteredInst.m_GeomHandle >= 0 ) &&
                        ( RegisteredInst.m_GeomHandle < kMaxRegisteredGeoms ) );
                ASSERT( ( iSubMesh >= 0 ) && ( iSubMesh < 256 ) );

                // don't let alpha cast shadows
                geom::submesh&  SubMesh = pGeom->m_pSubMesh[iSubMesh];
                geom::material& GeomMaterial = pGeom->m_pMaterial[SubMesh.iMaterial];
                xhandle         hMat =
                    GetRegisteredMaterialHandle( RegisteredInst.m_GeomHandle, GeomMaterial.iVirtualMat );
                ASSERT( ( hMat >= 0 ) && ( hMat < kMaxRegisteredMaterials ) );
                if ( s_RegisteredMaterials( hMat ).IsAlpha() )
                {
                    continue;
                }

                AppendSkinShadowDraw( RegisteredInst, *pGeom, hMat, iSubMesh, pBone, nBone, iShadowSource );
            }
        }
    }
}
