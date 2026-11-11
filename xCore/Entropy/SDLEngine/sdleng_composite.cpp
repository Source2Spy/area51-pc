//==============================================================================
//
//  sdleng_composite.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

#include "e_Composite.hpp"
#include "e_RenderPipelineKey.hpp"
#include "a51_shader_catalog.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    COMPOSITE_PIPELINE_BUCKETS = 256
};

static_assert( sizeof( composite_params ) == 32, "CompositeParams layout must match HLSL" );

//==============================================================================
//  TYPES
//==============================================================================

struct composite_pipeline_key
{
    const shader*         pPixelShader;
    const shader_backend* pPixelBackend;
    composite_blend_mode  BlendMode;
    rtarget_format        ColorFormat;
    rtarget_format        DepthFormat;
    u32                   SampleCount;

    composite_pipeline_key( void ) :
        pPixelShader ( NULL ),
        pPixelBackend( NULL ),
        BlendMode    ( COMPOSITE_BLEND_ALPHA ),
        ColorFormat  ( RTARGET_FORMAT_COUNT ),
        DepthFormat  ( RTARGET_FORMAT_COUNT ),
        SampleCount  ( 1 )
    {
    }
};

//------------------------------------------------------------------------------

struct composite_pipeline_entry
{
    composite_pipeline_key         Key;
    render_pipeline_key            NormalizedKey;
    u64                            Hash = 0;
    render_pipeline                Pipeline;
    render_pipeline_create_request Request;
    xbool                          bPending  = FALSE;
    composite_pipeline_entry*      pHashNext = NULL;

    composite_pipeline_entry( void ) :
        Key     (),
        Pipeline()
    {
    }
};

//------------------------------------------------------------------------------

struct composite_locals
{
    xbool                             bInitialized;
    shader                            VertexShader;
    shader                            PixelShader;
    shader                            ClampPixelShader;
    rstate_sampler                    Samplers[RSTATE_SAMPLER_PRESET_COUNT];
    xarray<composite_pipeline_entry*> Pipelines;
    composite_pipeline_entry*         Buckets[COMPOSITE_PIPELINE_BUCKETS] = {};

    composite_locals( void ) :
        bInitialized    ( FALSE ),
        VertexShader    (),
        PixelShader     (),
        ClampPixelShader(),
        Pipelines       ()
    {
    }
};

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static composite_locals s;

//==============================================================================
//  HELPERS
//==============================================================================

static
composite_blend_mode composite_NormalizeBlendMode( composite_blend_mode BlendMode )
{
    const s32 Mode = (s32)BlendMode;

    if( ( Mode < 0 ) || ( Mode >= COMPOSITE_BLEND_COUNT ) )
    {
        return COMPOSITE_BLEND_ALPHA;
    }

    return BlendMode;
}

//==============================================================================

static
rstate_blend_preset composite_GetBlendPreset( composite_blend_mode BlendMode )
{
    switch( BlendMode )
    {
        case COMPOSITE_BLEND_ALPHA:              return RSTATE_BLEND_PRESET_ALPHA;         break;
        case COMPOSITE_BLEND_ADDITIVE:           return RSTATE_BLEND_PRESET_ADD;           break;
        case COMPOSITE_BLEND_MULTIPLY:           return RSTATE_BLEND_PRESET_MULTIPLY;      break;
        case COMPOSITE_BLEND_OVERLAY:            return RSTATE_BLEND_PRESET_PREMULT_ALPHA; break;
        case COMPOSITE_BLEND_COPY:               return RSTATE_BLEND_PRESET_NONE;          break;
        case COMPOSITE_BLEND_ALPHA_RGB:          return RSTATE_BLEND_PRESET_ALPHA_RGB;     break;
        case COMPOSITE_BLEND_COPY_RGB:           return RSTATE_BLEND_PRESET_NONE_RGB;      break;
        case COMPOSITE_BLEND_ADDITIVE_RGB:       return RSTATE_BLEND_PRESET_ADD_RGB;       break;
        case COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB: return RSTATE_BLEND_PRESET_MAX_RGB;       break;
        default:                                 return RSTATE_BLEND_PRESET_ALPHA;         break;
    }
}

