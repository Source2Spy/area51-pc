//=========================================================================
//
//  GraphicsPipelineCache.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GraphicsPipelineCache.hpp"
#include "FrameGraph/RenderFailure.hpp"

#include "x_debug.hpp"
#include "x_threads.hpp"
#include "x_time.hpp"
#include "x_profile.hpp"

namespace render
{

//=========================================================================
//  STATIC DATA
//=========================================================================

GraphicsPipelineCache::Entry* GraphicsPipelineCache::s_pQueued       = NULL;
GraphicsPipelineCache::Entry* GraphicsPipelineCache::s_pQueuedTail   = NULL;
GraphicsPipelineCache::Entry* GraphicsPipelineCache::s_pActive       = NULL;
xbool                         GraphicsPipelineCache::s_PrewarmFailed = FALSE;
u32                           GraphicsPipelineCache::s_QueuedCount   = 0;
xtick                         GraphicsPipelineCache::s_PrewarmStart  = 0;
xbool                         GraphicsPipelineCache::s_ReportPrewarm = FALSE;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static void LogPipelineDesc( const render_pipeline_desc& Desc )
{
    const GraphicsPipelineKey Key( Desc );
    x_DebugMsg( "  normalized-pipeline-key=%016llx fields=", static_cast<unsigned long long>( Key.Hash() ) );

    for( u32 i = 0; i < Key.Count; ++i )
    {
        x_DebugMsg( "%s%016llx", i ? "," : "", static_cast<unsigned long long>( Key.Values[i] ) );
    }

    x_DebugMsg( "\n" );
    x_DebugMsg( "  shaders(vs=%p ps=%p) topology=%d vertexBuffers=%u elements=%u colors=%u depthFormat=%d "
                "samples=%u sampleMask=0x%x alphaToCoverage=%d\n",
                Desc.Shader.pVertexShader ? Desc.Shader.pVertexShader->pBackend : NULL,
                Desc.Shader.pPixelShader ? Desc.Shader.pPixelShader->pBackend : NULL,
                static_cast<s32>( Desc.Shader.Topology ), Desc.Shader.VertexBufferCount, Desc.Shader.InputElementCount,
                Desc.ColorCount, static_cast<s32>( Desc.DepthFormat ), Desc.SampleCount, Desc.SampleMask,
                Desc.bAlphaToCoverage );

    for( u32 i = 0; i < Desc.ColorCount; ++i )
    {
        const render_color_target_desc& Color = Desc.ColorTargets[i];
        x_DebugMsg( "  color[%u](format=%d blend=%d src=%d dst=%d op=%d srcA=%d dstA=%d opA=%d mask=0x%x)\n", i,
                    static_cast<s32>( Color.Format ), Color.Blend.bBlendEnable,
                    static_cast<s32>( Color.Blend.SrcBlend ), static_cast<s32>( Color.Blend.DestBlend ),
                    static_cast<s32>( Color.Blend.BlendOp ), static_cast<s32>( Color.Blend.SrcBlendAlpha ),
                    static_cast<s32>( Color.Blend.DestBlendAlpha ), static_cast<s32>( Color.Blend.BlendOpAlpha ),
                    Color.Blend.ColorWriteMask );
    }

    x_DebugMsg( "  depth(enable=%d write=%d func=%d stencil=%d readMask=0x%x writeMask=0x%x) "
                "raster(fill=%d cull=%d front=%d bias=%f clamp=%f slope=%f depthClip=%d)\n",
                Desc.Depth.bDepthEnable, Desc.Depth.bDepthWrite, static_cast<s32>( Desc.Depth.DepthFunc ),
                Desc.Depth.bStencilEnable, Desc.Depth.StencilReadMask, Desc.Depth.StencilWriteMask,
                static_cast<s32>( Desc.Raster.FillMode ), static_cast<s32>( Desc.Raster.CullMode ),
                static_cast<s32>( Desc.Raster.FrontFace ), Desc.Raster.DepthBias, Desc.Raster.DepthBiasClamp,
                Desc.Raster.SlopeScaledDepthBias, Desc.Raster.bDepthClipEnable );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

GraphicsPipelineCache::Entry::Entry( void )
    : m_Key( 0 ),
      m_Desc(),
      m_NormalizedKey( m_Desc ),
      m_pVertexShader( NULL ),
      m_pPixelShader( NULL ),
      m_VertexBuffers(),
      m_InputElements(),
      m_Pipeline(),
      m_isPrewarmed( FALSE ),
      m_isUsed( FALSE ),
      m_isQueued( FALSE ),
      m_pOwner( NULL ),
      m_pNextQueued( NULL )
{
}

//=========================================================================

GraphicsPipelineCache::GraphicsPipelineCache( void )
    : m_pDebugName( NULL ),
      m_Entries(),
      m_HashSlots(),
      m_nPrewarmed( 0 ),
      m_nRuntimeHits( 0 ),
      m_nRuntimeMisses( 0 ),
      m_ReportedValidationMisses(),
#if defined( CONFIG_DEBUG )
      m_Mode( GRAPHICS_PIPELINE_CACHE_VALIDATION )
#else
      m_Mode( GRAPHICS_PIPELINE_CACHE_SHIPPING )
#endif
{
    EnsurePreparedHashCapacity( 0 );
}

//=========================================================================

GraphicsPipelineCache::~GraphicsPipelineCache( void )
{
    Reset();
}

//=========================================================================
//  HASH INDEX
//=========================================================================

u32 GraphicsPipelineCache::HashKey( u64 Key )
{
    Key ^= Key >> 30;
    Key *= 0xbf58476d1ce4e5b9ull;
    Key ^= Key >> 27;
    Key *= 0x94d049bb133111ebull;
    Key ^= Key >> 31;

    return static_cast<u32>( Key ^ ( Key >> 32 ) );
}

//=========================================================================

void GraphicsPipelineCache::RebuildHashIndex( void )
{
    const s32 Capacity = m_HashSlots.GetCount();

    if( Capacity <= 0 )
    {
        return;
    }

    for( s32 i = 0; i < Capacity; ++i )
    {
        m_HashSlots[i] = -1;
    }

    const s32 Mask = Capacity - 1;

    for( s32 EntryIndex = 0; EntryIndex < m_Entries.GetCount(); ++EntryIndex )
    {
        u32 Slot = HashKey( m_Entries[EntryIndex]->m_Key ) & static_cast<u32>( Mask );

        while( m_HashSlots[static_cast<s32>( Slot )] >= 0 )
        {
            Slot = ( Slot + 1u ) & static_cast<u32>( Mask );
        }

        m_HashSlots[static_cast<s32>( Slot )] = EntryIndex;
    }
}

//=========================================================================

void GraphicsPipelineCache::EnsureHashCapacity( s32 RequiredCount )
{
    s32 Capacity = m_HashSlots.GetCount();

    if( Capacity <= 0 )
    {
        Capacity = 16;
    }

    while( RequiredCount * 10 >= Capacity * 7 )
    {
        Capacity *= 2;
    }

    if( Capacity != m_HashSlots.GetCount() )
    {
        m_HashSlots.SetCount( Capacity );
        RebuildHashIndex();
    }
}

//=========================================================================

void GraphicsPipelineCache::InsertHashEntry( s32 EntryIndex )
{
    const s32 Capacity = m_HashSlots.GetCount();
    const s32 Mask     = Capacity - 1;
    u32       Slot     = HashKey( m_Entries[EntryIndex]->m_Key ) & static_cast<u32>( Mask );

    while( m_HashSlots[static_cast<s32>( Slot )] >= 0 )
    {
        Slot = ( Slot + 1u ) & static_cast<u32>( Mask );
    }

    m_HashSlots[static_cast<s32>( Slot )] = EntryIndex;
}

//=========================================================================

GraphicsPipelineCache::Entry* GraphicsPipelineCache::FindEntry( const GraphicsPipelineKey&  Normalized,
                                                                const render_pipeline_desc& Desc ) const
{
    const u64 Key      = Normalized.Hash();
    const s32 Capacity = m_HashSlots.GetCount();

    if( !Capacity )
    {
        return NULL;
    }

    const u32 Mask = static_cast<u32>( Capacity - 1 );
    u32       Slot = HashKey( Key ) & Mask;

    for( s32 Probe = 0; Probe < Capacity; ++Probe )
    {
        const s32 Index = m_HashSlots[Slot];

        if( Index < 0 )
        {
            return NULL;
        }

        Entry* pEntry = m_Entries[Index];

        if( pEntry->m_Key == Key && pEntry->m_NormalizedKey == Normalized &&
            pEntry->m_pVertexShader == ( Desc.Shader.pVertexShader ? Desc.Shader.pVertexShader->pBackend : NULL ) &&
            pEntry->m_pPixelShader == ( Desc.Shader.pPixelShader ? Desc.Shader.pPixelShader->pBackend : NULL ) )
        {
            return pEntry;
        }

        Slot = ( Slot + 1 ) & Mask;
    }

    return NULL;
}

//=========================================================================
//  PREPARED PIPELINES
//=========================================================================

void GraphicsPipelineCache::EnsurePreparedHashCapacity( s32 RequiredCount )
{
    s32 Capacity = MAX( m_PreparedBuckets.GetCount(), 256 );

    while( static_cast<u64>( RequiredCount ) * 10 >= static_cast<u64>( Capacity ) * 7 )
    {
        Capacity *= 2;
    }

    if( Capacity == m_PreparedBuckets.GetCount() )
    {
        return;
    }

    m_PreparedBuckets.SetCount( Capacity );

    for( s32 i = 0; i < Capacity; ++i )
    {
        m_PreparedBuckets[i] = -1;
    }

    for( s32 i = 0; i < m_Prepared.GetCount(); ++i )
    {
        PreparedEntry& Prepared = m_Prepared[i];
        const u32 Bucket = HashKey( Prepared.Program.Hash() ^ Prepared.State ) & static_cast<u32>( Capacity - 1 );
        Prepared.Next             = m_PreparedBuckets[Bucket];
        m_PreparedBuckets[Bucket] = i;
    }
}

//=========================================================================

render_pipeline* GraphicsPipelineCache::FindPrepared( const ShaderProgramKey&     Program,
                                                      u64                         State,
                                                      xbool                       IsPrewarm,
                                                      const GraphicsPipelineKey** ppKey )
{
    const u32 Bucket = HashKey( Program.Hash() ^ State ) & static_cast<u32>( m_PreparedBuckets.GetCount() - 1 );

    for( s32 i = m_PreparedBuckets[Bucket]; i >= 0; i = m_Prepared[i].Next )
    {
        const PreparedEntry& Prepared    = m_Prepared[i];
        const Entry&         CachedEntry = *Prepared.pEntry;

        if( Prepared.State == State && Prepared.Program == Program &&
            CachedEntry.m_pVertexShader == CachedEntry.m_Desc.Shader.pVertexShader->pBackend &&
            CachedEntry.m_pPixelShader == CachedEntry.m_Desc.Shader.pPixelShader->pBackend &&
            ( IsPrewarm || CachedEntry.m_Pipeline ) )
        {
            if( !IsPrewarm )
            {
                ++m_nRuntimeHits;
                Prepared.pEntry->m_isUsed = TRUE;
            }

            if( ppKey )
            {
                *ppKey = &Prepared.pEntry->m_NormalizedKey;
            }

            return &Prepared.pEntry->m_Pipeline;
        }
    }

    return NULL;
}

//=========================================================================

void GraphicsPipelineCache::RememberPrepared( const ShaderProgramKey& Program, u64 State, render_pipeline* pPipeline )
{
    if( !pPipeline || FindPrepared( Program, State, TRUE ) )
    {
        return;
    }

    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        if( &m_Entries[i]->m_Pipeline != pPipeline )
        {
            continue;
        }

        EnsurePreparedHashCapacity( m_Prepared.GetCount() + 1 );

        const u32      Bucket     = HashKey( Program.Hash() ^ State ) & static_cast<u32>( m_PreparedBuckets.GetCount() - 1 );
        PreparedEntry& Prepared   = m_Prepared.Append();
        Prepared.Program          = Program;
        Prepared.State            = State;
        Prepared.pEntry           = m_Entries[i];
        Prepared.Next             = m_PreparedBuckets[Bucket];
        m_PreparedBuckets[Bucket] = m_Prepared.GetCount() - 1;

        return;
    }
    ASSERTS( FALSE, "prepared pipeline does not belong to its cache" );
}

//=========================================================================
//  PIPELINE PREWARM
//=========================================================================

render_pipeline* GraphicsPipelineCache::Prewarm( const render_pipeline_desc& Desc )
{
    return FindOrCreate( GraphicsPipelineKey( Desc ), Desc, TRUE );
}

//=========================================================================

render_pipeline* GraphicsPipelineCache::QueuePrewarm( const render_pipeline_desc& Desc )
{
    return FindOrCreate( GraphicsPipelineKey( Desc ), Desc, TRUE, TRUE );
}

//=========================================================================

void GraphicsPipelineCache::BeginPrewarm( void )
{
    s_PrewarmFailed = FALSE;
    s_QueuedCount   = 0;
    s_PrewarmStart  = x_GetTime();
    s_ReportPrewarm = TRUE;
}

//=========================================================================

xbool GraphicsPipelineCache::IsPrewarmComplete( void )
{
    return !s_pQueued && !s_pActive;
}

//=========================================================================

xbool GraphicsPipelineCache::HasPrewarmFailed( void )
{
    return s_PrewarmFailed;
}

//=========================================================================

xbool GraphicsPipelineCache::UpdatePrewarm( void )
{
    if( s_PrewarmFailed )
    {
        return FALSE;
    }

    // Finish active pipeline requests.
    Entry** ppActive = &s_pActive;

    while( *ppActive )
    {
        Entry*                              pEntry = *ppActive;
        const render_pipeline_create_status Status =
            render_PollPipelineCreate( pEntry->m_Request, &pEntry->m_Pipeline );

        if( Status == RENDER_PIPELINE_CREATE_PENDING )
        {
            ppActive = &pEntry->m_pNextQueued;
            continue;
        }

        pEntry->m_isQueued = FALSE;

        if( Status == RENDER_PIPELINE_CREATE_FAILED )
        {
            s_PrewarmFailed = TRUE;
        }

        render_ReleasePipelineCreate( pEntry->m_Request );
        *ppActive             = pEntry->m_pNextQueued;
        pEntry->m_pNextQueued = NULL;

        if( Status == RENDER_PIPELINE_CREATE_CANCELLED )
        {
            render_TrimPipelineCache();
        }

        if( s_PrewarmFailed )
        {
            return FALSE;
        }
    }
    // Start queued pipelines once their shader programs are ready.
    Entry** ppQueued  = &s_pQueued;
    Entry*  pPrevious = NULL;

    while( *ppQueued )
    {
        Entry*                           pEntry = *ppQueued;
        const shader_program_load_status Vertex =
            pEntry->m_Desc.Shader.pVertexShader
                ? shader_PollProgram( const_cast<shader&>( *pEntry->m_Desc.Shader.pVertexShader ) )
                : SHADER_PROGRAM_READY;
        const shader_program_load_status Pixel =
            pEntry->m_Desc.Shader.pPixelShader
                ? shader_PollProgram( const_cast<shader&>( *pEntry->m_Desc.Shader.pPixelShader ) )
                : SHADER_PROGRAM_READY;

        if( ( Vertex != SHADER_PROGRAM_READY && Vertex != SHADER_PROGRAM_PENDING ) ||
            ( Pixel != SHADER_PROGRAM_READY && Pixel != SHADER_PROGRAM_PENDING ) )
        {
            s_PrewarmFailed = TRUE;
            return FALSE;
        }

        if( Vertex == SHADER_PROGRAM_PENDING || Pixel == SHADER_PROGRAM_PENDING )
        {
            pPrevious = pEntry;
            ppQueued  = &pEntry->m_pNextQueued;
            continue;
        }

        *ppQueued = pEntry->m_pNextQueued;

        if( s_pQueuedTail == pEntry )
        {
            s_pQueuedTail = pPrevious;
        }

        pEntry->m_pNextQueued = NULL;

        pEntry->m_pVertexShader =
            pEntry->m_Desc.Shader.pVertexShader ? pEntry->m_Desc.Shader.pVertexShader->pBackend : NULL;
        pEntry->m_pPixelShader =
            pEntry->m_Desc.Shader.pPixelShader ? pEntry->m_Desc.Shader.pPixelShader->pBackend : NULL;

        if( pEntry->m_Pipeline )
        {
            pEntry->m_isQueued = FALSE;
            continue;
        }

        if( !render_BeginPipelineCreate( pEntry->m_Request, pEntry->m_Desc ) )
        {
            pEntry->m_isQueued = FALSE;
            s_PrewarmFailed    = TRUE;

            return FALSE;
        }

        *ppActive = pEntry;
        ppActive  = &pEntry->m_pNextQueued;
    }

    if( IsPrewarmComplete() && s_ReportPrewarm )
    {
        x_DebugMsg( "GraphicsPipelineCache: queued=%u native=%u elapsed=%.2fms failed=%d\n", s_QueuedCount,
                    render_GetPipelineCacheCount(), x_TicksToMs( x_GetTime() - s_PrewarmStart ), s_PrewarmFailed );
        s_ReportPrewarm = FALSE;
    }

    return !s_PrewarmFailed;
}

//=========================================================================

void GraphicsPipelineCache::WaitForPrewarm( void )
{
    while( !IsPrewarmComplete() )
    {
        if( !UpdatePrewarm() )
        {
            return;
        }

        if( s_pActive )
        {
            render_FinishPipelineCreate( s_pActive->m_Request, s_pActive->m_Pipeline );
        }
        else if( s_pQueued )
        {
            x_DelayThread( 1 );
        }
    }
}

//=========================================================================
//  PIPELINE CREATION
//=========================================================================

render_pipeline* GraphicsPipelineCache::GetOrCreate( const render_pipeline_desc& Desc )
{
    return FindOrCreate( GraphicsPipelineKey( Desc ), Desc, FALSE );
}

//=========================================================================

render_pipeline* GraphicsPipelineCache::FindOrCreate( const GraphicsPipelineKey&  Normalized,
                                                      const render_pipeline_desc& Desc,
                                                      xbool                       isPrewarm,
                                                      xbool                       isQueued )
{
    const u64 Key    = Normalized.Hash();
    Entry*    pEntry = FindEntry( Normalized, Desc );

    if( !isPrewarm && ( !pEntry || !pEntry->m_Pipeline ) && m_Mode == GRAPHICS_PIPELINE_CACHE_VALIDATION )
    {
        LogPipelineDesc( Desc );
        RENDER_WARNING( "GraphicsPipelineCache", "pipeline prewarm is incomplete cache=%s key=%llu",
                      m_pDebugName ? m_pDebugName : "Unnamed", static_cast<unsigned long long>( Key ) );
    }

    if( pEntry )
    {
        if( isPrewarm )
        {
            if( !pEntry->m_isPrewarmed )
            {
                RENDER_WARNING( "GraphicsPipelineCache", "late prewarm cache=%s key=%llu pipeline=%s",
                                m_pDebugName ? m_pDebugName : "Unnamed", static_cast<unsigned long long>( Key ),
                                Desc.pDebugName ? Desc.pDebugName : "Unnamed" );
                pEntry->m_isPrewarmed = TRUE;
                ++m_nPrewarmed;
            }
        }
        else
        {
            if( !pEntry->m_Pipeline )
            {
                if( !render_CreatePipeline( pEntry->m_Pipeline, Desc ) )
                {
                    return NULL;
                }

                static xprofile_counter PipelineRuntimeFallbacks =
                    x_GetProfiler().RegisterCounter( "PipelineRuntimeFallbacks", "Renderer" );
                PipelineRuntimeFallbacks.Add();
                ++m_nRuntimeMisses;
                RENDER_WARNING( "GraphicsPipelineCache", "prewarm was too late cache=%s key=%llu pipeline=%s",
                                m_pDebugName, static_cast<unsigned long long>( Key ), Desc.pDebugName );
            }

            ++m_nRuntimeHits;
            pEntry->m_isUsed = TRUE;
        }

        return &pEntry->m_Pipeline;
    }

    pEntry = new Entry;

    if( !pEntry )
    {
        RENDER_THROW( "GraphicsPipelineCache", "failed to allocate pipeline cache entry cache=%s",
                      m_pDebugName ? m_pDebugName : "Unnamed" );
        return NULL;
    }

    pEntry->m_Key           = Key;
    pEntry->m_Desc          = Desc;
    pEntry->m_NormalizedKey = Normalized;

    if( Desc.pDebugName )
    {
        pEntry->m_DebugName       = Desc.pDebugName;
        pEntry->m_Desc.pDebugName = pEntry->m_DebugName;
    }
    // Keep descriptor arrays owned by the cache entry.
    pEntry->m_VertexBuffers.SetCount( Desc.Shader.VertexBufferCount );

    for( u32 i = 0; i < Desc.Shader.VertexBufferCount; ++i )
    {
        pEntry->m_VertexBuffers[i] = Desc.Shader.pVertexBuffers[i];
    }

    pEntry->m_InputElements.SetCount( Desc.Shader.InputElementCount );

    for( u32 i = 0; i < Desc.Shader.InputElementCount; ++i )
    {
        pEntry->m_InputElements[i] = Desc.Shader.pInputElements[i];
    }

    pEntry->m_Desc.Shader.pVertexBuffers = pEntry->m_VertexBuffers.GetCount() ? pEntry->m_VertexBuffers.GetPtr() : NULL;
    pEntry->m_Desc.Shader.pInputElements = pEntry->m_InputElements.GetCount() ? pEntry->m_InputElements.GetPtr() : NULL;
    pEntry->m_pVertexShader              = Desc.Shader.pVertexShader ? Desc.Shader.pVertexShader->pBackend : NULL;
    pEntry->m_pPixelShader               = Desc.Shader.pPixelShader ? Desc.Shader.pPixelShader->pBackend : NULL;
    pEntry->m_isPrewarmed                = isPrewarm;
    pEntry->m_isUsed                     = !isPrewarm;

    pEntry->m_pOwner   = this;
    pEntry->m_isQueued = isQueued;

    if( !isQueued && !render_CreatePipeline( pEntry->m_Pipeline, Desc ) )
    {
        RENDER_THROW( "GraphicsPipelineCache", "failed to create pipeline cache=%s pipeline=%s",
                      m_pDebugName ? m_pDebugName : "Unnamed", Desc.pDebugName ? Desc.pDebugName : "Unnamed" );
        delete pEntry;

        return NULL;
    }

    if( !m_pDebugName )
    {
        m_CacheDebugName = Desc.pDebugName ? Desc.pDebugName : "Unnamed";
        m_pDebugName     = m_CacheDebugName;
    }

    EnsureHashCapacity( m_Entries.GetCount() + 1 );
    m_Entries.Append() = pEntry;
    InsertHashEntry( m_Entries.GetCount() - 1 );

    if( isQueued )
    {
        ++s_QueuedCount;

        if( s_pQueuedTail )
        {
            s_pQueuedTail->m_pNextQueued = pEntry;
        }
        else
        {
            s_pQueued = pEntry;
        }

        s_pQueuedTail = pEntry;
    }

    if( isPrewarm )
    {
        ++m_nPrewarmed;
    }
    else
    {
        static xprofile_counter PipelineRuntimeFallbacks =
            x_GetProfiler().RegisterCounter( "PipelineRuntimeFallbacks", "Renderer" );
        PipelineRuntimeFallbacks.Add();
        ++m_nRuntimeMisses;

        const render_color_target_desc* pColorTarget = Desc.ColorCount ? &Desc.ColorTargets[0] : NULL;

        RENDER_WARNING( "GraphicsPipelineCache",
                        "runtime miss cache=%s key=%llu pipeline=%s "
                        "colors=%u color0=%d depth=%d samples=%u blend=%d "
                        "depthTest=%d depthWrite=%d cull=%d",
                        m_pDebugName, static_cast<unsigned long long>( Key ),
                        Desc.pDebugName ? Desc.pDebugName : "Unnamed", Desc.ColorCount,
                        pColorTarget ? static_cast<s32>( pColorTarget->Format )
                                     : static_cast<s32>( RTARGET_FORMAT_COUNT ),
                        static_cast<s32>( Desc.DepthFormat ), Desc.SampleCount,
                        pColorTarget ? pColorTarget->Blend.bBlendEnable : FALSE, Desc.Depth.bDepthEnable,
                        Desc.Depth.bDepthWrite, static_cast<s32>( Desc.Raster.CullMode ) );
    }

    return &pEntry->m_Pipeline;
}

//=========================================================================
//  CACHE RESET
//=========================================================================

void GraphicsPipelineCache::Reset( void )
{
    m_Prepared.SetCount( 0 );

    for( s32 i = 0; i < m_PreparedBuckets.GetCount(); ++i )
    {
        m_PreparedBuckets[i] = -1;
    }
    // Unlink queued entries before releasing their storage.
    Entry** ppEntry = &s_pQueued;
    s_pQueuedTail   = NULL;

    while( *ppEntry )
    {
        Entry* pEntry = *ppEntry;

        if( pEntry->m_pOwner == this )
        {
            *ppEntry = pEntry->m_pNextQueued;
        }
        else
        {
            s_pQueuedTail = pEntry;
            ppEntry       = &pEntry->m_pNextQueued;
        }
    }
    // Cancel active requests before releasing them.
    for( Entry* pEntry = s_pActive; pEntry; pEntry = pEntry->m_pNextQueued )
    {
        if( pEntry->m_pOwner == this )
        {
            render_CancelPipelineCreate( pEntry->m_Request );
        }
    }

    Entry** ppActive = &s_pActive;

    while( *ppActive )
    {
        Entry* pEntry = *ppActive;

        if( pEntry->m_pOwner != this )
        {
            ppActive = &pEntry->m_pNextQueued;
            continue;
        }

        render_ReleasePipelineCreate( pEntry->m_Request );
        *ppActive             = pEntry->m_pNextQueued;
        pEntry->m_pNextQueued = NULL;
    }
    // Destroy cached pipelines and collect unused prewarm statistics.
    u32 NumUnused = 0;

    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        Entry* pEntry = m_Entries[i];

        if( !pEntry )
        {
            continue;
        }

        if( pEntry->m_isPrewarmed && !pEntry->m_isUsed )
        {
            ++NumUnused;
        }

        render_DestroyPipeline( pEntry->m_Pipeline );
        delete pEntry;
    }

    if( m_nRuntimeMisses || NumUnused )
    {
        x_DebugMsg( "GraphicsPipelineCache: cache=%s prewarmed=%u hits=%u misses=%u unused=%u\n",
                    m_pDebugName ? m_pDebugName : "Unnamed", m_nPrewarmed, m_nRuntimeHits, m_nRuntimeMisses,
                    NumUnused );
    }

    m_nPrewarmed     = 0;
    m_nRuntimeHits   = 0;
    m_nRuntimeMisses = 0;
    m_ReportedValidationMisses.Clear();
    m_Entries.Clear();
    m_HashSlots.Clear();
}

//=========================================================================

} // namespace render
