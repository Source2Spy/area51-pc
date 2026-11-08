//=========================================================================
//
//  FrameGraph_Compile.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameGraph_Private.hpp"
#include "e_Engine.hpp"
#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static u64 HashTransientCompatibility( const RenderResourceDesc& Desc )
{
    u64       Hash   = 1469598103934665603ull;
    const u8* pBytes = reinterpret_cast<const u8*>( &Desc.Extent );

    for( s32 i = 0; i < static_cast<s32>( sizeof( Desc.Extent ) ); ++i )
    {
        Hash ^= pBytes[i];
        Hash *= 1099511628211ull;
    }

    const void* pValues[] =
    {
        &Desc.Type, &Desc.Buffer.Size, &Desc.Buffer.Stride, &Desc.Buffer.UsageFlags,
        &Desc.Format, &Desc.MipCount, &Desc.SampleCount, &Desc.ArrayLayers, &Desc.ViewMask, &Desc.Usage, &Desc.Lifetime
    };
    const u32 ValueSizes[] =
    {
        sizeof( Desc.Type ), sizeof( Desc.Buffer.Size ), sizeof( Desc.Buffer.Stride ), sizeof( Desc.Buffer.UsageFlags ),
        sizeof( Desc.Format ), sizeof( Desc.MipCount ), sizeof( Desc.SampleCount ), sizeof( Desc.ArrayLayers ), sizeof( Desc.ViewMask ),
        sizeof( Desc.Usage ), sizeof( Desc.Lifetime )
    };

    for( u32 ValueIndex = 0; ValueIndex < ARRAYSIZE( pValues ); ++ValueIndex )
    {
        pBytes = reinterpret_cast<const u8*>( pValues[ValueIndex] );

        for( u32 ByteIndex = 0; ByteIndex < ValueSizes[ValueIndex]; ++ByteIndex )
        {
            Hash ^= pBytes[ByteIndex];
            Hash *= 1099511628211ull;
        }
    }

    return Hash;
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

s32 FrameGraphBuilder::CompareResourceFirstUse( const void* pA, const void* pB )
{
    const Resource& A = **static_cast<Resource* const*>( pA );
    const Resource& B = **static_cast<Resource* const*>( pB );

    if( A.FirstUse != B.FirstUse )
    {
        return A.FirstUse < B.FirstUse ? -1 : 1;
    }

    return A.Handle.Index < B.Handle.Index ? -1 : ( A.Handle.Index > B.Handle.Index ? 1 : 0 );
}

//=========================================================================

xbool FrameGraphBuilder::StoresResource( const Pass& PassData, RenderResourceHandle Handle )
{
    if( PassData.Desc.Depth.IsValid() && PassData.Desc.Depth.Index == Handle.Index )
    {
        return PassData.Desc.DepthStoreOp != RTARGET_STORE_DONT_CARE;
    }

    if( ( PassData.Desc.Color.IsValid() && PassData.Desc.Color.Index == Handle.Index ) ||
        ( PassData.Desc.Color1.IsValid() && PassData.Desc.Color1.Index == Handle.Index ) ||
        ( PassData.Desc.Color2.IsValid() && PassData.Desc.Color2.Index == Handle.Index ) )
    {
        return PassData.Desc.ColorStoreOp == RTARGET_STORE_STORE ||
               PassData.Desc.ColorStoreOp == RTARGET_STORE_RESOLVE_AND_STORE;
    }

    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::Compile( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "FrameGraph/Compile" );

    static xprofile_counter FrameGraphCompiles =
        x_GetProfiler().RegisterCounter( "FrameGraphCompiles", "RenderCounter" );
    static xprofile_counter FrameGraphPasses = x_GetProfiler().RegisterCounter( "FrameGraphPasses", "RenderCounter" );
    static xprofile_counter FrameGraphResources =
        x_GetProfiler().RegisterCounter( "FrameGraphResources", "RenderCounter" );
    static xprofile_counter FrameGraphTransientAliasSearches =
        x_GetProfiler().RegisterCounter( "FrameGraphTransientAliasSearches", "RenderCounter" );
    static xprofile_counter FrameGraphTransientAliases =
        x_GetProfiler().RegisterCounter( "FrameGraphTransientAliases", "RenderCounter" );

    FrameGraphCompiles.Add();
    FrameGraphPasses.Add( m_Passes.GetCount() );
    FrameGraphResources.Add( m_GraphResources.GetCount() );

    u64 TransientAliasSearches = 0;
    u32 TransientAliasCount    = 0;

    if( m_isCompiled || m_isExecuted )
    {
        RENDER_THROW( "FrameGraph", "Compile may only be called once per frame" );
        return FALSE;
    }

    if( m_pLastError )
    {
        return FALSE;
    }

    if( !m_pFrame )
    {
        Fail( "render graph has no active frame" );
        return FALSE;
    }

    if( !IsHandleValid( m_Output ) )
    {
        Fail( "render graph has no final output" );
        return FALSE;
    }

    // Rebuild resource lifetimes and pass dependencies for this frame.
    for( s32 R = 0; R < m_GraphResources.GetCount(); ++R )
    {
        Resource* pResource = m_GraphResources[R];
        pResource->Readers.SetCount( 0 );
        pResource->Producer = INVALID_RENDER_PASS;
        pResource->FirstUse = 0xffffffffu;
        pResource->LastUse  = 0;
    }

    xbool PresentSeen = FALSE;

    for( s32 P = 0; P < m_Passes.GetCount(); ++P )
    {
        Pass& PassData     = *m_Passes[P];
        m_pFailurePass     = PassData.Desc.pDebugName;
        m_pFailureResource = NULL;
        PassData.Dependencies.SetCount( 0 );
        PassData.Dependents.SetCount( 0 );
        PassData.Live = PassData.KeepAlive;

        const xbool HasColor  = PassData.Desc.Color.IsValid();
        const xbool HasColor1 = PassData.Desc.Color1.IsValid();
        const xbool HasColor2 = PassData.Desc.Color2.IsValid();
        const xbool HasDepth  = PassData.Desc.Depth.IsValid();

        if( PassData.Desc.Type < RENDER_PASS_GRAPHICS || PassData.Desc.Type > RENDER_PASS_PRESENT )
        {
            Fail( "invalid render pass type" );
            return FALSE;
        }

        if( PassData.Desc.Type == RENDER_PASS_COMPUTE )
        {
            if( HasColor || HasColor1 || HasColor2 || HasDepth || PassData.Desc.ColorResolve.IsValid() ||
                PassData.Desc.Source.IsValid() || PassData.Desc.Destination.IsValid() ||
                PassData.Desc.CompositePipelineCount || !PassData.Desc.pComputeShader ||
                !( *PassData.Desc.pComputeShader ) || PassData.Desc.pComputeShader->Stage != SHADER_STAGE_COMPUTE )
            {
                Fail( "compute pass requires a compute shader and no graphics attachments" );
                return FALSE;
            }

            u32 OutputResourceCount = 0;
            for( u32 Kind = 0; Kind < 2; ++Kind )
            {
                const RenderComputeBinding* pBindings = Kind ? PassData.Desc.ComputeBuffers : PassData.Desc.ComputeTextures;
                const u32 Count = Kind ? PassData.Desc.ComputeBufferCount : PassData.Desc.ComputeTextureCount;
                for( u32 i = 0; i < Count; ++i )
                {
                    const RenderResourceDesc* pDesc = GetDesc( pBindings[i].Resource );
                    if( !pDesc || pDesc->IsBuffer() != ( Kind != 0 ) ||
                        ( Kind && ( pBindings[i].MipLevel || pBindings[i].Layer ) ) ||
                        ( !Kind && ( pDesc->SampleCount != 1 || pBindings[i].MipLevel >= pDesc->MipCount ||
                                     pBindings[i].Layer >= pDesc->ArrayLayers ) ) ||
                        ( ( pBindings[i].ReadWrite || pBindings[i].PreserveContents ) && pBindings[i].Cycle ) ||
                        ( ( pBindings[i].ReadWrite || pBindings[i].PreserveContents ) &&
                          pDesc->Lifetime == RENDER_RESOURCE_PERSISTENT && !pDesc->PreserveContents ) )
                    {
                        Fail( "invalid compute output binding or content preservation" );
                        return FALSE;
                    }

                    xbool RepeatedResource = FALSE;
                    for( u32 j = 0; j < i; ++j )
                    {
                        if( pBindings[j].Resource.Index == pBindings[i].Resource.Index )
                        {
                            RepeatedResource = TRUE;
                            if( Kind ||
                                ( pBindings[j].MipLevel == pBindings[i].MipLevel &&
                                  pBindings[j].Layer == pBindings[i].Layer ) ||
                                pBindings[j].Cycle || pBindings[i].Cycle )
                            {
                                Fail( "compute output subresources overlap or cycle a shared texture" );
                                return FALSE;
                            }
                        }
                    }

                    if( !RepeatedResource )
                    {
                        ++OutputResourceCount;
                    }
                }
            }

            if( PassData.Writes.GetCount() != static_cast<s32>( OutputResourceCount ) )
            {
                Fail( "compute writes must match writable bindings" );
                return FALSE;
            }
        }
        else if( PassData.Desc.pComputeShader || PassData.Desc.ComputeTextureCount || PassData.Desc.ComputeBufferCount )
        {
            Fail( "compute bindings require a compute pass" );
            return FALSE;
        }

        if( PassData.Desc.Type == RENDER_PASS_TRANSFER &&
            ( HasColor || HasColor1 || HasColor2 || HasDepth || PassData.Desc.ColorResolve.IsValid() ||
              PassData.Desc.Color1Resolve.IsValid() || PassData.Desc.Color2Resolve.IsValid() ||
              PassData.Desc.Source.IsValid() || PassData.Desc.Destination.IsValid() ||
              PassData.Desc.CompositePipelineCount ) )
        {
            Fail( "transfer pass cannot have graphics attachments or copy-pass bindings" );
            return FALSE;
        }

        if( ( HasColor && !IsHandleValid( PassData.Desc.Color ) ) ||
            ( HasColor1 && !IsHandleValid( PassData.Desc.Color1 ) ) ||
            ( HasColor2 && !IsHandleValid( PassData.Desc.Color2 ) ) ||
            ( PassData.Desc.ColorResolve.IsValid() && !IsHandleValid( PassData.Desc.ColorResolve ) ) ||
            ( PassData.Desc.Color1Resolve.IsValid() && !IsHandleValid( PassData.Desc.Color1Resolve ) ) ||
            ( PassData.Desc.Color2Resolve.IsValid() && !IsHandleValid( PassData.Desc.Color2Resolve ) ) ||
            ( HasDepth && !IsHandleValid( PassData.Desc.Depth ) ) ||
            ( PassData.Desc.Source.IsValid() && !IsHandleValid( PassData.Desc.Source ) ) ||
            ( PassData.Desc.Destination.IsValid() && !IsHandleValid( PassData.Desc.Destination ) ) )
        {
            Fail( "render pass contains a stale attachment handle" );
            return FALSE;
        }

        // LOAD requires a retained backing texture; cycling history attachments
        // need an explicit copy before read/modify/write.
        if( ( HasColor && PassData.Desc.ColorLoadOp == RTARGET_LOAD_LOAD &&
              m_GraphResources[PassData.Desc.Color.Index]->Desc.Lifetime == RENDER_RESOURCE_PERSISTENT &&
              !m_GraphResources[PassData.Desc.Color.Index]->Desc.PreserveContents ) ||
            ( HasColor1 && PassData.Desc.ColorLoadOp == RTARGET_LOAD_LOAD &&
              m_GraphResources[PassData.Desc.Color1.Index]->Desc.Lifetime == RENDER_RESOURCE_PERSISTENT &&
              !m_GraphResources[PassData.Desc.Color1.Index]->Desc.PreserveContents ) ||
            ( HasColor2 && PassData.Desc.ColorLoadOp == RTARGET_LOAD_LOAD &&
              m_GraphResources[PassData.Desc.Color2.Index]->Desc.Lifetime == RENDER_RESOURCE_PERSISTENT &&
              !m_GraphResources[PassData.Desc.Color2.Index]->Desc.PreserveContents ) ||
            ( HasDepth && PassData.Desc.DepthLoadOp == RTARGET_LOAD_LOAD &&
              m_GraphResources[PassData.Desc.Depth.Index]->Desc.Lifetime == RENDER_RESOURCE_PERSISTENT &&
              !m_GraphResources[PassData.Desc.Depth.Index]->Desc.PreserveContents ) )
        {
            Fail( "persistent attachment cannot use LOAD with frames in flight; use an explicit history copy" );
            return FALSE;
        }

        for( s32 i = 0; i < PassData.Accesses.GetCount(); ++i )
        {
            const PassAccess&         Access       = PassData.Accesses[i];
            const RenderResourceDesc& ResourceData = m_GraphResources[Access.Resource]->Desc;
            m_pFailureResource                     = ResourceData.pDebugName;

            const u32 BufferAccess = RENDER_RESOURCE_ACCESS_VERTEX_READ | RENDER_RESOURCE_ACCESS_INDEX_READ |
                                     RENDER_RESOURCE_ACCESS_INDIRECT_READ;
            const u32 StorageAccess = RENDER_RESOURCE_ACCESS_STORAGE_READ | RENDER_RESOURCE_ACCESS_STORAGE_WRITE |
                                      RENDER_RESOURCE_ACCESS_STORAGE_READ_WRITE;
            if( ( ( Access.Flags & ( RENDER_RESOURCE_ACCESS_VERTEX_READ | RENDER_RESOURCE_ACCESS_INDEX_READ ) ) &&
                  PassData.Desc.Type != RENDER_PASS_GRAPHICS ) ||
                ( !ResourceData.IsBuffer() && ( Access.Flags & StorageAccess ) &&
                  PassData.Desc.Type != RENDER_PASS_COMPUTE ) )
            {
                Fail( "resource access is incompatible with the pass type" );
                return FALSE;
            }

            if( PassData.Desc.Type == RENDER_PASS_TRANSFER &&
                ( Access.Flags & ~( RENDER_RESOURCE_ACCESS_COPY_SOURCE | RENDER_RESOURCE_ACCESS_COPY_DEST |
                                     RENDER_RESOURCE_ACCESS_PRESERVE ) ) )
            {
                Fail( "transfer pass requires copy resource access" );
                return FALSE;
            }

            if( ResourceData.IsBuffer() )
            {
                const u32 Usage = ResourceData.Buffer.UsageFlags;
                const u32 ReadUsage = PassData.Desc.Type == RENDER_PASS_COMPUTE
                                          ? RBUFFER_USAGE_COMPUTE_STORAGE_READ : RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
                if( ( Access.Flags & ~( BufferAccess | StorageAccess | RENDER_RESOURCE_ACCESS_PRESERVE |
                                        RENDER_RESOURCE_ACCESS_COPY_SOURCE | RENDER_RESOURCE_ACCESS_COPY_DEST ) ) ||
                    ( ( Access.Flags & RENDER_RESOURCE_ACCESS_VERTEX_READ ) && !( Usage & RBUFFER_USAGE_VERTEX ) ) ||
                    ( ( Access.Flags & RENDER_RESOURCE_ACCESS_INDEX_READ ) && !( Usage & RBUFFER_USAGE_INDEX ) ) ||
                    ( ( Access.Flags & RENDER_RESOURCE_ACCESS_INDIRECT_READ ) && !( Usage & RBUFFER_USAGE_INDIRECT ) ) ||
                    ( ( Access.Flags & RENDER_RESOURCE_ACCESS_STORAGE_READ ) && !( Usage & ReadUsage ) ) ||
                    ( ( Access.Flags & RENDER_RESOURCE_ACCESS_STORAGE_WRITE ) &&
                      !( Usage & RBUFFER_USAGE_COMPUTE_STORAGE_WRITE ) ) )
                {
                    Fail( "buffer access is incompatible with its usage" );
                    return FALSE;
                }
            }
            else if( ( Access.Flags & BufferAccess ) ||
                     ( ( Access.Flags & RENDER_RESOURCE_ACCESS_STORAGE_READ ) &&
                       !( ResourceData.Usage & RENDER_RESOURCE_USAGE_STORAGE_READ ) ) ||
                     ( ( Access.Flags & RENDER_RESOURCE_ACCESS_STORAGE_WRITE ) &&
                       !( ResourceData.Usage & ( RENDER_RESOURCE_USAGE_STORAGE_WRITE |
                                                RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE ) ) ) ||
                     ( ( Access.Flags & RENDER_RESOURCE_ACCESS_STORAGE_READ_WRITE ) &&
                       !( ResourceData.Usage & RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE ) ) )
            {
                Fail( "texture access is incompatible with its usage" );
                return FALSE;
            }

            if( ( Access.Flags & ( RENDER_RESOURCE_ACCESS_STORAGE_WRITE | RENDER_RESOURCE_ACCESS_STORAGE_READ_WRITE ) ) &&
                PassData.Desc.Type != RENDER_PASS_COMPUTE )
            {
                Fail( "storage writes require a compute pass" );
                return FALSE;
            }

            if( ( Access.Flags & RENDER_RESOURCE_ACCESS_STORAGE_WRITE ) &&
                ( Access.Flags & ( RENDER_RESOURCE_ACCESS_SAMPLED_READ | RENDER_RESOURCE_ACCESS_STORAGE_READ |
                                   BufferAccess ) ) )
            {
                Fail( "writable compute resource cannot also be bound as an input" );
                return FALSE;
            }

            if( ( Access.Flags & RENDER_RESOURCE_ACCESS_SAMPLED_READ ) &&
                ( !( ResourceData.Usage & RENDER_RESOURCE_USAGE_SAMPLED ) ||
                  ( ResourceData.SampleCount > 1 && !rtarget_IsDepthFormat( ResourceData.Format ) ) ) )
            {
                Fail( "sampled read requires a single-sample shader resource" );
                return FALSE;
            }

            if( !ResourceData.IsBuffer() && ( Access.Flags & RENDER_RESOURCE_ACCESS_COPY_SOURCE ) &&
                !( ResourceData.Usage & RENDER_RESOURCE_USAGE_COPY_SRC ) )
            {
                Fail( "copy source access is missing COPY_SRC usage" );
                return FALSE;
            }

            if( !ResourceData.IsBuffer() && ( Access.Flags & RENDER_RESOURCE_ACCESS_COPY_DEST ) &&
                !( ResourceData.Usage & RENDER_RESOURCE_USAGE_COPY_DST ) )
            {
                Fail( "copy destination access is missing COPY_DST usage" );
                return FALSE;
            }

            if( ( Access.Flags & RENDER_RESOURCE_ACCESS_COLOR_WRITE ) &&
                ( rtarget_IsDepthFormat( ResourceData.Format ) ||
                  !( ResourceData.Usage & RENDER_RESOURCE_USAGE_COLOR ) ) )
            {
                Fail( "color write access has an incompatible resource usage" );
                return FALSE;
            }

            if( ( Access.Flags & RENDER_RESOURCE_ACCESS_DEPTH_WRITE ) &&
                ( !rtarget_IsDepthFormat( ResourceData.Format ) ||
                  !( ResourceData.Usage & RENDER_RESOURCE_USAGE_DEPTH ) ) )
            {
                Fail( "depth write access has an incompatible resource usage" );
                return FALSE;
            }

            if( ( Access.Flags & RENDER_RESOURCE_ACCESS_RESOLVE_SOURCE ) &&
                ( rtarget_IsDepthFormat( ResourceData.Format ) ||
                  !( ResourceData.Usage & RENDER_RESOURCE_USAGE_COLOR ) || ResourceData.SampleCount <= 1 ) )
            {
                Fail( "resolve source access requires a multisample color resource" );
                return FALSE;
            }

            if( ( Access.Flags & RENDER_RESOURCE_ACCESS_RESOLVE_DEST ) &&
                ( rtarget_IsDepthFormat( ResourceData.Format ) ||
                  !( ResourceData.Usage & RENDER_RESOURCE_USAGE_COLOR ) || ResourceData.SampleCount != 1 ) )
            {
                Fail( "resolve destination access requires a single-sample color resource" );
                return FALSE;
            }

            if( ( Access.Flags & RENDER_RESOURCE_ACCESS_SAMPLED_READ ) &&
                ( Access.Flags & ( RENDER_RESOURCE_ACCESS_COLOR_WRITE | RENDER_RESOURCE_ACCESS_DEPTH_WRITE ) ) )
            {
                Fail( "graphics pass cannot sample an attachment it writes" );
                return FALSE;
            }
        }

        if( PassData.Desc.bOptional && PassData.Writes.GetCount() > 0 )
        {
            Fail( "optional pass must not write graph resources; provide a fallback branch" );
            return FALSE;
        }

        if( PresentSeen )
        {
            Fail( "render pass declared after Present" );
            return FALSE;
        }

        if( PassData.Desc.Type == RENDER_PASS_GRAPHICS && !HasColor && !HasColor1 && !HasColor2 && !HasDepth )
        {
            Fail( "graphics pass has no attachment" );
            return FALSE;
        }

        if( ( HasColor1 && !HasColor ) || ( HasColor2 && !HasColor ) ||
            ( PassData.Desc.ColorResolve.IsValid() && !HasColor ) ||
            ( PassData.Desc.Color1Resolve.IsValid() && !HasColor1 ) ||
            ( PassData.Desc.Color2Resolve.IsValid() && !HasColor2 ) )
        {
            Fail( "secondary or resolve color attachment has no primary attachment" );
            return FALSE;
        }

        if( ( HasColor1 || HasColor2 || PassData.Desc.ColorResolve.IsValid() || PassData.Desc.Color1Resolve.IsValid() ||
              PassData.Desc.Color2Resolve.IsValid() ) &&
            PassData.Desc.Type != RENDER_PASS_GRAPHICS )
        {
            Fail( "secondary and resolve color attachments require a graphics pass" );
            return FALSE;
        }

        if( PassData.Desc.Type == RENDER_PASS_COPY &&
            ( !PassData.Desc.Source.IsValid() || !PassData.Desc.Destination.IsValid() ) )
        {
            Fail( "copy pass requires source and destination" );
            return FALSE;
        }

        if( PassData.Desc.Type == RENDER_PASS_COPY )
        {
            const RenderResourceDesc& SourceData      = m_GraphResources[PassData.Desc.Source.Index]->Desc;
            const RenderResourceDesc& DestinationData = m_GraphResources[PassData.Desc.Destination.Index]->Desc;

            if( !( SourceData.Usage & RENDER_RESOURCE_USAGE_COPY_SRC ) ||
                !( DestinationData.Usage & RENDER_RESOURCE_USAGE_COPY_DST ) ||
                SourceData.Format != DestinationData.Format || SourceData.SampleCount != DestinationData.SampleCount )
            {
                Fail( "copy pass has incompatible resources" );
                return FALSE;
            }
        }

        if( PassData.Desc.Type == RENDER_PASS_PRESENT && ( !HasColor || HasDepth || !PassData.Desc.Source.IsValid() ) )
        {
            Fail( "Present requires an output color attachment and an input source" );
            return FALSE;
        }

        PresentSeen = PassData.Desc.Type == RENDER_PASS_PRESENT;

        if( ( HasColor && !PassDeclaresWrite( static_cast<RenderPassId>( P ), PassData.Desc.Color ) ) ||
            ( HasColor1 && !PassDeclaresWrite( static_cast<RenderPassId>( P ), PassData.Desc.Color1 ) ) ||
            ( HasColor2 && !PassDeclaresWrite( static_cast<RenderPassId>( P ), PassData.Desc.Color2 ) ) ||
            ( PassData.Desc.ColorResolve.IsValid() &&
              !PassDeclaresWrite( static_cast<RenderPassId>( P ), PassData.Desc.ColorResolve ) ) ||
            ( PassData.Desc.Color1Resolve.IsValid() &&
              !PassDeclaresWrite( static_cast<RenderPassId>( P ), PassData.Desc.Color1Resolve ) ) ||
            ( PassData.Desc.Color2Resolve.IsValid() &&
              !PassDeclaresWrite( static_cast<RenderPassId>( P ), PassData.Desc.Color2Resolve ) ) ||
            ( HasDepth && !PassDeclaresWrite( static_cast<RenderPassId>( P ), PassData.Desc.Depth ) ) )
        {
            Fail( "render pass attachment is not declared as a write" );
            return FALSE;
        }

        for( s32 i = 0; i < PassData.Writes.GetCount(); ++i )
        {
            const u32 Write = PassData.Writes[i];

            if( ( PassData.Desc.Type == RENDER_PASS_GRAPHICS ) &&
                ( ( !HasColor || PassData.Desc.Color.Index != Write ) &&
                  ( !HasColor1 || PassData.Desc.Color1.Index != Write ) &&
                  ( !HasColor2 || PassData.Desc.Color2.Index != Write ) &&
                  ( !PassData.Desc.ColorResolve.IsValid() || PassData.Desc.ColorResolve.Index != Write ) &&
                  ( !PassData.Desc.Color1Resolve.IsValid() || PassData.Desc.Color1Resolve.Index != Write ) &&
                  ( !PassData.Desc.Color2Resolve.IsValid() || PassData.Desc.Color2Resolve.Index != Write ) &&
                  ( !HasDepth || PassData.Desc.Depth.Index != Write ) ) )
            {
                Fail( "render pass write is not an attachment" );
                return FALSE;
            }
        }

        if( HasColor )
        {
            const RenderResourceDesc& ColorData = m_GraphResources[PassData.Desc.Color.Index]->Desc;

            if( rtarget_IsDepthFormat( ColorData.Format ) || !( ColorData.Usage & RENDER_RESOURCE_USAGE_COLOR ) )
            {
                Fail( "color attachment has an incompatible descriptor" );
                return FALSE;
            }
        }

        if( HasColor1 )
        {
            const RenderResourceDesc& ColorData = m_GraphResources[PassData.Desc.Color1.Index]->Desc;

            if( rtarget_IsDepthFormat( ColorData.Format ) || !( ColorData.Usage & RENDER_RESOURCE_USAGE_COLOR ) )
            {
                Fail( "secondary color attachment has an incompatible descriptor" );
                return FALSE;
            }

            const RenderResourceDesc& Primary = m_GraphResources[PassData.Desc.Color.Index]->Desc;

            if( ColorData.Extent.Width != Primary.Extent.Width || ColorData.Extent.Height != Primary.Extent.Height ||
                ColorData.SampleCount != Primary.SampleCount )
            {
                Fail( "color attachments have mismatched extent or sample count" );
                return FALSE;
            }
        }

        if( HasColor2 )
        {
            const RenderResourceDesc& ColorData = m_GraphResources[PassData.Desc.Color2.Index]->Desc;

            if( rtarget_IsDepthFormat( ColorData.Format ) || !( ColorData.Usage & RENDER_RESOURCE_USAGE_COLOR ) )
            {
                Fail( "tertiary color attachment has an incompatible descriptor" );
                return FALSE;
            }

            const RenderResourceDesc& Primary = m_GraphResources[PassData.Desc.Color.Index]->Desc;

            if( ColorData.Extent.Width != Primary.Extent.Width || ColorData.Extent.Height != Primary.Extent.Height ||
                ColorData.SampleCount != Primary.SampleCount )
            {
                Fail( "color attachments have mismatched extent or sample count" );
                return FALSE;
            }
        }

        if( PassData.Desc.ColorResolve.IsValid() )
        {
            const RenderResourceDesc& SourceData = m_GraphResources[PassData.Desc.Color.Index]->Desc;
            const RenderResourceDesc& Resolve    = m_GraphResources[PassData.Desc.ColorResolve.Index]->Desc;

            if( rtarget_IsDepthFormat( Resolve.Format ) || !( Resolve.Usage & RENDER_RESOURCE_USAGE_COLOR ) ||
                SourceData.Format != Resolve.Format || SourceData.Extent.Width != Resolve.Extent.Width ||
                SourceData.Extent.Height != Resolve.Extent.Height || SourceData.SampleCount <= 1 ||

                Resolve.SampleCount != 1 ||
                ( PassData.Desc.ColorStoreOp != RTARGET_STORE_RESOLVE &&
                  PassData.Desc.ColorStoreOp != RTARGET_STORE_RESOLVE_AND_STORE ) )
            {
                Fail( "primary color resolve attachment is incompatible" );
                return FALSE;
            }
        }
        else if( PassData.Desc.ColorStoreOp == RTARGET_STORE_RESOLVE ||
                 PassData.Desc.ColorStoreOp == RTARGET_STORE_RESOLVE_AND_STORE )
        {
            Fail( "primary color resolve store op has no resolve attachment" );
            return FALSE;
        }

        if( PassData.Desc.Color1Resolve.IsValid() )
        {
            const RenderResourceDesc& SourceData = m_GraphResources[PassData.Desc.Color1.Index]->Desc;
            const RenderResourceDesc& Resolve    = m_GraphResources[PassData.Desc.Color1Resolve.Index]->Desc;

            if( rtarget_IsDepthFormat( Resolve.Format ) || !( Resolve.Usage & RENDER_RESOURCE_USAGE_COLOR ) ||
                SourceData.Format != Resolve.Format || SourceData.Extent.Width != Resolve.Extent.Width ||
                SourceData.Extent.Height != Resolve.Extent.Height || SourceData.SampleCount <= 1 ||

                Resolve.SampleCount != 1 ||
                ( PassData.Desc.ColorStoreOp != RTARGET_STORE_RESOLVE &&
                  PassData.Desc.ColorStoreOp != RTARGET_STORE_RESOLVE_AND_STORE ) )
            {
                Fail( "secondary color resolve attachment is incompatible" );
                return FALSE;
            }
        }

        if( PassData.Desc.Color2Resolve.IsValid() )
        {
            const RenderResourceDesc& SourceData = m_GraphResources[PassData.Desc.Color2.Index]->Desc;
            const RenderResourceDesc& Resolve    = m_GraphResources[PassData.Desc.Color2Resolve.Index]->Desc;

            if( rtarget_IsDepthFormat( Resolve.Format ) || !( Resolve.Usage & RENDER_RESOURCE_USAGE_COLOR ) ||
                SourceData.Format != Resolve.Format || SourceData.Extent.Width != Resolve.Extent.Width ||
                SourceData.Extent.Height != Resolve.Extent.Height || SourceData.SampleCount <= 1 ||

                Resolve.SampleCount != 1 ||
                ( PassData.Desc.ColorStoreOp != RTARGET_STORE_RESOLVE &&
                  PassData.Desc.ColorStoreOp != RTARGET_STORE_RESOLVE_AND_STORE ) )
            {
                Fail( "tertiary color resolve attachment is incompatible" );
                return FALSE;
            }
        }

        if( HasDepth )
        {
            const RenderResourceDesc& DepthData = m_GraphResources[PassData.Desc.Depth.Index]->Desc;

            if( !rtarget_IsDepthFormat( DepthData.Format ) || !( DepthData.Usage & RENDER_RESOURCE_USAGE_DEPTH ) )
            {
                Fail( "depth attachment has an incompatible descriptor" );
                return FALSE;
            }

            if( HasColor )
            {
                const RenderResourceDesc& ColorData = m_GraphResources[PassData.Desc.Color.Index]->Desc;

                if( ColorData.Extent.Width != DepthData.Extent.Width ||
                    ColorData.Extent.Height != DepthData.Extent.Height ||
                    ColorData.SampleCount != DepthData.SampleCount )
                {
                    Fail( "render pass attachments have mismatched extent or sample count" );
                    return FALSE;
                }
            }
        }

        for( s32 i = 0; i < PassData.Reads.GetCount(); ++i )
        {
            Resource& ResourceData = *m_GraphResources[PassData.Reads[i]];
            m_pFailureResource     = ResourceData.Desc.pDebugName;

            if( ResourceData.Producer == INVALID_RENDER_PASS && !ResourceData.HasInitialContents )
            {
                Fail( "graph resource is read before it has a producer" );
                return FALSE;
            }

            if( ResourceData.Producer != INVALID_RENDER_PASS &&
                !StoresResource( *m_Passes[ResourceData.Producer], ResourceData.Handle ) )
            {
                Fail( "graph resource contents were discarded by its producer" );
                return FALSE;
            }

            if( ResourceData.Producer != INVALID_RENDER_PASS )
            {
                AddDependency( PassData, ResourceData.Producer );
            }
        }

        for( s32 i = 0; i < PassData.Writes.GetCount(); ++i )
        {
            Resource& ResourceData = *m_GraphResources[PassData.Writes[i]];
            m_pFailureResource     = ResourceData.Desc.pDebugName;

            for( s32 j = 0; j < PassData.Reads.GetCount(); ++j )
            {
                if( PassData.Reads[j] == PassData.Writes[i] && PassData.Desc.Type != RENDER_PASS_GRAPHICS &&
                    !( ( PassData.Desc.Type == RENDER_PASS_COMPUTE || PassData.Desc.Type == RENDER_PASS_TRANSFER ) &&
                       ( PassDeclaresAccess( static_cast<RenderPassId>( P ), ResourceData.Handle,
                                            RENDER_RESOURCE_ACCESS_STORAGE_READ_WRITE ) ||
                         PassDeclaresAccess( static_cast<RenderPassId>( P ), ResourceData.Handle,
                                            RENDER_RESOURCE_ACCESS_PRESERVE ) ) ) )
                {
                    Fail( "pass reads and writes the same resource" );
                    return FALSE;
                }
            }

            if( ResourceData.Producer == INVALID_RENDER_PASS && !ResourceData.HasInitialContents &&
                PassData.Desc.Type == RENDER_PASS_GRAPHICS )
            {
                const xbool isColorAttachment = ( HasColor && PassData.Desc.Color.Index == PassData.Writes[i] ) ||
                                                ( HasColor1 && PassData.Desc.Color1.Index == PassData.Writes[i] ) ||
                                                ( HasColor2 && PassData.Desc.Color2.Index == PassData.Writes[i] );
                const xbool isDepthAttachment = HasDepth && PassData.Desc.Depth.Index == PassData.Writes[i];
                const xbool LoadsUninitializedAttachment =
                    ( isColorAttachment && PassData.Desc.ColorLoadOp == RTARGET_LOAD_LOAD ) ||
                    ( isDepthAttachment && PassData.Desc.DepthLoadOp == RTARGET_LOAD_LOAD );

                if( LoadsUninitializedAttachment )
                {
                    Fail( "render pass loads an attachment before its first producer" );
                    return FALSE;
                }
            }

            if( ResourceData.Producer != INVALID_RENDER_PASS )
            {
                xbool PreservePrevious = FALSE;

                if( PassData.Desc.Type == RENDER_PASS_GRAPHICS )
                {
                    const xbool isColor = ( HasColor && PassData.Desc.Color.Index == PassData.Writes[i] ) ||
                                          ( HasColor1 && PassData.Desc.Color1.Index == PassData.Writes[i] ) ||
                                          ( HasColor2 && PassData.Desc.Color2.Index == PassData.Writes[i] );
                    const xbool isDepth = HasDepth && PassData.Desc.Depth.Index == PassData.Writes[i];
                    PreservePrevious    = ( isColor && PassData.Desc.ColorLoadOp == RTARGET_LOAD_LOAD ) ||
                                       ( isDepth && PassData.Desc.DepthLoadOp == RTARGET_LOAD_LOAD );
                }

                // LOAD consumes the prior version; other writes replace it after outstanding readers.
                if( PreservePrevious )
                {
                    if( !StoresResource( *m_Passes[ResourceData.Producer], ResourceData.Handle ) )
                    {
                        Fail( "render pass loads discarded attachment contents" );
                        return FALSE;
                    }

                    AddDependency( PassData, ResourceData.Producer );
                }
            }

            ResourceData.Readers.SetCount( 0 );
            ResourceData.Producer = static_cast<RenderPassId>( P );
        }
    }

    if( !PresentSeen )
    {
        Fail( "render graph has no Present pass" );
        return FALSE;
    }

    if( m_Passes.GetCount() == 0 || m_Passes[m_Passes.GetCount() - 1]->Desc.Type != RENDER_PASS_PRESENT )
    {
        Fail( "Present must be the final declared pass" );
        return FALSE;
    }

    // Seed liveness from Present, the output, external writes and KeepAlive passes.
    for( s32 P = 0; P < m_Passes.GetCount(); ++P )
    {
        Pass& PassData = *m_Passes[P];

        if( PassData.Desc.Type == RENDER_PASS_PRESENT )
        {
            PassData.Live = TRUE;
        }

        for( s32 W = 0; W < PassData.Writes.GetCount(); ++W )
        {
            if( m_GraphResources[PassData.Writes[W]]->Desc.Lifetime == RENDER_RESOURCE_EXTERNAL )
            {
                PassData.Live = TRUE;
            }
        }
    }

    const Resource& Output = *m_GraphResources[m_Output.Index];

    if( Output.Producer != INVALID_RENDER_PASS )
    {
        m_Passes[Output.Producer]->Live = TRUE;
    }

    m_LiveQueue.SetCount( 0 );

    for( s32 P = 0; P < m_Passes.GetCount(); ++P )
    {
        if( m_Passes[P]->Live )
        {
            m_LiveQueue.Append() = static_cast<RenderPassId>( P );
        }
    }

    for( s32 Head = 0; Head < m_LiveQueue.GetCount(); ++Head )
    {
        const Pass& PassData = *m_Passes[m_LiveQueue[Head]];

        for( s32 D = 0; D < PassData.Dependencies.GetCount(); ++D )
        {
            Pass& Dependency = *m_Passes[PassData.Dependencies[D]];

            if( !Dependency.Live )
            {
                Dependency.Live = TRUE;

                m_LiveQueue.Append() = PassData.Dependencies[D];
            }
        }
    }

    // Only data dependencies keep passes alive. Order surviving accesses separately.
    for( s32 R = 0; R < m_GraphResources.GetCount(); ++R )
    {
        m_GraphResources[R]->Producer = INVALID_RENDER_PASS;
        m_GraphResources[R]->Readers.SetCount( 0 );
    }

    for( s32 P = 0; P < m_Passes.GetCount(); ++P )
    {
        Pass& PassData = *m_Passes[P];

        if( !PassData.Live )
        {
            continue;
        }

        for( s32 i = 0; i < PassData.Reads.GetCount(); ++i )
        {
            Resource& ResourceData = *m_GraphResources[PassData.Reads[i]];

            if( ResourceData.Producer != INVALID_RENDER_PASS )
            {
                AddDependency( PassData, ResourceData.Producer );
            }

            ResourceData.Readers.Append() = static_cast<RenderPassId>( P );
        }

        for( s32 i = 0; i < PassData.Writes.GetCount(); ++i )
        {
            Resource& ResourceData = *m_GraphResources[PassData.Writes[i]];

            if( ResourceData.Producer != INVALID_RENDER_PASS )
            {
                AddDependency( PassData, ResourceData.Producer );
            }

            for( s32 j = 0; j < ResourceData.Readers.GetCount(); ++j )
            {
                if( ResourceData.Readers[j] != static_cast<RenderPassId>( P ) )
                {
                    AddDependency( PassData, ResourceData.Readers[j] );
                }
            }

            ResourceData.Readers.SetCount( 0 );
            ResourceData.Producer = static_cast<RenderPassId>( P );
        }
    }

    // Build reverse edges once for the topological walk.
    for( s32 P = 0; P < m_Passes.GetCount(); ++P )
    {
        const Pass& PassData = *m_Passes[P];

        if( !PassData.Live )
        {
            continue;
        }

        for( s32 D = 0; D < PassData.Dependencies.GetCount(); ++D )
        {
            const RenderPassId Dependency = PassData.Dependencies[D];

            if( m_Passes[Dependency]->Live )
            {
                m_Passes[Dependency]->Dependents.Append() = static_cast<RenderPassId>( P );
            }
        }
    }

    m_Incoming.SetCount( m_Passes.GetCount() );

    for( s32 i = 0; i < m_Incoming.GetCount(); ++i )
    {
        m_Incoming[i] = 0xffffffffu;

        if( !m_Passes[i]->Live )
        {
            continue;
        }

        m_Incoming[i] = 0;

        for( s32 D = 0; D < m_Passes[i]->Dependencies.GetCount(); ++D )
        {
            if( m_Passes[m_Passes[i]->Dependencies[D]]->Live )
            {
                ++m_Incoming[i];
            }
        }
    }

    // Order the live passes by their dependencies.
    m_ExecutionOrder.SetCount( 0 );

    s32 LivePassCount = 0;

    for( s32 P = 0; P < m_Passes.GetCount(); ++P )
    {
        LivePassCount += m_Passes[P]->Live ? 1 : 0;
    }

    m_ReadyQueue.SetCount( 0 );

    for( s32 P = 0; P < m_Passes.GetCount(); ++P )
    {
        if( m_Passes[P]->Live && m_Incoming[P] == 0 )
        {
            m_ReadyQueue.Append() = static_cast<RenderPassId>( P );
        }
    }

    for( s32 Head = 0; Head < m_ReadyQueue.GetCount(); ++Head )
    {
        const RenderPassId EmittedId = m_ReadyQueue[Head];
        m_Incoming[EmittedId]        = 0xffffffffu;
        m_ExecutionOrder.Append()    = EmittedId;

        const Pass& Emitted = *m_Passes[EmittedId];

        for( s32 D = 0; D < Emitted.Dependents.GetCount(); ++D )
        {
            const RenderPassId Dependent = Emitted.Dependents[D];

            if( m_Incoming[Dependent] != 0xffffffffu )
            {
                ASSERT( m_Incoming[Dependent] > 0 );
                --m_Incoming[Dependent];

                if( m_Incoming[Dependent] == 0 )
                {
                    s32 Position = Head + 1;

                    while( Position < m_ReadyQueue.GetCount() && m_ReadyQueue[Position] < Dependent )
                    {
                        ++Position;
                    }

                    m_ReadyQueue.Insert( Position ) = Dependent;
                }
            }
        }
    }

    if( m_ExecutionOrder.GetCount() != LivePassCount )
    {
        Fail( "render pass dependency cycle" );
        return FALSE;
    }

    // Compute resource lifetimes in execution order.
    for( s32 OrderData = 0; OrderData < m_ExecutionOrder.GetCount(); ++OrderData )
    {
        const Pass& PassData = *m_Passes[m_ExecutionOrder[OrderData]];

        for( s32 i = 0; i < PassData.Reads.GetCount(); ++i )
        {
            Resource& ResourceData = *m_GraphResources[PassData.Reads[i]];
            ResourceData.FirstUse  = MIN( ResourceData.FirstUse, static_cast<u32>( OrderData ) );
            ResourceData.LastUse   = static_cast<u32>( OrderData );
        }

        for( s32 i = 0; i < PassData.Writes.GetCount(); ++i )
        {
            Resource& ResourceData = *m_GraphResources[PassData.Writes[i]];
            ResourceData.FirstUse  = MIN( ResourceData.FirstUse, static_cast<u32>( OrderData ) );
            ResourceData.LastUse   = static_cast<u32>( OrderData );
        }
    }

    // Alias live transient resources with disjoint lifetimes.
    m_TransientPhysicalUses.SetCount( 0 );

    if( m_TransientPhysicalUses.GetCapacity() < m_GraphResources.GetCount() )
    {
        m_TransientPhysicalUses.SetCapacity( m_GraphResources.GetCount() );
    }

    const u64 AliasBucketTarget = MAX( u64( 1 ), static_cast<u64>( m_GraphResources.GetCount() ) * 2 );

    if( AliasBucketTarget > 0x7fffffffu )
    {
        Fail( "render graph is too large for transient alias bookkeeping" );
        return FALSE;
    }

    s32 AliasBucketCount = 1;

    while( static_cast<u64>( AliasBucketCount ) < AliasBucketTarget )
    {
        if( AliasBucketCount > ( 0x7fffffff / 2 ) )
        {
            Fail( "render graph transient alias table exceeds xarray limits" );
            return FALSE;
        }

        AliasBucketCount *= 2;
    }

    if( m_TransientAliasBuckets.GetCount() < AliasBucketCount )
    {
        m_TransientAliasBuckets.SetCount( AliasBucketCount );
    }

    for( s32 BucketIndex = 0; BucketIndex < m_TransientAliasBuckets.GetCount(); ++BucketIndex )
    {
        m_TransientAliasBuckets[BucketIndex] = -1;
    }

    m_LiveResources.SetCount( 0 );

    for( s32 R = 0; R < m_GraphResources.GetCount(); ++R )
    {
        if( m_GraphResources[R]->FirstUse != 0xffffffffu )
        {
            m_LiveResources.Append() = m_GraphResources[R];
        }
    }

    if( m_LiveResources.GetCount() > 1 )
    {
        x_qsort( m_LiveResources.GetPtr(), m_LiveResources.GetCount(), sizeof( Resource* ), CompareResourceFirstUse );
    }

    for( s32 R = 0; R < m_LiveResources.GetCount(); ++R )
    {
        Resource& ResourceData = *m_LiveResources[R];

        if( ResourceData.Desc.Lifetime == RENDER_RESOURCE_EXTERNAL && ResourceData.FirstUse != 0xffffffffu &&
            !( ResourceData.pExternalTexture ? GetShaderResource( ResourceData.Handle ) != NULL :
               ResourceData.Desc.IsBuffer() ? GetBuffer( ResourceData.Handle ) != NULL :
                                              m_Resources.Get( ResourceData.Physical ) != NULL ) )
        {
            Fail( "external graph resource was invalidated before execution" );
            return FALSE;
        }

        if( ResourceData.FirstUse == 0xffffffffu || ResourceData.Desc.Lifetime == RENDER_RESOURCE_EXTERNAL )
        {
            continue;
        }

        xbool isAliased = FALSE;

        if( ResourceData.Desc.Lifetime == RENDER_RESOURCE_TRANSIENT )
        {
            const u64 CompatibilityKey = HashTransientCompatibility( ResourceData.Desc );
            const s32 BucketMask       = m_TransientAliasBuckets.GetCount() - 1;
            s32       PhysicalIndex    = m_TransientAliasBuckets[static_cast<s32>( CompatibilityKey ) & BucketMask];

            for( ; PhysicalIndex >= 0; PhysicalIndex = m_TransientPhysicalUses[PhysicalIndex].NextWithKey )
            {
                TransientPhysicalUse& PhysicalData = m_TransientPhysicalUses[PhysicalIndex];
                TransientAliasSearches++;

                if( ( PhysicalData.CompatibilityKey == CompatibilityKey ) &&
                    ( PhysicalData.LastUse < ResourceData.FirstUse ) &&
                    ResourceDescsMatch( PhysicalData.Desc, ResourceData.Desc ) )
                {
                    ResourceData.Physical = PhysicalData.Physical;

                    PhysicalData.LastUse = ResourceData.LastUse;
                    TransientAliasCount++;
                    isAliased = TRUE;
                    break;
                }
            }
        }

        if( !ResourceData.Physical.IsValid() )
        {
            ResourceData.Physical = m_Resources.Acquire( ResourceData.Desc );
        }

        if( !ResourceData.Physical.IsValid() )
        {
            Fail( "failed to allocate a live graph resource" );
            return FALSE;
        }

        if( ResourceData.Desc.Lifetime == RENDER_RESOURCE_TRANSIENT )
        {
            if( !isAliased )
            {
                TransientPhysicalUse& PhysicalData = m_TransientPhysicalUses.Append();
                PhysicalData.Physical              = ResourceData.Physical;
                PhysicalData.Desc                  = ResourceData.Desc;
                PhysicalData.LastUse               = ResourceData.LastUse;
                PhysicalData.CompatibilityKey      = HashTransientCompatibility( ResourceData.Desc );

                const s32 BucketIndex =
                    static_cast<s32>( PhysicalData.CompatibilityKey ) & ( m_TransientAliasBuckets.GetCount() - 1 );
                PhysicalData.NextWithKey             = m_TransientAliasBuckets[BucketIndex];
                m_TransientAliasBuckets[BucketIndex] = m_TransientPhysicalUses.GetCount() - 1;
            }
        }
    }

    FrameGraphTransientAliasSearches.Add( static_cast<f64>( TransientAliasSearches ) );
    FrameGraphTransientAliases.Add( TransientAliasCount );

    // Group compatible attachment passes.
    for( s32 i = 0; i < m_ExecutionOrder.GetCount(); )
    {
        u32 Last = static_cast<u32>( i );

        while( Last + 1u < static_cast<u32>( m_ExecutionOrder.GetCount() ) &&
               CanMergePasses( *m_Passes[m_ExecutionOrder[Last]], *m_Passes[m_ExecutionOrder[Last + 1u]] ) )
        {
            ++Last;
        }

        Pass& PassData         = *m_Passes[m_ExecutionOrder[i]];
        PassData.ColorStore[0] = GetAttachmentStoreOp( PassData.Desc.Color, PassData.Desc.ColorStoreOp, Last );
        PassData.ColorStore[1] = GetAttachmentStoreOp( PassData.Desc.Color1, PassData.Desc.ColorStoreOp, Last );
        PassData.ColorStore[2] = GetAttachmentStoreOp( PassData.Desc.Color2, PassData.Desc.ColorStoreOp, Last );
        PassData.DepthStore    = GetAttachmentStoreOp( PassData.Desc.Depth, PassData.Desc.DepthStoreOp, Last );

        for( u32 j = static_cast<u32>( i ); j <= Last; ++j )
        {
            m_Passes[m_ExecutionOrder[j]]->GroupEnd = Last;
        }

        i = static_cast<s32>( Last + 1u );
    }

    if( !PrewarmCompositePipelines() )
    {
        return FALSE;
    }

    m_isCompiled = TRUE;

    return TRUE;
}

//=========================================================================
} // namespace render