//==============================================================================

static
const shader* composite_GetPixelShader( composite_blend_mode BlendMode, const shader* pCustomShader )
{
    if( pCustomShader )
    {
        return pCustomShader;
    }

    return BlendMode == COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB ? &s.ClampPixelShader : &s.PixelShader;
}

//==============================================================================

static
void composite_DestroyPipelines( void )
{
    for( s32 i = 0; i < s.Pipelines.GetCount(); i++ )
    {
        composite_pipeline_entry* pEntry = s.Pipelines[i];

        if( pEntry )
        {
            if( pEntry->bPending )
            {
                render_ReleasePipelineCreate( pEntry->Request );
            }

            render_DestroyPipeline( pEntry->Pipeline );
            delete pEntry;
        }
    }

    s.Pipelines.Clear();
    x_memset( s.Buckets, 0, sizeof( s.Buckets ) );
}

//==============================================================================

void composite_InvalidateShader( const shader_backend* pShaderBackend )
{
    if( !pShaderBackend )
    {
        return;
    }

    for( s32 i = s.Pipelines.GetCount() - 1; i >= 0; --i )
    {
        composite_pipeline_entry* pEntry = s.Pipelines[i];

        if( !pEntry ||
            ( pEntry->Key.pPixelBackend != pShaderBackend &&
              s.VertexShader.pBackend != pShaderBackend ) )
        {
            continue;
        }

        composite_pipeline_entry** ppEntry = &s.Buckets[pEntry->Hash % COMPOSITE_PIPELINE_BUCKETS];

        while( *ppEntry != pEntry )
        {
            ppEntry = &( *ppEntry )->pHashNext;
        }

        *ppEntry = pEntry->pHashNext;
        s.Pipelines.Delete( i );

        if( pEntry->bPending )
        {
            render_ReleasePipelineCreate( pEntry->Request );
        }

        render_DestroyPipeline( pEntry->Pipeline );
        delete pEntry;
    }
}

//==============================================================================

static
void composite_ReleaseResources( void )
{
    composite_DestroyPipelines();

    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        rstate_DestroySampler( s.Samplers[i] );
    }

    shader_Destroy( s.PixelShader );
    shader_Destroy( s.ClampPixelShader );
    shader_Destroy( s.VertexShader );
}

//==============================================================================

