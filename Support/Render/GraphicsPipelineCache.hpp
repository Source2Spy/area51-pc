//=========================================================================
//
//  GraphicsPipelineCache.hpp
//
//=========================================================================

#ifndef GRAPHICS_PIPELINE_CACHE_HPP
#define GRAPHICS_PIPELINE_CACHE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "e_RenderState.hpp"
#include "e_RenderPipelineKey.hpp"
#include "x_array.hpp"
#include "x_time.hpp"
#include "x_string.hpp"
#include "ShaderContract.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

using GraphicsPipelineKey = ::render_pipeline_key;

//-------------------------------------------------------------------------

enum GraphicsPipelineCacheMode
{
    GRAPHICS_PIPELINE_CACHE_VALIDATION,
    GRAPHICS_PIPELINE_CACHE_SHIPPING,
};

//=========================================================================
//  CLASSES
//=========================================================================

class GraphicsPipelineCache
{
public:

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    GraphicsPipelineCache( void );
    ~GraphicsPipelineCache( void );

    void Reset( void );
    void SetMode( GraphicsPipelineCacheMode Mode );

    //-------------------------------------------------------------------------
    //  PIPELINE PREWARM
    //-------------------------------------------------------------------------

    render_pipeline* Prewarm( const render_pipeline_desc& Desc );
    render_pipeline* QueuePrewarm( const render_pipeline_desc& Desc );

    static void  BeginPrewarm( void );
    static xbool UpdatePrewarm( void );
    static xbool IsPrewarmComplete( void );
    static xbool HasPrewarmFailed( void );
    static void  WaitForPrewarm( void );

    //-------------------------------------------------------------------------
    //  PIPELINE LOOKUP
    //-------------------------------------------------------------------------

    render_pipeline* GetOrCreate( const render_pipeline_desc& Desc );
    render_pipeline* FindPrepared( const ShaderProgramKey&     Program,
                                   u64                         State,
                                   xbool                       IsPrewarm = FALSE,
                                   const GraphicsPipelineKey** ppKey     = NULL );
    void             RememberPrepared( const ShaderProgramKey& Program, u64 State, render_pipeline* pPipeline );

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct Entry
    {
        Entry( void );

        //-------------------------------------------------------------------------

        u64                               m_Key;
        render_pipeline_desc              m_Desc;
        GraphicsPipelineKey               m_NormalizedKey;
        xstring                           m_DebugName;
        shader_backend*                   m_pVertexShader;
        shader_backend*                   m_pPixelShader;
        xarray<shader_vertex_buffer_desc> m_VertexBuffers;
        xarray<shader_vertex_element>     m_InputElements;
        render_pipeline                   m_Pipeline;
        render_pipeline_create_request    m_Request;
        xbool                             m_isPrewarmed;
        xbool                             m_isUsed;
        xbool                             m_isQueued;
        GraphicsPipelineCache*            m_pOwner;
        Entry*                            m_pNextQueued;
    };

    //-------------------------------------------------------------------------

    struct PreparedEntry
    {
        ShaderProgramKey Program;
        u64              State;
        Entry*           pEntry;
        s32              Next;
    };

    //-------------------------------------------------------------------------
    //  HELPER FUNCTIONS
    //-------------------------------------------------------------------------

    Entry*           FindEntry( const GraphicsPipelineKey& Key, const render_pipeline_desc& Desc ) const;
    render_pipeline* FindOrCreate( const GraphicsPipelineKey&  Key,
                                   const render_pipeline_desc& Desc,
                                   xbool                       isPrewarm,
                                   xbool                       isQueued = FALSE );
    void             EnsureHashCapacity( s32 RequiredCount );
    void             EnsurePreparedHashCapacity( s32 RequiredCount );
    void             RebuildHashIndex( void );
    void             InsertHashEntry( s32 EntryIndex );
    static u32       HashKey( u64 Key );

    //-------------------------------------------------------------------------
    //  STATIC DATA
    //-------------------------------------------------------------------------

    static Entry* s_pQueued;
    static Entry* s_pQueuedTail;
    static Entry* s_pActive;
    static xbool  s_PrewarmFailed;
    static u32    s_QueuedCount;
    static xtick  s_PrewarmStart;
    static xbool  s_ReportPrewarm;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<PreparedEntry> m_Prepared;
    xarray<s32>           m_PreparedBuckets;

    const char*               m_pDebugName;
    xstring                   m_CacheDebugName;
    xarray<Entry*>            m_Entries;
    xarray<s32>               m_HashSlots;
    u32                       m_nPrewarmed;
    u32                       m_nRuntimeHits;
    u32                       m_nRuntimeMisses;
    xarray<u64>               m_ReportedValidationMisses;
    GraphicsPipelineCacheMode m_Mode;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

u64 GraphicsPipelineStateKey( rtarget_format       Color,
                              rtarget_format       DepthFormat,
                              u32                  Samples,
                              rstate_blend_preset  Blend,
                              rstate_depth_preset  Depth,
                              rstate_raster_preset Raster,
                              shader_topology      Topology = SHADER_TOPOLOGY_TRIANGLE_LIST );

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
u64 GraphicsPipelineStateKey( rtarget_format       Color,
                              rtarget_format       DepthFormat,
                              u32                  Samples,
                              rstate_blend_preset  Blend,
                              rstate_depth_preset  Depth,
                              rstate_raster_preset Raster,
                              shader_topology      Topology )
{
    return static_cast<u64>( Color ) | ( static_cast<u64>( DepthFormat ) << 8 ) |
           ( static_cast<u64>( Samples ) << 16 ) | ( static_cast<u64>( Blend ) << 24 ) |
           ( static_cast<u64>( Depth ) << 32 ) | ( static_cast<u64>( Raster ) << 40 ) |
           ( static_cast<u64>( Topology ) << 48 );
}

//=========================================================================

inline
void GraphicsPipelineCache::SetMode( GraphicsPipelineCacheMode Mode )
{
    m_Mode = Mode;
}

} // namespace render

#endif // GRAPHICS_PIPELINE_CACHE_HPP