static
xbool composite_CreateResources( void )
{
    if( !shader_LoadComboStage( s.VertexShader, A51FixedPrograms[A51FixedProgram_composite].PackageFile, SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_composite].Key, SHADER_STAGE_VERTEX ) )
    {
        x_DebugMsg( "CompositeMgr: failed to load composite vertex shader\n" );
        return FALSE;
    }

    if( !shader_LoadComboStage( s.PixelShader, A51FixedPrograms[A51FixedProgram_composite].PackageFile, SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_composite].Key, SHADER_STAGE_PIXEL ) )
    {
        x_DebugMsg( "CompositeMgr: failed to load composite pixel shader\n" );
        return FALSE;
    }

    if( !shader_LoadComboStage( s.ClampPixelShader,
                                A51FixedPrograms[A51FixedProgram_composite_clamp_negative].PackageFile,
                                SHADER_COMBO_FIXED,
                                A51FixedPrograms[A51FixedProgram_composite_clamp_negative].Key,
                                SHADER_STAGE_PIXEL ) )
    {
        x_DebugMsg( "CompositeMgr: failed to load negative color clamp pixel shader\n" );
        return FALSE;
    }

    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        if( !rstate_CreateSampler( s.Samplers[i], (rstate_sampler_preset)i, "CompositeSampler" ) )
        {
            x_DebugMsg( "CompositeMgr: failed to create sampler preset %d\n", i );
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool composite_SetTargetViewport( const rtarget& Target )
{
    if( ( Target.Desc.Width == 0 ) || ( Target.Desc.Height == 0 ) )
    {
        return FALSE;
    }

    rdraw_viewport Viewport;
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    Viewport.Width    = (f32)Target.Desc.Width;
    Viewport.Height   = (f32)Target.Desc.Height;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    return rdraw_SetViewport( Viewport );
}

//==============================================================================

static
xbool composite_SetPipelineKey( composite_pipeline_key& Key,
                                composite_blend_mode    BlendMode,
                                const shader&           PixelShader,
                                rtarget_format          ColorFormat,
                                rtarget_format          DepthFormat,
                                u32                     SampleCount )
{
    if( !PixelShader || ( PixelShader.Stage != SHADER_STAGE_PIXEL ) || !PixelShader.pBackend )
    {
        return FALSE;
    }

    if( ( (u32)ColorFormat >= (u32)RTARGET_FORMAT_COUNT ) ||
        ( ColorFormat == RTARGET_FORMAT_DEPTH_STENCIL ) ||
        ( ColorFormat == RTARGET_FORMAT_DEPTH32F ) )
    {
        return FALSE;
    }

    if( ( DepthFormat != RTARGET_FORMAT_COUNT ) &&
        ( DepthFormat != RTARGET_FORMAT_DEPTH_STENCIL ) &&
        ( DepthFormat != RTARGET_FORMAT_DEPTH32F ) )
    {
        return FALSE;
    }

    Key.pPixelShader  = &PixelShader;
    Key.pPixelBackend = PixelShader.pBackend;
    Key.BlendMode     = BlendMode;
    Key.ColorFormat   = ColorFormat;
    Key.DepthFormat   = DepthFormat;
    Key.SampleCount   = SampleCount ? SampleCount : 1;

    return TRUE;
}

//==============================================================================

static
xbool composite_BuildPipelineKey( composite_pipeline_key& Key,
                                  composite_blend_mode    BlendMode,
                                  const shader&           PixelShader )
{
    if( rtarget_GetCurrentCount() != 1 )
    {
        x_DebugMsg( "CompositeMgr: composite pass requires exactly one color target\n" );
        return FALSE;
    }

    const rtarget* pColorTarget = rtarget_GetCurrentTarget( 0 );

    if( !pColorTarget )
    {
        return FALSE;
    }

    const rtarget* pDepthTarget = rtarget_GetCurrentDepth();

    return composite_SetPipelineKey( Key,
                                     BlendMode,
                                     PixelShader,
                                     pColorTarget->Desc.Format,
                                     pDepthTarget ? pDepthTarget->Desc.Format : RTARGET_FORMAT_COUNT,
                                     pColorTarget->Desc.SampleCount );
}

//==============================================================================

static
void composite_BuildPipelineDesc( render_pipeline_desc&         Desc,
                                  const composite_pipeline_key& Key )
{
    render_color_target_desc ColorTarget;
    ColorTarget.Format = Key.ColorFormat;
    ColorTarget.Blend  = rstate_GetBlendDesc( composite_GetBlendPreset( Key.BlendMode ) );

    Desc.Shader.pVertexShader = &s.VertexShader;
    Desc.Shader.pPixelShader  = Key.pPixelShader;
    Desc.Shader.Topology      = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Desc.Raster               = rstate_GetRasterDesc( RSTATE_RASTER_PRESET_SOLID_NO_CULL );
    Desc.Depth                = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE );
    Desc.ColorTargets[0]      = ColorTarget;
    Desc.ColorCount           = 1;
    Desc.DepthFormat          = Key.DepthFormat;
    Desc.SampleCount          = Key.SampleCount;
    Desc.pDebugName           = Key.BlendMode == COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB
                                    ? "ClampNegativeColor"
                                    : "CompositePipeline";
}

//==============================================================================

static
render_pipeline* composite_GetPipeline( const composite_pipeline_key& Key, xbool IsPrewarm )
{
    render_pipeline_desc Desc;
    composite_BuildPipelineDesc( Desc, Key );
    const render_pipeline_key NormalizedKey( Desc );
    const u64                 Hash = NormalizedKey.Hash();

    for( composite_pipeline_entry* pEntry = s.Buckets[Hash % COMPOSITE_PIPELINE_BUCKETS];
         pEntry;
         pEntry = pEntry->pHashNext )
    {
        if( pEntry->Hash == Hash && pEntry->NormalizedKey == NormalizedKey &&
            pEntry->Key.pPixelBackend == Key.pPixelBackend )
        {
            if( pEntry->bPending )
            {
#if defined( CONFIG_DEBUG )
                if( !IsPrewarm )
                {
                    ASSERTS( FALSE, "Composite pipeline prewarm is incomplete" );
                    return NULL;
                }
#endif
                if( !render_CreatePipeline( pEntry->Pipeline, Desc ) )
                {
                    return NULL;
                }

                render_ReleasePipelineCreate( pEntry->Request );
                pEntry->bPending = FALSE;
            }

            return &pEntry->Pipeline;
        }
    }

    if( !IsPrewarm )
    {
        x_DebugMsg( "Composite PSO miss: key=%016llx fields=", static_cast<unsigned long long>( Hash ) );

        for( u32 i = 0; i < NormalizedKey.Count; ++i )
        {
            x_DebugMsg( "%s%016llx", i ? "," : "", static_cast<unsigned long long>( NormalizedKey.Values[i] ) );
        }

        x_DebugMsg( "\n" );
#if defined( CONFIG_DEBUG )
        ASSERTS( FALSE, "Composite pipeline was not covered by prewarm" );
        return NULL;
#endif
    }

    composite_pipeline_entry* pEntry = new composite_pipeline_entry;

    if( !pEntry )
    {
        return NULL;
    }

    pEntry->Key           = Key;
    pEntry->Hash          = Hash;
    pEntry->NormalizedKey = NormalizedKey;

    if( !render_CreatePipeline( pEntry->Pipeline, Desc ) )
    {
        delete pEntry;
        return NULL;
    }

    pEntry->bPending                             = FALSE;
    pEntry->pHashNext                            = s.Buckets[Hash % COMPOSITE_PIPELINE_BUCKETS];
    s.Buckets[Hash % COMPOSITE_PIPELINE_BUCKETS] = pEntry;
    s.Pipelines.Append( pEntry );

    return &pEntry->Pipeline;
}

//==============================================================================

static
xbool composite_BindSource( const shader&          PixelShader,
                            u32                    SourceSemantic,
                            const shader_resource* pSourceResource,
                            rstate_sampler_preset  SamplerMode )
{
    if( SourceSemantic == 0xFFFFFFFFu )
    {
        return TRUE;
    }

    if( ( SamplerMode < 0 ) || ( SamplerMode >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        SamplerMode = RSTATE_SAMPLER_PRESET_POINT_CLAMP;
    }

    if( SourceSemantic >= A51ResourceCount )
    {
        return FALSE;
    }

    return shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL,
                                                       PixelShader.ResourceSlots[SourceSemantic],
                                                       pSourceResource,
                                                       &s.Samplers[SamplerMode] ) );
}

//==============================================================================

static
xbool composite_BindDefaultParams( const shader& PixelShader, composite_blend_mode BlendMode, f32 Alpha )
{
    composite_params Params;
    Params.BlendColor = vector4( 1.0f, 1.0f, 1.0f, Alpha );
    Params.BlendMode  = (s32)BlendMode;
    Params.Padding0   = 0.0f;
    Params.Padding1   = 0.0f;
    Params.Padding2   = 0.0f;

    return shader_PushUniformData( SHADER_STAGE_PIXEL,
                                   PixelShader.ResourceSlots[A51Resource_CompositeParams],
                                   &Params,
                                   sizeof( Params ) );
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

void composite_Init( void )
{
    if( s.bInitialized )
    {
        return;
    }

    if( !composite_CreateResources() )
    {
        composite_ReleaseResources();
        return;
    }

    s.Pipelines.SetCapacity( 64 );
    s.bInitialized = TRUE;
}

//==============================================================================

void composite_Kill( void )
{
    if( !s.bInitialized )
    {
        return;
    }

    composite_ReleaseResources();
    s.bInitialized = FALSE;
}

//==============================================================================
//  PIPELINE PREPARATION
//==============================================================================

xbool composite_GetPipelineDesc( render_pipeline_desc& Desc,
                                 composite_blend_mode  BlendMode,
                                 rtarget_format        ColorFormat,
                                 rtarget_format        DepthFormat,
                                 u32                   SampleCount,
                                 const shader*         pCustomShader )
{
    if( !s.bInitialized )
    {
        return FALSE;
    }

    BlendMode                           = composite_NormalizeBlendMode( BlendMode );
    const shader*          pPixelShader = composite_GetPixelShader( BlendMode, pCustomShader );
    composite_pipeline_key Key;

    if( !composite_SetPipelineKey( Key, BlendMode, *pPixelShader, ColorFormat, DepthFormat, SampleCount ) )
    {
        return FALSE;
    }

    Desc = render_pipeline_desc();
    composite_BuildPipelineDesc( Desc, Key );

    return TRUE;
}

//==============================================================================

render_pipeline* composite_QueuePipeline( const render_pipeline_desc& Desc )
{
    if( !s.bInitialized || Desc.Shader.pVertexShader != &s.VertexShader ||
        !Desc.Shader.pPixelShader || !Desc.Shader.pPixelShader->pBackend )
    {
        return NULL;
    }

    // The declared descriptor already identifies the composite shader and targets.
    const render_pipeline_key Key( Desc );
    const u64                 Hash = Key.Hash();

    for( composite_pipeline_entry* pEntry = s.Buckets[Hash % COMPOSITE_PIPELINE_BUCKETS];
         pEntry;
         pEntry = pEntry->pHashNext )
    {
        if( pEntry->Hash == Hash && pEntry->NormalizedKey == Key &&
            pEntry->Key.pPixelBackend == Desc.Shader.pPixelShader->pBackend )
        {
            return &pEntry->Pipeline;
        }
    }

    composite_pipeline_entry* pEntry = new composite_pipeline_entry;
    pEntry->Key.pPixelShader         = Desc.Shader.pPixelShader;
    pEntry->Key.pPixelBackend        = Desc.Shader.pPixelShader->pBackend;
    pEntry->Hash                     = Hash;
    pEntry->NormalizedKey            = Key;

    if( !render_BeginPipelineCreate( pEntry->Request, Desc ) )
    {
        delete pEntry;
        return NULL;
    }

    pEntry->bPending                             = TRUE;
    pEntry->pHashNext                            = s.Buckets[Hash % COMPOSITE_PIPELINE_BUCKETS];
    s.Buckets[Hash % COMPOSITE_PIPELINE_BUCKETS] = pEntry;
    s.Pipelines.Append( pEntry );

    return &pEntry->Pipeline;
}

//==============================================================================

xbool composite_FinishPipeline( render_pipeline& Pipeline )
{
    if( Pipeline )
    {
        return TRUE;
    }

    for( s32 i = 0; i < s.Pipelines.GetCount(); ++i )
    {
        composite_pipeline_entry& Entry = *s.Pipelines[i];

        if( &Entry.Pipeline != &Pipeline )
        {
            continue;
        }

        if( !Entry.bPending || !render_FinishPipelineCreate( Entry.Request, Entry.Pipeline ) )
        {
            return FALSE;
        }

        render_ReleasePipelineCreate( Entry.Request );
        Entry.bPending = FALSE;
        return TRUE;
    }

    return FALSE;
}

//==============================================================================

xbool composite_FinishPrewarm( void )
{
    for( s32 i = 0; i < s.Pipelines.GetCount(); ++i )
    {
        composite_pipeline_entry& Entry = *s.Pipelines[i];

        if( !Entry.bPending )
        {
            continue;
        }

        if( !render_FinishPipelineCreate( Entry.Request, Entry.Pipeline ) )
        {
            return FALSE;
        }

        render_ReleasePipelineCreate( Entry.Request );
        Entry.bPending = FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool composite_PrewarmPipeline( composite_blend_mode BlendMode,
                                 rtarget_format       ColorFormat,
                                 rtarget_format       DepthFormat,
                                 u32                  SampleCount,
                                 const shader*        pCustomShader )
{
    if( !s.bInitialized )
    {
        return FALSE;
    }

    BlendMode                  = composite_NormalizeBlendMode( BlendMode );
    const shader* pPixelShader = composite_GetPixelShader( BlendMode, pCustomShader );

    composite_pipeline_key Key;

    if( !composite_SetPipelineKey( Key,
                                   BlendMode,
                                   *pPixelShader,
                                   ColorFormat,
                                   DepthFormat,
                                   SampleCount ) )
    {
        return FALSE;
    }

    return composite_GetPipeline( Key, TRUE ) != NULL;
}

//==============================================================================

static
    //==============================================================================
    //  COMPOSITE DRAWING
    //==============================================================================

    xbool composite_Submit( const rtarget*                pSource,
                            composite_blend_mode          BlendMode,
                            f32                           Alpha,
                            const shader*                 pCustomShader,
                            rstate_sampler_preset         SamplerMode,
                            u32                           SourceSemantic,
                            const composite_extra_source* pExtraSources,
                            u32                           nExtraSources )
{
    if( !s.bInitialized )
    {
        return FALSE;
    }

    if( nExtraSources && !pExtraSources )
    {
        return FALSE;
    }

    if( !sdleng_InRenderPass() )
    {
        return FALSE;
    }

    BlendMode = composite_NormalizeBlendMode( BlendMode );

    const shader_resource* pSourceResource = pSource ? rtarget_GetShaderResource( *pSource ) : NULL;

    if( pSource && !pSourceResource )
    {
        return FALSE;
    }

    const rtarget* pCurrentTarget = rtarget_GetCurrentTarget( 0 );
    // In-pass clamping must preserve the scene viewport for subsequent draws.
    if( !pCurrentTarget ||
        ( BlendMode != COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB && !composite_SetTargetViewport( *pCurrentTarget ) ) )
    {
        return FALSE;
    }

    const shader* pPixelShader = composite_GetPixelShader( BlendMode, pCustomShader );

    composite_pipeline_key Key;

    if( !composite_BuildPipelineKey( Key, BlendMode, *pPixelShader ) )
    {
        return FALSE;
    }

    render_pipeline* pPipeline = composite_GetPipeline( Key, FALSE );

    if( !pPipeline )
    {
        return FALSE;
    }

    if( !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }

    const u32 ResolvedSource = pCustomShader ? SourceSemantic : A51Resource_CompositeSource;

    if( pSource && !composite_BindSource( *pPixelShader, ResolvedSource, pSourceResource, SamplerMode ) )
    {
        return FALSE;
    }

    for( u32 i = 0; i < nExtraSources; ++i )
    {
        composite_extra_source const& Extra          = pExtraSources[i];
        shader_resource const*        pExtraResource = Extra.pResource;

        if( !pExtraResource || !composite_BindSource( *pPixelShader, Extra.ResourceSemantic, pExtraResource, Extra.SamplerMode ) )
        {
            return FALSE;
        }
    }

    if( !pCustomShader && BlendMode != COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB &&
        !composite_BindDefaultParams( *pPixelShader, BlendMode, Alpha ) )
    {
        return FALSE;
    }

    return rdraw_Draw( 3 );
}

//==============================================================================

xbool composite_Draw( const shader& PixelShader, composite_blend_mode BlendMode )
{
    return composite_Submit( NULL, BlendMode, 1.0f, &PixelShader, RSTATE_SAMPLER_PRESET_POINT_CLAMP, 0xFFFFFFFFu, NULL, 0 );
}

//==============================================================================

xbool composite_ClampNegative( void )
{
    return composite_Submit( NULL, COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB, 1.0f, NULL, RSTATE_SAMPLER_PRESET_POINT_CLAMP, 0xFFFFFFFFu, NULL, 0 );
}

//==============================================================================

xbool composite_Blit( const rtarget& Source, composite_blend_mode BlendMode, f32 Alpha, const shader* pCustomShader, rstate_sampler_preset SamplerMode, u32 SourceSemantic, const composite_extra_source* pExtraSources, u32 nExtraSources )
{
    return composite_Submit( &Source, BlendMode, Alpha, pCustomShader, SamplerMode, SourceSemantic, pExtraSources, nExtraSources );
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
