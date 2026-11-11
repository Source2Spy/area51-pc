//==============================================================================
//
//  sdleng_shader.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"
#if defined( A51_GAME_SHADER_CONTRACT )
#include "a51_shader_catalog.hpp"
#endif
#include "e_ECS.hpp"
#include "x_hash.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

#ifndef X_PLUS_HPP
#include "x_plus.hpp"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    SDLENG_NATIVE_STAGE_BUCKETS = 256
};

//==============================================================================
//  TYPES
//==============================================================================

struct native_stage_entry
{
    xarray<byte>   Key;
    shader_native* pNative = NULL;
};

//------------------------------------------------------------------------------

struct sdlshader_sampler_cache_entry
{
    SDL_GPUTexture* pTexture;
    SDL_GPUSampler* pSampler;
    u64             Serial;

    sdlshader_sampler_cache_entry( void ) :
        pTexture( NULL ),
        pSampler( NULL ),
        Serial  ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct sdlshader_texture_cache_entry
{
    SDL_GPUTexture* pTexture;
    u64             Serial;

    sdlshader_texture_cache_entry( void ) :
        pTexture( NULL ),
        Serial  ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct sdlshader_buffer_cache_entry
{
    SDL_GPUBuffer* pBuffer;
    u64            Serial;

    sdlshader_buffer_cache_entry( void ) :
        pBuffer( NULL ),
        Serial ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct sdlshader_compute_state
{
    const shader_backend* pShader;
    SDL_GPUTexture*       pTextures[SDLENG_MAX_STORAGE_TEXTURE_SLOTS];
    SDL_GPUBuffer*        pBuffers[SDLENG_MAX_STORAGE_BUFFER_SLOTS];
    u32                   SamplerMask;
    u32                   TextureMask;
    u32                   BufferMask;
    u32                   UniformMask;
    xbool                 Dispatched;
};

//------------------------------------------------------------------------------

struct shader_program_load
{
    ecs_request_ptr     Request;
    shader_program_desc Desc;
    xstring             File;
    xstring             DebugName;
};

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static xarray<native_stage_entry> s_NativeStages[SDLENG_NATIVE_STAGE_BUCKETS];
static SDL_SpinLock               s_NativeStageLock = 0;

//==============================================================================

static shader_backend* s_pShaderList      = NULL;
static u32             s_ShaderCount      = 0;
static u64             s_NextShaderSerial = 1;

//==============================================================================

static u64                           s_ShaderCatalogId = 0;
static ecs_package_store             s_ShaderPackages;
static u32                           s_ShaderPendingCount = 0;
static sdlshader_sampler_cache_entry s_SamplerBindings[2][SDLENG_MAX_SAMPLER_SLOTS];
static sdlshader_texture_cache_entry s_StorageTextureBindings[2][SDLENG_MAX_STORAGE_TEXTURE_SLOTS];
static sdlshader_buffer_cache_entry  s_StorageBufferBindings[2][SDLENG_MAX_STORAGE_BUFFER_SLOTS];
static SDL_GPUTextureSamplerBinding  s_SamplerBatchBindings[2][SDLENG_MAX_SAMPLER_SLOTS];
static SDL_GPUTexture*               s_StorageTextureBatchBindings[2][SDLENG_MAX_STORAGE_TEXTURE_SLOTS];
static SDL_GPUBuffer*                s_StorageBufferBatchBindings[2][SDLENG_MAX_STORAGE_BUFFER_SLOTS];
static xbool                         s_SamplerBatchUsed[2][SDLENG_MAX_SAMPLER_SLOTS];
static xbool                         s_StorageTextureBatchUsed[2][SDLENG_MAX_STORAGE_TEXTURE_SLOTS];
static xbool                         s_StorageBufferBatchUsed[2][SDLENG_MAX_STORAGE_BUFFER_SLOTS];
static u64                           s_ShaderBindingSerial = 1;
static xarray<byte>                  s_UniformData[3][SDLENG_MAX_UNIFORM_BUFFER_SLOTS];
#if !defined(X_RETAIL)
static const render_pipeline_backend* s_pDebugPipeline                  = NULL;
static u32                            s_DebugVertexSamplerMask          = 0;
static u32                            s_DebugFragmentSamplerMask        = 0;
static u32                            s_DebugVertexStorageTextureMask   = 0;
static u32                            s_DebugFragmentStorageTextureMask = 0;
static u32                            s_DebugVertexStorageBufferMask    = 0;
static u32                            s_DebugFragmentStorageBufferMask  = 0;
static u32                            s_DebugBindingRevision            = 1;
static u32                            s_DebugValidatedRevision          = 0;
static const render_pipeline_backend* s_pValidatedPipeline              = NULL;
static xbool                          s_bDebugValidationResult          = FALSE;
#endif

static shader_resource_access_validator s_ResourceAccessValidator = NULL;
static void*                            s_pResourceAccessContext  = NULL;

static sdlshader_compute_state s_ComputeState = {};

//==============================================================================
//  NATIVE STAGE KEYS AND IDENTITIES
//==============================================================================

static
void NativeStageWord( xarray<byte>& Key, u64 Value )
{
    for( u32 i = 0; i < 8; ++i )
    {
        Key.Append( (byte)( Value >> ( i * 8 ) ) );
    }
}

//==============================================================================

static
void NativeStageText( xarray<byte>& Key, const char* pText )
{
    const s32 Length = x_strlen( pText );
    NativeStageWord( Key, Length );

    for( s32 i = 0; i < Length; ++i )
    {
        Key.Append( (byte)pText[i] );
    }
}

//==============================================================================

static
xbool NativeStageKey( const shader_desc& Desc, const x_content_id* pContent, xarray<byte>& Key )
{
    // Reserve the complete serialized key before appending its variable-length data.
    u64 Size = 32 + 14 * 8 + x_strlen( Desc.pEntryPoint ? Desc.pEntryPoint : "main" );

    for( u32 i = 0; i < Desc.BindingCount; ++i )
    {
        Size += 4 * 8 + x_strlen( Desc.pBindings[i].pName );
    }

    if( Desc.pInterface )
    {
        const ecs_package_layout& Layout = *Desc.pInterface;
        Size += 4 * 8 + Layout.Bindings.GetCount() * 8;

        for( s32 i = 0; i < Layout.Members.GetCount(); ++i )
        {
            Size += 6 * 8 + x_strlen( Layout.Members[i].Buffer ) + x_strlen( Layout.Members[i].Name );
        }

        for( s32 Side = 0; Side < 2; ++Side )
        {
            const xarray<ecs_package_variable>& Variables = Side ? Layout.Outputs : Layout.Inputs;
            Size += 8;

            for( s32 i = 0; i < Variables.GetCount(); ++i )
            {
                Size += 4 * 8 + x_strlen( Variables[i].Semantic );
            }
        }
    }

    if( Size > S32_MAX )
    {
        return FALSE;
    }

    Key.SetCapacity( (s32)Size );

    // Start with the shader content, then append the entry point and resource layout.
    const x_content_id Digest = pContent ? *pContent : x_HashContent( (const byte*)Desc.Blob.pData, Desc.Blob.Size );

    for( s32 i = 0; i < 32; ++i )
    {
        Key.Append( Digest[i] );
    }

    NativeStageWord( Key, Desc.Stage );
    NativeStageWord( Key, Desc.Format );
    NativeStageText( Key, Desc.pEntryPoint ? Desc.pEntryPoint : "main" );
    NativeStageWord( Key, Desc.Resources.SamplerCount );
    NativeStageWord( Key, Desc.Resources.UniformBufferCount );
    NativeStageWord( Key, Desc.Resources.StorageTextureCount );
    NativeStageWord( Key, Desc.Resources.StorageBufferCount );
    NativeStageWord( Key, Desc.Compute.ReadWriteTextureCount );
    NativeStageWord( Key, Desc.Compute.ReadWriteBufferCount );
    NativeStageWord( Key, Desc.Compute.ThreadCountX );
    NativeStageWord( Key, Desc.Compute.ThreadCountY );
    NativeStageWord( Key, Desc.Compute.ThreadCountZ );
    NativeStageWord( Key, Desc.BindingCount );

    for( u32 i = 0; i < Desc.BindingCount; ++i )
    {
        const shader_binding_desc& Binding = Desc.pBindings[i];
        NativeStageWord( Key, Binding.Kind );
        NativeStageWord( Key, Binding.Slot );
        NativeStageWord( Key, Binding.Count );
        NativeStageText( Key, Binding.pName );
    }

    NativeStageWord( Key, Desc.pInterface != NULL );

    // Include the reflected interface so incompatible layouts cannot share a stage.
    if( Desc.pInterface )
    {
        const ecs_package_layout& Layout = *Desc.pInterface;
        NativeStageWord( Key, Layout.InputInterface );
        NativeStageWord( Key, Layout.OutputInterface );
        NativeStageWord( Key, Layout.Bindings.GetCount() );

        for( s32 i = 0; i < Layout.Bindings.GetCount(); ++i )
        {
            NativeStageWord( Key, Layout.Bindings[i].ByteSize );
        }

        NativeStageWord( Key, Layout.Members.GetCount() );

        for( s32 i = 0; i < Layout.Members.GetCount(); ++i )
        {
            const ecs_package_member& Member = Layout.Members[i];
            NativeStageText( Key, Member.Buffer );
            NativeStageText( Key, Member.Name );
            NativeStageWord( Key, Member.Offset );
            NativeStageWord( Key, Member.Size );
            NativeStageWord( Key, Member.ArrayStride );
            NativeStageWord( Key, Member.MatrixStride );
        }

        for( s32 Side = 0; Side < 2; ++Side )
        {
            const xarray<ecs_package_variable>& Variables = Side ? Layout.Outputs : Layout.Inputs;
            NativeStageWord( Key, Variables.GetCount() );

            for( s32 i = 0; i < Variables.GetCount(); ++i )
            {
                NativeStageText( Key, Variables[i].Semantic );
                NativeStageWord( Key, Variables[i].Index );
                NativeStageWord( Key, Variables[i].Components );
                NativeStageWord( Key, Variables[i].ComponentType );
            }
        }
    }

    ASSERT( (u64)Key.GetCount() == Size );

    return TRUE;
}

//==============================================================================

static
u64 sdlshader_AllocateSerial( void )
{
    const u64 FirstCandidate = s_NextShaderSerial;
    u64       Candidate      = FirstCandidate;

    do
    {
        xbool bUsed = FALSE;

        for( shader_backend* pBackend = s_pShaderList; pBackend; pBackend = pBackend->pNext )
        {
            if( pBackend->Serial == Candidate )
            {
                bUsed = TRUE;
                break;
            }
        }

        ++s_NextShaderSerial;

        if( s_NextShaderSerial == 0 )
        {
            s_NextShaderSerial = 1;
        }

        if( !bUsed )
        {
            return Candidate;
        }

        Candidate = s_NextShaderSerial;
    } while( Candidate != FirstCandidate );

    return 0;
}

//==============================================================================
//  NATIVE SHADER LIFETIME
//==============================================================================

void shader_native::ReleaseReference( void ) const
{
    // Remove the registry entry before a concurrent lookup can observe zero owners.
    SDL_LockSpinlock( &s_NativeStageLock );
    const s32 Count = RemoveReference();

    if( Count == 0 )
    {
        xarray<native_stage_entry>& Bucket = s_NativeStages[RegistryBucket];

        for( s32 i = 0; i < Bucket.GetCount(); ++i )
        {
            if( Bucket[i].pNative != this )
            {
                continue;
            }

            const s32 Last = Bucket.GetCount() - 1;

            if( i != Last )
            {
                Bucket[i] = Bucket[Last];
            }

            Bucket[Last].Key.Clear();
            Bucket[Last].pNative = NULL;
            Bucket.SetCount( Last );
            break;
        }
    }

    SDL_UnlockSpinlock( &s_NativeStageLock );
    // Driver work must not run while the registry lock is held.
    if( Count == 0 )
    {
        delete this;
    }
}

//==============================================================================

shader_native::~shader_native( void )
{
    if( pShader && pDevice )
    {
        SDL_ReleaseGPUShader( pDevice, pShader );
    }

    if( pComputePipeline && pDevice )
    {
        SDL_ReleaseGPUComputePipeline( pDevice, pComputePipeline );
    }
}

//==============================================================================
//  GRAPHICS BINDING VALIDATION
//==============================================================================

#if !defined(X_RETAIL)

//==============================================================================

static
void sdlshader_InvalidateGraphicsBindingValidation( void )
{
    ++s_DebugBindingRevision;

    if( s_DebugBindingRevision == 0 )
    {
        s_DebugBindingRevision = 1;
    }

    s_pValidatedPipeline = NULL;
}

//==============================================================================

static
xbool sdlshader_CacheGraphicsBindingValidation( xbool Result )
{
    s_pValidatedPipeline     = s_pDebugPipeline;
    s_DebugValidatedRevision = s_DebugBindingRevision;
    s_bDebugValidationResult = Result;

    return Result;
}

//==============================================================================

void sdleng_ResetGraphicsBindingDebug( void )
{
    s_pDebugPipeline                  = NULL;
    s_DebugVertexSamplerMask          = 0;
    s_DebugFragmentSamplerMask        = 0;
    s_DebugVertexStorageTextureMask   = 0;
    s_DebugFragmentStorageTextureMask = 0;
    s_DebugVertexStorageBufferMask    = 0;
    s_DebugFragmentStorageBufferMask  = 0;
    sdlshader_InvalidateGraphicsBindingValidation();
}

//==============================================================================

void sdleng_SetGraphicsPipelineDebug( const render_pipeline_backend* pPipeline )
{
    if( s_pDebugPipeline == pPipeline )
    {
        return;
    }

    s_pDebugPipeline                  = pPipeline;
    s_DebugVertexSamplerMask          = 0;
    s_DebugFragmentSamplerMask        = 0;
    s_DebugVertexStorageTextureMask   = 0;
    s_DebugFragmentStorageTextureMask = 0;
    s_DebugVertexStorageBufferMask    = 0;
    s_DebugFragmentStorageBufferMask  = 0;
    sdlshader_InvalidateGraphicsBindingValidation();
}

//==============================================================================

void sdleng_ClearGraphicsPipelineDebug( const render_pipeline_backend* pPipeline )
{
    if( s_pDebugPipeline == pPipeline )
    {
        s_pDebugPipeline = NULL;
        sdlshader_InvalidateGraphicsBindingValidation();
    }
}

//==============================================================================

void sdleng_RecordSamplerBindingDebug( shader_stage Stage, u32 Slot )
{
    if( Slot >= 32 )
    {
        return;
    }

    if( Stage == SHADER_STAGE_VERTEX )
    {
        const u32 Mask = 1u << Slot;

        if( !( s_DebugVertexSamplerMask & Mask ) )
        {
            s_DebugVertexSamplerMask |= Mask;
            sdlshader_InvalidateGraphicsBindingValidation();
        }
    }
    else if( Stage == SHADER_STAGE_PIXEL )
    {
        const u32 Mask = 1u << Slot;

        if( !( s_DebugFragmentSamplerMask & Mask ) )
        {
            s_DebugFragmentSamplerMask |= Mask;
            sdlshader_InvalidateGraphicsBindingValidation();
        }
    }
}

//==============================================================================

static
void sdlshader_RecordBindingMask( shader_stage Stage, u32 Slot, u32& Mask )
{
    if( ( Slot >= 32 ) || ( Stage == SHADER_STAGE_COMPUTE ) )
    {
        return;
    }

    const u32 Bit = 1u << Slot;

    if( !( Mask & Bit ) )
    {
        Mask |= Bit;
        sdlshader_InvalidateGraphicsBindingValidation();
    }
}

//==============================================================================

void sdleng_RecordStorageTextureBindingDebug( shader_stage Stage, u32 Slot )
{
    if( Stage == SHADER_STAGE_VERTEX )
    {
        sdlshader_RecordBindingMask( Stage, Slot, s_DebugVertexStorageTextureMask );
    }
    else if( Stage == SHADER_STAGE_PIXEL )
    {
        sdlshader_RecordBindingMask( Stage, Slot, s_DebugFragmentStorageTextureMask );
    }
}

//==============================================================================

void sdleng_RecordStorageBufferBindingDebug( shader_stage Stage, u32 Slot )
{
    if( Stage == SHADER_STAGE_VERTEX )
    {
        sdlshader_RecordBindingMask( Stage, Slot, s_DebugVertexStorageBufferMask );
    }
    else if( Stage == SHADER_STAGE_PIXEL )
    {
        sdlshader_RecordBindingMask( Stage, Slot, s_DebugFragmentStorageBufferMask );
    }
}

//==============================================================================

static
xbool sdlshader_ValidateBindingMask( const char* pPipelineName,
                                     const char* pStageName,
                                     const char* pBindingName,
                                     u32         Count,
                                     u32         BoundMask )
{
    for( u32 Slot = 0; Slot < Count; ++Slot )
    {
        if( BoundMask & ( 1u << Slot ) )
        {
            continue;
        }

        x_DebugMsg( "SDLEngine: pipeline '%s' missing %s %s slot %u\n",
                    pPipelineName,
                    pStageName,
                    pBindingName,
                    Slot );

        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool sdleng_ValidateGraphicsBindings( void )
{
    if( ( s_pValidatedPipeline == s_pDebugPipeline ) &&
        ( s_DebugValidatedRevision == s_DebugBindingRevision ) )
    {
        return s_bDebugValidationResult;
    }

    if( !s_pDebugPipeline )
    {
        x_DebugMsg( "SDLEngine: draw has no active graphics pipeline\n" );
        return sdlshader_CacheGraphicsBindingValidation( FALSE );
    }

    const shader_resource_counts* pVertexResources = &s_pDebugPipeline->VertexResources;
    const shader_resource_counts* pPixelResources  = &s_pDebugPipeline->PixelResources;
    const char*                   pPipelineName    = s_pDebugPipeline->pDebugName ? s_pDebugPipeline->pDebugName : "unnamed";

    if( pVertexResources &&
        ( !sdlshader_ValidateBindingMask( pPipelineName, "vertex", "sampler", pVertexResources->SamplerCount, s_DebugVertexSamplerMask ) ||
          !sdlshader_ValidateBindingMask( pPipelineName, "vertex", "storage texture", pVertexResources->StorageTextureCount, s_DebugVertexStorageTextureMask ) ||
          !sdlshader_ValidateBindingMask( pPipelineName, "vertex", "storage buffer", pVertexResources->StorageBufferCount, s_DebugVertexStorageBufferMask ) ) )
    {
        return sdlshader_CacheGraphicsBindingValidation( FALSE );
    }

    if( pPixelResources &&
        ( !sdlshader_ValidateBindingMask( pPipelineName, "fragment", "sampler", pPixelResources->SamplerCount, s_DebugFragmentSamplerMask ) ||
          !sdlshader_ValidateBindingMask( pPipelineName, "fragment", "storage texture", pPixelResources->StorageTextureCount, s_DebugFragmentStorageTextureMask ) ||
          !sdlshader_ValidateBindingMask( pPipelineName, "fragment", "storage buffer", pPixelResources->StorageBufferCount, s_DebugFragmentStorageBufferMask ) ) )
    {
        return sdlshader_CacheGraphicsBindingValidation( FALSE );
    }

    return sdlshader_CacheGraphicsBindingValidation( TRUE );
}

#endif // !defined(X_RETAIL)

//==============================================================================
//  HELPERS
//==============================================================================

static
xbool sdlshader_IsValidStage( shader_stage Stage )
{
    return ( Stage == SHADER_STAGE_VERTEX ) ||
           ( Stage == SHADER_STAGE_PIXEL ) ||
           ( Stage == SHADER_STAGE_COMPUTE );
}

//==============================================================================

static
u32 sdlshader_HashBinding( shader_binding_kind Kind, const char* pName )
{
    u32 Hash = 2166136261u;

    while( *pName )
    {
        Hash ^= (u8)*pName++;
        Hash *= 16777619u;
    }

    Hash ^= (u32)Kind + 0x9E3779B9u + ( Hash << 6 ) + ( Hash >> 2 );

    return Hash;
}

//==============================================================================

static
xbool sdlshader_BuildBindingLookup( shader_backend& Backend )
{
    Backend.BindingLookupCount = 0;
    Backend.BindingLookupMask  = 0;

    const u32 BindingCount = Backend.BindingCount;

    if( BindingCount == 0 )
    {
        return TRUE;
    }

    if( BindingCount > ( SDLENG_SHADER_BINDING_LOOKUP_SLOTS / 2 ) )
    {
        x_DebugMsg( "SDLShader: binding reflection lookup exceeds bounded storage\n" );
        return FALSE;
    }

    u32 TableSize = 1;

    while( TableSize < ( BindingCount * 2 ) )
    {
        TableSize <<= 1;
    }

    Backend.BindingLookupCount = TableSize;
    Backend.BindingLookupMask  = TableSize - 1;

    for( u32 i = 0; i < TableSize; ++i )
    {
        Backend.BindingLookup[i] = -1;
    }

    for( u32 BindingIndex = 0; BindingIndex < BindingCount; ++BindingIndex )
    {
        const shader_binding_record& Binding     = Backend.Bindings[BindingIndex];
        u32                          LookupIndex = sdlshader_HashBinding( Binding.Kind, Binding.Name ) & Backend.BindingLookupMask;

        for( u32 Probe = 0; Probe < TableSize; ++Probe )
        {
            const s32 ExistingIndex = Backend.BindingLookup[LookupIndex];

            if( ExistingIndex < 0 )
            {
                Backend.BindingLookup[LookupIndex] = (s32)BindingIndex;
                break;
            }

            const shader_binding_record& Existing = Backend.Bindings[ExistingIndex];

            if( ( Existing.Kind == Binding.Kind ) && ( x_strcmp( Existing.Name, Binding.Name ) == 0 ) )
            {
                break;
            }

            LookupIndex = ( LookupIndex + 1 ) & Backend.BindingLookupMask;
        }
    }

    return TRUE;
}

//==============================================================================

static
u32 sdlshader_StageCacheIndex( shader_stage Stage )
{
    return ( Stage == SHADER_STAGE_VERTEX ) ? 0u : 1u;
}

//==============================================================================

static
xbool sdlshader_CacheSamplerBinding( shader_stage    Stage,
                                     u32             Slot,
                                     SDL_GPUTexture* pTexture,
                                     SDL_GPUSampler* pSampler )
{
    sdlshader_sampler_cache_entry& Entry =
        s_SamplerBindings[sdlshader_StageCacheIndex( Stage )][Slot];

    if( ( Entry.Serial == s_ShaderBindingSerial ) &&
        ( Entry.pTexture == pTexture ) &&
        ( Entry.pSampler == pSampler ) )
    {
        return FALSE;
    }

    Entry.pTexture = pTexture;
    Entry.pSampler = pSampler;
    Entry.Serial   = s_ShaderBindingSerial;

    return TRUE;
}

//==============================================================================

static
xbool sdlshader_CacheStorageTextureBinding( shader_stage    Stage,
                                            u32             Slot,
                                            SDL_GPUTexture* pTexture )
{
    sdlshader_texture_cache_entry& Entry =
        s_StorageTextureBindings[sdlshader_StageCacheIndex( Stage )][Slot];

    if( ( Entry.Serial == s_ShaderBindingSerial ) && ( Entry.pTexture == pTexture ) )
    {
        return FALSE;
    }

    Entry.pTexture = pTexture;
    Entry.Serial   = s_ShaderBindingSerial;

    return TRUE;
}

//==============================================================================

static
xbool sdlshader_CacheStorageBufferBinding( shader_stage   Stage,
                                           u32            Slot,
                                           SDL_GPUBuffer* pBuffer )
{
    sdlshader_buffer_cache_entry& Entry =
        s_StorageBufferBindings[sdlshader_StageCacheIndex( Stage )][Slot];

    if( ( Entry.Serial == s_ShaderBindingSerial ) && ( Entry.pBuffer == pBuffer ) )
    {
        return FALSE;
    }

    Entry.pBuffer = pBuffer;
    Entry.Serial  = s_ShaderBindingSerial;

    return TRUE;
}

//==============================================================================

static
void sdlshader_PrepareBatch( xbool* pUsed, u32 Count )
{
    for( u32 Slot = 0; Slot < Count; ++Slot )
    {
        pUsed[Slot] = FALSE;
    }
}

//==============================================================================

static
void sdlshader_SubmitSamplerBatch( SDL_GPURenderPass*            pRenderPass,
                                   shader_stage                  Stage,
                                   SDL_GPUTextureSamplerBinding* pBindings,
                                   xbool*                        pUsed,
                                   u32                           Count )
{
    u32 Slot = 0;

    while( Slot < Count )
    {
        while( ( Slot < Count ) && !pUsed[Slot] )
        {
            ++Slot;
        }

        if( Slot == Count )
        {
            break;
        }

        const u32 FirstSlot = Slot;

        while( ( Slot < Count ) && pUsed[Slot] )
        {
            ++Slot;
        }

        if( Stage == SHADER_STAGE_VERTEX )
        {
            SDL_BindGPUVertexSamplers( pRenderPass, FirstSlot, &pBindings[FirstSlot], Slot - FirstSlot );
        }
        else
        {
            SDL_BindGPUFragmentSamplers( pRenderPass, FirstSlot, &pBindings[FirstSlot], Slot - FirstSlot );
        }
    }
}

//==============================================================================

static
void sdlshader_SubmitStorageTextureBatch( SDL_GPURenderPass* pRenderPass,
                                          shader_stage       Stage,
                                          SDL_GPUTexture**   pBindings,
                                          xbool*             pUsed,
                                          u32                Count )
{
    u32 Slot = 0;

    while( Slot < Count )
    {
        while( ( Slot < Count ) && !pUsed[Slot] )
        {
            ++Slot;
        }

        if( Slot == Count )
        {
            break;
        }

        const u32 FirstSlot = Slot;

        while( ( Slot < Count ) && pUsed[Slot] )
        {
            ++Slot;
        }

        if( Stage == SHADER_STAGE_VERTEX )
        {
            SDL_BindGPUVertexStorageTextures( pRenderPass, FirstSlot, &pBindings[FirstSlot], Slot - FirstSlot );
        }
        else
        {
            SDL_BindGPUFragmentStorageTextures( pRenderPass, FirstSlot, &pBindings[FirstSlot], Slot - FirstSlot );
        }
    }
}

//==============================================================================

static
void sdlshader_SubmitStorageBufferBatch( SDL_GPURenderPass* pRenderPass,
                                         shader_stage       Stage,
                                         SDL_GPUBuffer**    pBindings,
                                         xbool*             pUsed,
                                         u32                Count )
{
    u32 Slot = 0;

    while( Slot < Count )
    {
        while( ( Slot < Count ) && !pUsed[Slot] )
        {
            ++Slot;
        }

        if( Slot == Count )
        {
            break;
        }

        const u32 FirstSlot = Slot;

        while( ( Slot < Count ) && pUsed[Slot] )
        {
            ++Slot;
        }

        if( Stage == SHADER_STAGE_VERTEX )
        {
            SDL_BindGPUVertexStorageBuffers( pRenderPass, FirstSlot, &pBindings[FirstSlot], Slot - FirstSlot );
        }
        else
        {
            SDL_BindGPUFragmentStorageBuffers( pRenderPass, FirstSlot, &pBindings[FirstSlot], Slot - FirstSlot );
        }
    }
}

//==============================================================================

void sdleng_ResetShaderBindings( void )
{
    ++s_ShaderBindingSerial;

    if( s_ShaderBindingSerial == 0 )
    {
        s_ShaderBindingSerial = 1;

        for( u32 Stage = 0; Stage < 2; ++Stage )
        {
            for( u32 Slot = 0; Slot < SDLENG_MAX_SAMPLER_SLOTS; ++Slot )
            {
                s_SamplerBindings[Stage][Slot].Serial = 0;
            }

            for( u32 Slot = 0; Slot < SDLENG_MAX_STORAGE_TEXTURE_SLOTS; ++Slot )
            {
                s_StorageTextureBindings[Stage][Slot].Serial = 0;
            }

            for( u32 Slot = 0; Slot < SDLENG_MAX_STORAGE_BUFFER_SLOTS; ++Slot )
            {
                s_StorageBufferBindings[Stage][Slot].Serial = 0;
            }
        }
    }
}

//==============================================================================

static
SDL_GPUShaderFormat sdlshader_ToSDLFormat( shader_format Format )
{
    switch( Format )
    {
        case SHADER_FORMAT_SPIRV:    return SDL_GPU_SHADERFORMAT_SPIRV;    break;
        case SHADER_FORMAT_DXIL:     return SDL_GPU_SHADERFORMAT_DXIL;     break;
        case SHADER_FORMAT_MSL:      return SDL_GPU_SHADERFORMAT_MSL;      break;
        case SHADER_FORMAT_METALLIB: return SDL_GPU_SHADERFORMAT_METALLIB; break;
        case SHADER_FORMAT_PRIVATE:  return SDL_GPU_SHADERFORMAT_PRIVATE;  break;
        default:                     return SDL_GPU_SHADERFORMAT_INVALID;  break;
    }
}

//==============================================================================

static
void sdlshader_AddFormatCandidate( shader_format* pFormats,
                                   u32&           Count,
                                   shader_format  Format )
{
    for( u32 i = 0; i < Count; i++ )
    {
        if( pFormats[i] == Format )
        {
            return;
        }
    }

    if( Count < 5 )
    {
        pFormats[Count++] = Format;
    }
}

//==============================================================================

static
xbool sdlshader_ToSDLGraphicsStage( shader_stage Stage, SDL_GPUShaderStage& SDLStage )
{
    switch( Stage )
    {
        case SHADER_STAGE_VERTEX:
        {
            SDLStage = SDL_GPU_SHADERSTAGE_VERTEX;
            return TRUE;
        }

        case SHADER_STAGE_PIXEL:
        {
            SDLStage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//==============================================================================

static
void sdlshader_ReleaseBackend( shader_backend* pBackend )
{
    if( !pBackend )
    {
        return;
    }

    pBackend->Native.Clear();

    pBackend->pShader          = NULL;
    pBackend->pComputePipeline = NULL;
}

//==============================================================================

static
xbool sdlshader_LoadFileBlob( shader_blob& Blob, const char* pFileName )
{
    Blob = shader_blob();

    if( !pFileName )
    {
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );

    if( !pFile )
    {
        x_DebugMsg( "SDLShader: failed to open shader file '%s'\n", pFileName );
        return FALSE;
    }

    const s32 FileLength = x_flength( pFile );

    if( FileLength <= 0 )
    {
        x_DebugMsg( "SDLShader: shader file '%s' is empty\n", pFileName );
        x_fclose( pFile );
        return FALSE;
    }

    void* pData = x_malloc( FileLength );

    if( !pData )
    {
        x_fclose( pFile );
        return FALSE;
    }

    const s32 BytesRead = x_fread( pData, 1, FileLength, pFile );
    x_fclose( pFile );

    if( BytesRead != FileLength )
    {
        x_DebugMsg( "SDLShader: failed to read shader file '%s' (%d/%d bytes)\n",
                    pFileName,
                    BytesRead,
                    FileLength );

        x_free( pData );
        return FALSE;
    }

    Blob.pData = pData;
    Blob.Size  = (u32)FileLength;

    return TRUE;
}

//==============================================================================

static
void sdlshader_FreeFileBlob( shader_blob& Blob )
{
    if( Blob.pData )
    {
        x_free( (void*)Blob.pData );
    }

    Blob = shader_blob();
}

//==============================================================================

static
xbool sdlshader_GetCommandBuffer( SDL_GPUCommandBuffer*& pCommandBuffer )
{
    pCommandBuffer = sdleng_GetCommandBuffer();

    if( !pCommandBuffer )
    {
        x_DebugMsg( "SDLShader: no active command buffer for shader binding\n" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlshader_GetRenderPass( SDL_GPURenderPass*& pRenderPass )
{
    pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        x_DebugMsg( "SDLShader: no active render pass for graphics resource binding\n" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlshader_ValidateComputeInput( shader_binding_kind Kind, u32 Slot, SDL_GPUTexture* pTexture, SDL_GPUBuffer* pBuffer )
{
    if( !sdleng_InComputePass() || !s_ComputeState.pShader )
    {
        return FALSE;
    }

    const shader_backend& Backend = *s_ComputeState.pShader;
    const u32             Count   = Kind == SHADER_BINDING_SAMPLED_TEXTURE ? Backend.Resources.SamplerCount : Kind == SHADER_BINDING_STORAGE_TEXTURE ? Backend.Resources.StorageTextureCount
                                                                                                                                                     : Backend.Resources.StorageBufferCount;

    if( Slot >= Count )
    {
        return FALSE;
    }

    for( u32 i = 0; i < Backend.Compute.ReadWriteTextureCount; ++i )
    {
        if( pTexture && pTexture == s_ComputeState.pTextures[i] )
        {
            return FALSE;
        }
    }

    for( u32 i = 0; i < Backend.Compute.ReadWriteBufferCount; ++i )
    {
        if( pBuffer && pBuffer == s_ComputeState.pBuffers[i] )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================
//  SHADER LIFETIME
//==============================================================================

void shader_SetCatalogId( u64 CatalogId )
{
    if( s_ShaderCatalogId != CatalogId )
    {
        s_ShaderPackages.Clear();
    }

    s_ShaderCatalogId = CatalogId;
}

//==============================================================================

void shader_Init( void )
{
#if defined( A51_GAME_SHADER_CONTRACT )
    s_ShaderCatalogId = A51ShaderCatalogId;
#else
    s_ShaderCatalogId = 0;
#endif
    for( u32 Stage = 0; Stage < 3; ++Stage )
    {
        for( u32 Slot = 0; Slot < SDLENG_MAX_UNIFORM_BUFFER_SLOTS; ++Slot )
        {
            s_UniformData[Stage][Slot].SetCapacity( 2048 );
            s_UniformData[Stage][Slot].SetCount( 0 );
        }
    }

    s_pShaderList = NULL;
    s_ShaderCount = 0;
}

//==============================================================================

void shader_Kill( void )
{
    sdleng_EndComputePass();
    x_memset( &s_ComputeState, 0, sizeof( s_ComputeState ) );
    s_ShaderPackages.Clear();
    SDL_LockSpinlock( &s_NativeStageLock );

    for( s32 Bucket = 0; Bucket < SDLENG_NATIVE_STAGE_BUCKETS; ++Bucket )
    {
        s_NativeStages[Bucket].Clear();
    }

    SDL_UnlockSpinlock( &s_NativeStageLock );

    for( u32 Stage = 0; Stage < 3; ++Stage )
    {
        for( u32 Slot = 0; Slot < SDLENG_MAX_UNIFORM_BUFFER_SLOTS; ++Slot )
        {
            s_UniformData[Stage][Slot].Clear();
        }
    }

    if( s_pShaderList )
    {
        sdleng_ResetShaderBindings();
    }

    while( s_pShaderList )
    {
        shader_backend* pBackend = s_pShaderList;
        sdleng_UnlinkBackend( s_pShaderList, pBackend, s_ShaderCount );
        sdlshader_ReleaseBackend( pBackend );

        if( pBackend->pOwner )
        {
            pBackend->pOwner->Stage    = SHADER_STAGE_VERTEX;
            pBackend->pOwner->Format   = SHADER_FORMAT_UNKNOWN;
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        delete pBackend;
    }
}

//==============================================================================

static
xbool sdlshader_Create( shader& Shader, const shader_desc& Desc, const x_content_id* pContent )
{
    if( Shader.pBackend && ( sdleng_InRenderPass() || sdleng_InComputePass() ) )
    {
        return FALSE;
    }

    if( !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    if( !sdlshader_IsValidStage( Desc.Stage ) || !Desc.Blob.pData || ( Desc.Blob.Size == 0 ) )
    {
        return FALSE;
    }

    if( Desc.BindingCount && !Desc.pBindings )
    {
        return FALSE;
    }

    if( Desc.BindingCount > SDLENG_MAX_SHADER_BINDINGS )
    {
        return FALSE;
    }

    if( Desc.Resources.SamplerCount > SDLENG_MAX_SAMPLER_SLOTS ||
        Desc.Resources.StorageTextureCount > SDLENG_MAX_STORAGE_TEXTURE_SLOTS ||
        Desc.Resources.StorageBufferCount > SDLENG_MAX_STORAGE_BUFFER_SLOTS ||
        Desc.Resources.UniformBufferCount > SDLENG_MAX_UNIFORM_BUFFER_SLOTS )
    {
        x_DebugMsg( "SDLShader: resource counts exceed SDL GPU per-stage limits\n" );
        return FALSE;
    }

    for( u32 i = 0; i < Desc.BindingCount; ++i )
    {
        const shader_binding_desc& Binding = Desc.pBindings[i];

        if( !Binding.pName || !Binding.pName[0] || ( Binding.Count == 0 ) )
        {
            return FALSE;
        }

        u32 SlotLimit     = 0;
        u32 ResourceCount = 0;

        switch( Binding.Kind )
        {
            case SHADER_BINDING_SAMPLED_TEXTURE:
            {
                SlotLimit     = SDLENG_MAX_SAMPLER_SLOTS;
                ResourceCount = Desc.Resources.SamplerCount;
                break;
            }

            case SHADER_BINDING_UNIFORM_BUFFER:
            {
                SlotLimit     = SDLENG_MAX_UNIFORM_BUFFER_SLOTS;
                ResourceCount = Desc.Resources.UniformBufferCount;
                break;
            }

            case SHADER_BINDING_STORAGE_TEXTURE:
            {
                SlotLimit     = SDLENG_MAX_STORAGE_TEXTURE_SLOTS;
                ResourceCount = Desc.Resources.StorageTextureCount;
                break;
            }

            case SHADER_BINDING_STORAGE_BUFFER:
            {
                SlotLimit     = SDLENG_MAX_STORAGE_BUFFER_SLOTS;
                ResourceCount = Desc.Resources.StorageBufferCount;
                break;
            }

            case SHADER_BINDING_READ_WRITE_TEXTURE:
            {
                SlotLimit     = SDLENG_MAX_STORAGE_TEXTURE_SLOTS;
                ResourceCount = Desc.Compute.ReadWriteTextureCount;
                break;
            }

            case SHADER_BINDING_READ_WRITE_BUFFER:
            {
                SlotLimit     = SDLENG_MAX_STORAGE_BUFFER_SLOTS;
                ResourceCount = Desc.Compute.ReadWriteBufferCount;
                break;
            }

            default:
            {
                return FALSE;
            }
        }

        if( ( Binding.Slot >= SlotLimit ) ||
            ( Binding.Count > ( SlotLimit - Binding.Slot ) ) ||
            ( Binding.Slot >= ResourceCount ) ||
            ( Binding.Count > ( ResourceCount - Binding.Slot ) ) )
        {
            x_DebugMsg( "SDLShader: binding '%s' exceeds SDL GPU slot limits\n", Binding.pName );
            return FALSE;
        }
    }

    if( Desc.Stage == SHADER_STAGE_COMPUTE )
    {
        const shader_compute_desc& Compute = Desc.Compute;

        if( !Compute.ThreadCountX || !Compute.ThreadCountY || !Compute.ThreadCountZ ||
            Compute.ThreadCountX > 1024 || Compute.ThreadCountY > 1024 || Compute.ThreadCountZ > 64 ||
            static_cast<u64>( Compute.ThreadCountX ) * Compute.ThreadCountY * Compute.ThreadCountZ > 1024 ||
            Compute.ReadWriteTextureCount > SDLENG_MAX_STORAGE_TEXTURE_SLOTS ||
            Compute.ReadWriteBufferCount > SDLENG_MAX_STORAGE_BUFFER_SLOTS )
        {
            x_DebugMsg( "SDLShader: invalid compute workgroup or read-write resource counts\n" );
            return FALSE;
        }
    }
    else if( Desc.Compute.ReadWriteTextureCount || Desc.Compute.ReadWriteBufferCount ||
             Desc.Compute.ThreadCountX || Desc.Compute.ThreadCountY || Desc.Compute.ThreadCountZ )
    {
        return FALSE;
    }

    SDL_GPUShaderStage SDLStage = SDL_GPU_SHADERSTAGE_VERTEX;

    if( Desc.Stage != SHADER_STAGE_COMPUTE && !sdlshader_ToSDLGraphicsStage( Desc.Stage, SDLStage ) )
    {
        return FALSE;
    }

    const SDL_GPUShaderFormat SDLFormat = sdlshader_ToSDLFormat( Desc.Format );

    if( SDLFormat == SDL_GPU_SHADERFORMAT_INVALID )
    {
        return FALSE;
    }

    const SDL_GPUShaderFormat SupportedFormats = SDL_GetGPUShaderFormats( g_pSDLGPUDevice );

    if( ( SupportedFormats & SDLFormat ) == 0 )
    {
        x_DebugMsg( "SDLShader: shader format 0x%08X is not supported by the active SDL GPU device\n",
                    SDLFormat );

        return FALSE;
    }

    shader_backend* pBackend = new shader_backend;

    if( !pBackend )
    {
        return FALSE;
    }

    const char* pEntryPoint = Desc.pEntryPoint ? Desc.pEntryPoint : "main";

    SDL_GPUShaderCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof( CreateInfo ) );
    CreateInfo.code_size            = Desc.Blob.Size;
    CreateInfo.code                 = (const Uint8*)Desc.Blob.pData;
    CreateInfo.entrypoint           = pEntryPoint;
    CreateInfo.format               = SDLFormat;
    CreateInfo.stage                = SDLStage;
    CreateInfo.num_samplers         = Desc.Resources.SamplerCount;
    CreateInfo.num_storage_textures = Desc.Resources.StorageTextureCount;
    CreateInfo.num_storage_buffers  = Desc.Resources.StorageBufferCount;
    CreateInfo.num_uniform_buffers  = Desc.Resources.UniformBufferCount;

    xarray<byte> NativeKey;

    if( !NativeStageKey( Desc, pContent, NativeKey ) )
    {
        delete pBackend;
        return FALSE;
    }

    const x_content_id Identity = x_HashContent( NativeKey.GetPtr(), NativeKey.GetCount() );
    SDL_LockSpinlock( &s_NativeStageLock );
    xarray<native_stage_entry>& Bucket = s_NativeStages[Identity[0]];
    xref<shader_native>         Native;

    for( s32 i = Bucket.GetCount() - 1; i >= 0; --i )
    {
        if( Bucket[i].Key.GetCount() == NativeKey.GetCount() &&
            x_memcmp( Bucket[i].Key.GetPtr(), NativeKey.GetPtr(), NativeKey.GetCount() ) == 0 )
        {
            Native.Set( Bucket[i].pNative );
            break;
        }
    }

    SDL_UnlockSpinlock( &s_NativeStageLock );
    SDL_GPUShader*          pShader          = Native ? Native->pShader : NULL;
    SDL_GPUComputePipeline* pComputePipeline = Native ? Native->pComputePipeline : NULL;

    if( !Native )
    {
        SDL_PropertiesID Props = 0;

        if( Desc.pDebugName && Desc.pDebugName[0] )
        {
            Props = SDL_CreateProperties();

            if( !Props )
            {
                sdleng_LogError( "SDLShader", "SDL_CreateProperties" );
                sdlshader_ReleaseBackend( pBackend );
                delete pBackend;
                return FALSE;
            }

            const char* pNameProperty = Desc.Stage == SHADER_STAGE_COMPUTE
                                            ? SDL_PROP_GPU_COMPUTEPIPELINE_CREATE_NAME_STRING
                                            : SDL_PROP_GPU_SHADER_CREATE_NAME_STRING;

            if( !SDL_SetStringProperty( Props, pNameProperty, Desc.pDebugName ) )
            {
                sdleng_LogError( "SDLShader", "SDL_SetStringProperty" );
                SDL_DestroyProperties( Props );
                sdlshader_ReleaseBackend( pBackend );
                delete pBackend;
                return FALSE;
            }
        }

        CreateInfo.props = Props;
        static xprofile_zone ShaderCreateAPI =
            x_GetProfiler().RegisterZone( "ShaderCreateAPI", "RendererAPI" );

        const xtick Start = x_GetTime();

        if( Desc.Stage == SHADER_STAGE_COMPUTE )
        {
            SDL_GPUComputePipelineCreateInfo ComputeInfo = {};
            ComputeInfo.code_size                        = Desc.Blob.Size;
            ComputeInfo.code                             = static_cast<const Uint8*>( Desc.Blob.pData );
            ComputeInfo.entrypoint                       = pEntryPoint;
            ComputeInfo.format                           = SDLFormat;
            ComputeInfo.num_samplers                     = Desc.Resources.SamplerCount;
            ComputeInfo.num_readonly_storage_textures    = Desc.Resources.StorageTextureCount;
            ComputeInfo.num_readonly_storage_buffers     = Desc.Resources.StorageBufferCount;
            ComputeInfo.num_readwrite_storage_textures   = Desc.Compute.ReadWriteTextureCount;
            ComputeInfo.num_readwrite_storage_buffers    = Desc.Compute.ReadWriteBufferCount;
            ComputeInfo.num_uniform_buffers              = Desc.Resources.UniformBufferCount;
            ComputeInfo.threadcount_x                    = Desc.Compute.ThreadCountX;
            ComputeInfo.threadcount_y                    = Desc.Compute.ThreadCountY;
            ComputeInfo.threadcount_z                    = Desc.Compute.ThreadCountZ;
            ComputeInfo.props                            = Props;
            pComputePipeline                             = SDL_CreateGPUComputePipeline( g_pSDLGPUDevice, &ComputeInfo );
        }
        else
        {
            pShader = SDL_CreateGPUShader( g_pSDLGPUDevice, &CreateInfo );
        }

        ShaderCreateAPI.Record( x_GetTime() - Start );

        if( Props )
        {
            SDL_DestroyProperties( Props );
        }
    }

    if( !pShader && !pComputePipeline )
    {
        sdleng_LogError( "SDLShader", Desc.Stage == SHADER_STAGE_COMPUTE ? "SDL_CreateGPUComputePipeline" : "SDL_CreateGPUShader" );

        sdlshader_ReleaseBackend( pBackend );
        delete pBackend;
        return FALSE;
    }

    if( !Native )
    {
        Native.Set( new shader_native );
        Native->pDevice          = g_pSDLGPUDevice;
        Native->pShader          = pShader;
        Native->pComputePipeline = pComputePipeline;
        Native->Serial           = sdlshader_AllocateSerial();
        Native->RegistryBucket   = Identity[0];
        SDL_LockSpinlock( &s_NativeStageLock );
        native_stage_entry& Entry = Bucket.Append();
        Entry.Key                 = NativeKey;
        Entry.pNative             = Native.Get();
        SDL_UnlockSpinlock( &s_NativeStageLock );
    }

    pBackend->Native           = Native;
    pBackend->pShader          = pShader;
    pBackend->pComputePipeline = pComputePipeline;
    pBackend->Serial           = Native->Serial;

    if( pBackend->Serial == 0 )
    {
        sdlshader_ReleaseBackend( pBackend );
        delete pBackend;
        return FALSE;
    }

    pBackend->Stage        = Desc.Stage;
    pBackend->Compute      = Desc.Compute;
    pBackend->Format       = Desc.Format;
    pBackend->SDLFormat    = SDLFormat;
    pBackend->Resources    = Desc.Resources;
    pBackend->BindingCount = Desc.BindingCount;

    for( u32 i = 0; i < Desc.BindingCount; i++ )
    {
        const shader_binding_desc& DescBinding = Desc.pBindings[i];
        shader_binding_record&     Binding     = pBackend->Bindings[i];
        Binding.Kind                           = DescBinding.Kind;
        Binding.Name                           = DescBinding.pName;
        Binding.Slot                           = DescBinding.Slot;
        Binding.Count                          = DescBinding.Count;
    }

    if( !sdlshader_BuildBindingLookup( *pBackend ) )
    {
        sdlshader_ReleaseBackend( pBackend );
        delete pBackend;
        return FALSE;
    }

    if( !shader_Destroy( Shader ) )
    {
        sdlshader_ReleaseBackend( pBackend );
        delete pBackend;
        return FALSE;
    }

    pBackend->pOwner = &Shader;
    Shader.Stage     = Desc.Stage;
    Shader.Format    = Desc.Format;
    Shader.pBackend  = pBackend;
#if defined( A51_GAME_SHADER_CONTRACT )
    for( u32 Semantic = 0; Semantic < A51ResourceCount; ++Semantic )
    {
        for( u32 Binding = 0; Binding < Desc.BindingCount; ++Binding )
        {
            if( !x_strcmp( A51ResourceNames[Semantic], Desc.pBindings[Binding].pName ) )
            {
                Shader.ResourceSlots[Semantic] = Desc.pBindings[Binding].Slot;
            }
        }
    }
#endif

    sdleng_LinkBackend( s_pShaderList, pBackend, s_ShaderCount );

    return TRUE;
}

//==============================================================================

xbool shader_Create( shader& Shader, const shader_desc& Desc )
{
    return sdlshader_Create( Shader, Desc, NULL );
}

//==============================================================================

xbool shader_LoadFromFile( shader& Shader, const shader_file_desc& Desc )
{
    shader_blob Blob;

    if( !sdlshader_LoadFileBlob( Blob, Desc.pFileName ) )
    {
        return FALSE;
    }

    shader_desc ShaderDesc;
    ShaderDesc.Stage        = Desc.Stage;
    ShaderDesc.Format       = Desc.Format;
    ShaderDesc.Blob         = Blob;
    ShaderDesc.pEntryPoint  = Desc.pEntryPoint;
    ShaderDesc.Resources    = Desc.Resources;
    ShaderDesc.Compute      = Desc.Compute;
    ShaderDesc.pBindings    = Desc.pBindings;
    ShaderDesc.BindingCount = Desc.BindingCount;
    ShaderDesc.pDebugName   = Desc.pDebugName ? Desc.pDebugName : Desc.pFileName;

    const xbool Result = shader_Create( Shader, ShaderDesc );
    sdlshader_FreeFileBlob( Blob );

    return Result;
}

//==============================================================================
//  PACKAGED SHADER PROGRAMS
//==============================================================================

static
u32 sdlshader_PackageFormat( void )
{
    const SDL_GPUShaderFormat Supported = SDL_GetGPUShaderFormats( g_pSDLGPUDevice );
    const char*               pDriver   = SDL_GetGPUDeviceDriver( g_pSDLGPUDevice );

    if( pDriver && x_stristr( pDriver, "vulkan" ) )
    {
        return 1;
    }

    if( pDriver && ( x_stristr( pDriver, "d3d12" ) || x_stristr( pDriver, "direct3d" ) ) )
    {
        return 2;
    }

    return ( Supported & SDL_GPU_SHADERFORMAT_SPIRV ) ? 1 : ( Supported & SDL_GPU_SHADERFORMAT_DXIL ) ? 2
                                                                                                      : 0;
}

//==============================================================================

shader_format shader_GetPreferredFormat( void )
{
    if( !g_pSDLGPUDevice )
    {
        return SHADER_FORMAT_UNKNOWN;
    }

    const u32 Format = sdlshader_PackageFormat();

    return Format == 1 ? SHADER_FORMAT_SPIRV : Format == 2 ? SHADER_FORMAT_DXIL
                                                           : SHADER_FORMAT_UNKNOWN;
}

//==============================================================================

static
xbool sdlshader_CreateProgramView( shader& Shader, const shader_program_desc& Desc, const ecs_program_view& View )
{
    const u32   Format  = View.Program->Format;
    const auto& Program = *View.Program;
    const auto& Layout  = *View.Layout();
    const auto& Binary  = View.Package->Index.Binaries[Program.Binary];

    if( Layout.Bindings.GetCount() > SDLENG_MAX_SHADER_BINDINGS )
    {
        return FALSE;
    }

    const byte* pCode = View.Code();

    if( ( Format == 1 && ( Binary.Size < 20 || ( Binary.Size & 3 ) || x_load_le32( pCode ) != 0x07230203u ) ) ||
        ( Format == 2 &&
          ( Binary.Size < 32 || x_memcmp( pCode, "DXBC", 4 ) || x_load_le32( pCode + 24 ) != Binary.Size ) ) )
    {
        x_DebugMsg( "SDLShader: invalid target bytecode for program %016llx in '%s'\n", (unsigned long long)Program.Id, Desc.pFileName );

        return FALSE;
    }

    shader_binding_desc Bindings[SDLENG_MAX_SHADER_BINDINGS];

    for( u32 i = 0; i < Layout.Bindings.GetCount(); ++i )
    {
        Bindings[i].Kind  = static_cast<shader_binding_kind>( Layout.Bindings[i].Kind );
        Bindings[i].Slot  = Layout.Bindings[i].Slot;
        Bindings[i].Count = Layout.Bindings[i].Count;
        Bindings[i].pName = (const char*)Layout.Bindings[i].Name;
    }

    shader_desc Native;
    Native.pInterface                    = &Layout;
    Native.Stage                         = static_cast<shader_stage>( Program.Stage );
    Native.Format                        = static_cast<shader_format>( Program.Format );
    Native.Blob                          = shader_blob( View.Code(), Binary.Size );
    Native.pEntryPoint                   = (const char*)Program.Entry;
    Native.Resources.SamplerCount        = Layout.Resources[0];
    Native.Resources.UniformBufferCount  = Layout.Resources[1];
    Native.Resources.StorageTextureCount = Layout.Resources[2];
    Native.Resources.StorageBufferCount  = Layout.Resources[3];
    Native.pBindings                     = Bindings;
    Native.BindingCount                  = static_cast<u32>( Layout.Bindings.GetCount() );
    Native.pDebugName                    = Desc.pDebugName ? Desc.pDebugName : (const char*)Program.Name;

    if( !sdlshader_Create( Shader, Native, &Binary.Content ) )
    {
        return FALSE;
    }

    Shader.pBackend->Package     = View.Package;
    Shader.pBackend->PackageFile = Desc.pFileName;
    Shader.ProgramId             = Program.Id;

    if( !s_ShaderCatalogId )
    {
        s_ShaderCatalogId = View.Package->Index.CatalogId;
    }

    return TRUE;
}

//==============================================================================

xbool shader_RequestProgram( shader& Shader, const shader_program_desc& Desc )
{
    if( !Desc.pFileName || !Desc.ProgramId || !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    const u64 Catalog = Desc.ExpectedCatalogId ? Desc.ExpectedCatalogId : s_ShaderCatalogId;

    if( Shader.pLoad && Shader.ProgramId == Desc.ProgramId && Shader.pLoad->File == Desc.pFileName &&
        Shader.pLoad->Desc.ExpectedCatalogId == Catalog )
    {
        return TRUE;
    }

    if( Shader.pBackend && Shader.ProgramId == Desc.ProgramId && Shader.pBackend->Package &&
        Shader.pBackend->Package->Index.CatalogId == Catalog && Shader.pBackend->PackageFile == Desc.pFileName )
    {
        return TRUE;
    }

    shader_Destroy( Shader );
    ecs_package_ptr Package = s_ShaderPackages.Find( Desc.pFileName, Catalog );
    xstring         Error;

    if( !Package && !s_ShaderPackages.Open( Desc.pFileName, ecs_OpenPackageSource( Desc.pFileName ), Catalog, Package, Error ) )
    {
        x_DebugMsg( "SDLShader: package '%s': %s\n", Desc.pFileName, (const char*)Error );
        return FALSE;
    }

    const u32                  Format   = sdlshader_PackageFormat();
    const ecs_package_program* pProgram = ecs_FindPackageProgram( Package->Index, Desc.ProgramId, Format );

    if( !pProgram || Package->Index.Layouts[pProgram->Layout].Bindings.GetCount() > SDLENG_MAX_SHADER_BINDINGS )
    {
        return FALSE;
    }

    shader_program_load* pLoad    = new shader_program_load;
    pLoad->File                   = Desc.pFileName;
    pLoad->DebugName              = Desc.pDebugName ? Desc.pDebugName : Desc.pFileName;
    pLoad->Desc                   = Desc;
    pLoad->Desc.ExpectedCatalogId = Catalog;
    pLoad->Desc.pFileName         = pLoad->File;
    pLoad->Desc.pDebugName        = pLoad->DebugName;

    if( !s_ShaderPackages.RequestProgram( Package, Desc.ProgramId, Format, pLoad->Request, Error, SDL_GetBooleanProperty( SDL_GetGPUDeviceProperties( g_pSDLGPUDevice ), SDL_PROP_GPU_DEVICE_SHADER_FLOAT16_BOOLEAN, false ) ? ECS_SHADER_CAPABILITY_FLOAT16 : 0 ) )
    {
        x_DebugMsg( "SDLShader: program %016llx: %s\n", (unsigned long long)Desc.ProgramId, (const char*)Error );
        delete pLoad;
        return FALSE;
    }

    Shader.pLoad = pLoad;
    ++s_ShaderPendingCount;
    Shader.ProgramId = Desc.ProgramId;
    Shader.Stage     = static_cast<shader_stage>( pProgram->Stage );
    Shader.Format    = static_cast<shader_format>( Format );
#if defined( A51_GAME_SHADER_CONTRACT )
    const ecs_package_layout& Layout = Package->Index.Layouts[pProgram->Layout];

    for( u32 Semantic = 0; Semantic < A51ResourceCount; ++Semantic )
    {
        for( s32 Binding = 0; Binding < Layout.Bindings.GetCount(); ++Binding )
        {
            if( !x_strcmp( A51ResourceNames[Semantic], Layout.Bindings[Binding].Name ) )
            {
                Shader.ResourceSlots[Semantic] = Layout.Bindings[Binding].Slot;
            }
        }
    }
#endif
    return TRUE;
}

//==============================================================================

shader_program_load_status shader_PollProgram( shader& Shader )
{
    if( !Shader.pLoad )
    {
        return Shader.pBackend ? SHADER_PROGRAM_READY : SHADER_PROGRAM_FAILED;
    }

    ecs_program_view                 View;
    xstring                          Error;
    const ecs_program_request_status Status = s_ShaderPackages.PollProgram( Shader.pLoad->Request, View, Error );

    if( Status == ECS_PROGRAM_PENDING )
    {
        return SHADER_PROGRAM_PENDING;
    }

    shader_program_load* pLoad = Shader.pLoad;
    // shader_Create may destroy its destination, so detach the request first.
    Shader.pLoad        = NULL;
    const xbool Created = Status == ECS_PROGRAM_READY && sdlshader_CreateProgramView( Shader, pLoad->Desc, View );

    if( !Created && !Error.IsEmpty() )
    {
        x_DebugMsg( "SDLShader: %s: %s\n", (const char*)pLoad->File, (const char*)Error );
    }

    s_ShaderPackages.ReleaseRequest( pLoad->Request );
    delete pLoad;
    --s_ShaderPendingCount;

    return Created ? SHADER_PROGRAM_READY : Status == ECS_PROGRAM_CANCELLED ? SHADER_PROGRAM_CANCELLED
                                                                            : SHADER_PROGRAM_FAILED;
}

//==============================================================================

xbool shader_LoadProgram( shader& Shader, const shader_program_desc& Desc )
{
    if( !shader_RequestProgram( Shader, Desc ) )
    {
        return FALSE;
    }

    shader_program_load_status Status;

    while( ( Status = shader_PollProgram( Shader ) ) == SHADER_PROGRAM_PENDING )
    {
        x_DelayThread( 1 );
    }

    return Status == SHADER_PROGRAM_READY;
}

//==============================================================================

xbool shader_LoadProgram( shader& Shader, const char* pFileName, u64 ProgramId )
{
    shader_program_desc Desc;
    Desc.pFileName = pFileName;
    Desc.ProgramId = ProgramId;

    return shader_LoadProgram( Shader, Desc );
}

//==============================================================================

static
xbool sdlshader_LoadComboStage( shader& Shader, const char* pFileName, shader_combo_kind Kind, u64 Key, shader_stage Stage, xbool Asynchronous )
{
    if( !pFileName || ( Stage != SHADER_STAGE_VERTEX && Stage != SHADER_STAGE_PIXEL ) )
    {
        return FALSE;
    }

    ecs_package_ptr Package = s_ShaderPackages.Find( pFileName, s_ShaderCatalogId );
    xstring         Error;

    if( !Package &&
        !s_ShaderPackages.Open( pFileName, ecs_OpenPackageSource( pFileName ), s_ShaderCatalogId, Package, Error ) )
    {
        x_DebugMsg( "SDLShader: package '%s': %s\n", pFileName, (const char*)Error );
        return FALSE;
    }

    const auto* Combo = ecs_FindPackageCombo( Package->Index, Kind, Key );

    if( !Combo )
    {
        x_DebugMsg( "SDLShader: missing combo kind=%u key=%016llx in '%s'\n", Kind, (unsigned long long)Key, pFileName );

        return FALSE;
    }

    shader_program_desc Desc;
    Desc.pFileName = pFileName;
    Desc.ProgramId = Stage == SHADER_STAGE_VERTEX ? Combo->Vertex : Combo->Pixel;

    return Asynchronous ? shader_RequestProgram( Shader, Desc ) : shader_LoadProgram( Shader, Desc );
}

//==============================================================================

xbool shader_LoadComboStage( shader& Shader, const char* pFileName, shader_combo_kind Kind, u64 Key, shader_stage Stage )
{
    return sdlshader_LoadComboStage( Shader, pFileName, Kind, Key, Stage, FALSE );
}

//==============================================================================

xbool shader_RequestComboStage( shader& Shader, const char* pFileName, shader_combo_kind Kind, u64 Key, shader_stage Stage )
{
    return sdlshader_LoadComboStage( Shader, pFileName, Kind, Key, Stage, TRUE );
}

//==============================================================================
//  SHADER DESTRUCTION
//==============================================================================

xbool shader_Destroy( shader& Shader )
{
    if( Shader.pLoad )
    {
        s_ShaderPackages.CancelRequest( Shader.pLoad->Request );
        s_ShaderPackages.ReleaseRequest( Shader.pLoad->Request );
        delete Shader.pLoad;
        Shader.pLoad = NULL;
        --s_ShaderPendingCount;
        Shader.ProgramId = 0;
    }

    shader_backend* pBackend = Shader.pBackend;

    if( !pBackend )
    {
        return TRUE;
    }

    if( sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        ASSERTS( FALSE, "Cannot destroy a shader during an active GPU pass" );
        return FALSE;
    }

    composite_InvalidateShader( pBackend );
    const u64 Serial = pBackend->Serial;
    sdleng_ResetShaderBindings();
    sdleng_UnlinkBackend( s_pShaderList, pBackend, s_ShaderCount );
    xbool HasOtherOwner = FALSE;

    for( auto* pOther = s_pShaderList; pOther; pOther = pOther->pNext )
    {
        if( pOther->Serial == Serial )
        {
            HasOtherOwner = TRUE;
        }
    }

    if( !HasOtherOwner )
    {
        sdlstate_InvalidateShaderPipelines( Serial );
    }

    if( pBackend->Package )
    {
        xbool HasPackageOwner = FALSE;

        for( auto* pOther = s_pShaderList; pOther; pOther = pOther->pNext )
        {
            if( pOther->Package && pOther->Package->Index.PackageId == pBackend->Package->Index.PackageId &&
                pOther->Package->Index.CatalogId == pBackend->Package->Index.CatalogId )
            {
                HasPackageOwner = TRUE;
            }
        }

        if( !HasPackageOwner )
        {
            s_ShaderPackages.Close( pBackend->PackageFile, pBackend->Package->Index.CatalogId );
        }
    }

    if( !s_ShaderCount && !s_ShaderPendingCount )
    {
        s_ShaderPackages.Clear();
    }

    sdlshader_ReleaseBackend( pBackend );
    delete pBackend;

    Shader.Stage     = SHADER_STAGE_VERTEX;
    Shader.Format    = SHADER_FORMAT_UNKNOWN;
    Shader.pBackend  = NULL;
    Shader.ProgramId = 0;

    for( u32& Slot : Shader.ResourceSlots )
    {
        Slot = 0xFFFFFFFFu;
    }

    return TRUE;
}

//==============================================================================
//  COMPUTE PASSES
//==============================================================================

xbool shader_BeginComputePass( const shader& Shader, const shader_compute_pass_desc& Desc )
{
    const shader_backend* pBackend = Shader.pBackend;

    if( !pBackend || Shader.Stage != SHADER_STAGE_COMPUTE || !pBackend->pComputePipeline ||
        sdleng_InRenderPass() || sdleng_InComputePass() ||
        Desc.TextureCount != pBackend->Compute.ReadWriteTextureCount ||
        Desc.BufferCount != pBackend->Compute.ReadWriteBufferCount ||
        ( Desc.TextureCount && !Desc.pTextures ) || ( Desc.BufferCount && !Desc.pBuffers ) )
    {
        return FALSE;
    }

    SDL_GPUStorageTextureReadWriteBinding Textures[SDLENG_MAX_STORAGE_TEXTURE_SLOTS] = {};
    SDL_GPUStorageBufferReadWriteBinding  Buffers[SDLENG_MAX_STORAGE_BUFFER_SLOTS]   = {};

    for( u32 i = 0; i < Desc.TextureCount; ++i )
    {
        const shader_compute_texture_binding& Binding  = Desc.pTextures[i];
        const shader_resource*                pTexture = Binding.pTexture;

        if( !pTexture || !pTexture->pBackend || !pTexture->pBackend->pTexture ||
            pTexture->pBackend->Kind != SDLENG_SHADER_RESOURCE_TEXTURE ||
            !( pTexture->pBackend->UsageFlags & ( SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE |
                                                  SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE ) ) ||
            Binding.MipLevel >= pTexture->pBackend->MipCount || Binding.Layer >= pTexture->pBackend->LayerCount )
        {
            return FALSE;
        }

        for( u32 j = 0; j < i; ++j )
        {
            if( Textures[j].texture == pTexture->pBackend->pTexture )
            {
                if( ( Textures[j].mip_level == Binding.MipLevel && Textures[j].layer == Binding.Layer ) ||
                    Textures[j].cycle || Binding.bCycle )
                {
                    return FALSE;
                }
            }
        }

        Textures[i].texture   = pTexture->pBackend->pTexture;
        Textures[i].mip_level = Binding.MipLevel;
        Textures[i].layer     = Binding.Layer;
        Textures[i].cycle     = Binding.bCycle != FALSE;
    }

    for( u32 i = 0; i < Desc.BufferCount; ++i )
    {
        const shader_compute_buffer_binding& Binding = Desc.pBuffers[i];
        const rbuffer*                       pBuffer = Binding.pBuffer;

        if( !pBuffer || !pBuffer->pBackend || !pBuffer->pBackend->pBuffer ||
            !( pBuffer->Desc.UsageFlags & RBUFFER_USAGE_COMPUTE_STORAGE_WRITE ) )
        {
            return FALSE;
        }

        for( u32 j = 0; j < i; ++j )
        {
            if( Buffers[j].buffer == pBuffer->pBackend->pBuffer )
            {
                return FALSE;
            }
        }

        Buffers[i].buffer = pBuffer->pBackend->pBuffer;
        Buffers[i].cycle  = Binding.bCycle != FALSE;
    }

    if( !sdleng_BeginComputePass( Textures, Desc.TextureCount, Buffers, Desc.BufferCount ) )
    {
        return FALSE;
    }

    x_memset( &s_ComputeState, 0, sizeof( s_ComputeState ) );
    s_ComputeState.pShader = pBackend;

    for( u32 i = 0; i < Desc.TextureCount; ++i )
    {
        s_ComputeState.pTextures[i] = Textures[i].texture;
    }

    for( u32 i = 0; i < Desc.BufferCount; ++i )
    {
        s_ComputeState.pBuffers[i] = Buffers[i].buffer;
    }

    for( u32 i = 0; i < SDLENG_MAX_UNIFORM_BUFFER_SLOTS; ++i )
    {
        s_UniformData[SHADER_STAGE_COMPUTE][i].SetCount( 0 );
    }

    SDL_BindGPUComputePipeline( sdleng_GetComputePass(), pBackend->pComputePipeline );

    return TRUE;
}

//==============================================================================

xbool shader_EndComputePass( void )
{
    if( !sdleng_InComputePass() )
    {
        return FALSE;
    }

    sdleng_EndComputePass();
    x_memset( &s_ComputeState, 0, sizeof( s_ComputeState ) );

    return TRUE;
}

//==============================================================================

static
xbool sdlshader_ValidateDispatch( void )
{
    if( !sdleng_InComputePass() || !s_ComputeState.pShader || s_ComputeState.Dispatched )
    {
        return FALSE;
    }

    const shader_resource_counts& Resources = s_ComputeState.pShader->Resources;

    if( s_ComputeState.SamplerMask != ( ( 1u << Resources.SamplerCount ) - 1 ) ||
        s_ComputeState.TextureMask != ( ( 1u << Resources.StorageTextureCount ) - 1 ) ||
        s_ComputeState.BufferMask != ( ( 1u << Resources.StorageBufferCount ) - 1 ) ||
        s_ComputeState.UniformMask != ( ( 1u << Resources.UniformBufferCount ) - 1 ) )
    {
        x_DebugMsg( "SDLShader: compute dispatch has unbound resources\n" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
void sdlshader_RecordDispatch( void )
{
    s_ComputeState.Dispatched = TRUE;

    static xprofile_counter DispatchCountMetric =
        x_GetProfiler().RegisterCounter( "ComputeDispatchCalls", "Renderer" );

    DispatchCountMetric.Add();
}

//==============================================================================

xbool shader_Dispatch( u32 GroupCountX, u32 GroupCountY, u32 GroupCountZ )
{
    if( !GroupCountX || !GroupCountY || !GroupCountZ ||
        GroupCountX > 65535 || GroupCountY > 65535 || GroupCountZ > 65535 ||
        !sdlshader_ValidateDispatch() )
    {
        return FALSE;
    }

    SDL_DispatchGPUCompute( sdleng_GetComputePass(), GroupCountX, GroupCountY, GroupCountZ );
    sdlshader_RecordDispatch();

    return TRUE;
}

//==============================================================================

xbool shader_DispatchIndirect( const rbuffer& Buffer, u32 Offset )
{
    if( !Buffer.pBackend || !Buffer.pBackend->pBuffer ||
        !( Buffer.Desc.UsageFlags & RBUFFER_USAGE_INDIRECT ) || ( Offset & 3 ) ||
        Offset > Buffer.Desc.Size || sizeof( SDL_GPUIndirectDispatchCommand ) > Buffer.Desc.Size - Offset ||
        !sdlshader_ValidateDispatch() )
    {
        return FALSE;
    }

    if( !shader_ValidateResourceAccess( *rbuffer_GetResource( Buffer ), SHADER_RESOURCE_ACCESS_INDIRECT ) )
    {
        return FALSE;
    }

    for( u32 i = 0; i < s_ComputeState.pShader->Compute.ReadWriteBufferCount; ++i )
    {
        if( s_ComputeState.pBuffers[i] == Buffer.pBackend->pBuffer )
        {
            return FALSE;
        }
    }

    SDL_DispatchGPUComputeIndirect( sdleng_GetComputePass(), Buffer.pBackend->pBuffer, Offset );
    sdlshader_RecordDispatch();

    return TRUE;
}

//==============================================================================
//  UNIFORM DATA
//==============================================================================

void sdleng_ResetShaderUniforms( void )
{
    for( u32 Stage = 0; Stage < 3; ++Stage )
    {
        for( u32 Slot = 0; Slot < SDLENG_MAX_UNIFORM_BUFFER_SLOTS; ++Slot )
        {
            s_UniformData[Stage][Slot].SetCount( 0 );
        }
    }
}

//==============================================================================

xbool shader_PushUniformData( shader_stage Stage, u32 Slot, const void* pData, u32 Size )
{
    if( !sdlshader_IsValidStage( Stage ) ||
        ( Slot >= SDLENG_MAX_UNIFORM_BUFFER_SLOTS ) ||
        !pData || ( Size == 0 ) || ( Size > 0x7fffffffu ) )
    {
        return FALSE;
    }

    if( Stage == SHADER_STAGE_COMPUTE )
    {
        if( !sdleng_InComputePass() || !s_ComputeState.pShader || Size > 65536 ||
            Slot >= s_ComputeState.pShader->Resources.UniformBufferCount )
        {
            return FALSE;
        }

        s_ComputeState.UniformMask |= 1u << Slot;
    }

    SDL_GPUCommandBuffer* pCommandBuffer = NULL;

    if( !sdlshader_GetCommandBuffer( pCommandBuffer ) )
    {
        return FALSE;
    }

    xarray<byte>& Cached = s_UniformData[static_cast<u32>( Stage )][Slot];

    if( static_cast<u32>( Cached.GetCount() ) == Size &&
        x_memcmp( Cached.GetPtr(), pData, Size ) == 0 )
    {
        return TRUE;
    }

    Cached.SetCount( static_cast<s32>( Size ) );
    x_memcpy( Cached.GetPtr(), pData, Size );

    static xprofile_counter UniformPushCountMetric =
        x_GetProfiler().RegisterCounter( "UniformPushCalls", "Renderer" );

    UniformPushCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;

    switch( Stage )
    {
        case SHADER_STAGE_VERTEX:
        {
            SDL_PushGPUVertexUniformData( pCommandBuffer, Slot, pData, Size );
            break;
        }

        case SHADER_STAGE_PIXEL:
        {
            SDL_PushGPUFragmentUniformData( pCommandBuffer, Slot, pData, Size );
            break;
        }

        case SHADER_STAGE_COMPUTE:
        {
            SDL_PushGPUComputeUniformData( pCommandBuffer, Slot, pData, Size );
            break;
        }

        default:
        {
            return FALSE;
        }
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone UniformPushMetric =
            x_GetProfiler().RegisterZone( "UniformPushAPI", "RendererAPI" );

        UniformPushMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool shader_PushUniformData( const shader& Shader, shader_stage Stage, const char* pName, const void* pData, u32 Size )
{
    u32 Slot = 0;

    if( !shader_FindUniformSlot( Shader, pName, Slot ) )
    {
        return FALSE;
    }

    return shader_PushUniformData( Stage, Slot, pData, Size );
}

//==============================================================================

xbool shader_PushUniformData( const shader_uniform_binding& Binding )
{
    return shader_PushUniformData( Binding.Stage,
                                   Binding.Slot,
                                   Binding.pData,
                                   Binding.Size );
}

//==============================================================================
//  SAMPLER BINDING
//==============================================================================

static
xbool sdlshader_IsRenderAttachment( SDL_GPUTexture* pTexture )
{
    return sdleng_IsCurrentRenderAttachment( pTexture );
}

//==============================================================================

xbool shader_BindSampler( const shader_sampler_binding& Binding )
{
    if( !sdlshader_IsValidStage( Binding.Stage ) ||
        ( Binding.Slot >= SDLENG_MAX_SAMPLER_SLOTS ) )
    {
        return FALSE;
    }

    if( !Binding.pTexture || !shader_ValidateResourceAccess( *Binding.pTexture, SHADER_RESOURCE_ACCESS_SAMPLED ) )
    {
        return FALSE;
    }

    SDL_GPUTexture* pTexture = sdleng_GetGPUTexture( Binding.pTexture );
    SDL_GPUSampler* pSampler = sdleng_GetGPUSampler( Binding.pSampler );

    if( !pTexture || !pSampler )
    {
        return FALSE;
    }

    if( sdlshader_IsRenderAttachment( pTexture ) )
    {
        x_DebugMsg( "SDLShader: texture cannot be sampled while bound as a render attachment slot=%u stage=%d\n",
                    Binding.Slot,
                    static_cast<s32>( Binding.Stage ) );

        ASSERTS( FALSE, "Render attachment and shader input alias" );
        return FALSE;
    }

    if( Binding.Stage == SHADER_STAGE_COMPUTE )
    {
        if( !( Binding.pTexture->pBackend->UsageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER ) ||
            !sdlshader_ValidateComputeInput( SHADER_BINDING_SAMPLED_TEXTURE, Binding.Slot, pTexture, NULL ) )
        {
            return FALSE;
        }

        SDL_GPUTextureSamplerBinding SDLBinding = {};
        SDLBinding.texture                      = pTexture;
        SDLBinding.sampler                      = pSampler;
        SDL_BindGPUComputeSamplers( sdleng_GetComputePass(), Binding.Slot, &SDLBinding, 1 );
        s_ComputeState.SamplerMask |= 1u << Binding.Slot;
        return TRUE;
    }

    SDL_GPURenderPass* pRenderPass = NULL;

    if( !sdlshader_GetRenderPass( pRenderPass ) )
    {
        return FALSE;
    }

    const xbool bIssued = sdlshader_CacheSamplerBinding( Binding.Stage,
                                                         Binding.Slot,
                                                         pTexture,
                                                         pSampler );

    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );

    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );

    ResourceBindRequestMetric.Add();

    if( bIssued )
    {
        ResourceBindIssuedMetric.Add();
    }

    sdleng_RecordSamplerBindingDebug( Binding.Stage, Binding.Slot );

    if( !bIssued )
    {
        return TRUE;
    }

    SDL_GPUTextureSamplerBinding SDLBinding;
    x_memset( &SDLBinding, 0, sizeof( SDLBinding ) );
    SDLBinding.texture = pTexture;
    SDLBinding.sampler = pSampler;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;

    if( Binding.Stage == SHADER_STAGE_VERTEX )
    {
        SDL_BindGPUVertexSamplers( pRenderPass, Binding.Slot, &SDLBinding, 1 );
    }
    else
    {
        SDL_BindGPUFragmentSamplers( pRenderPass, Binding.Slot, &SDLBinding, 1 );
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );

        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool shader_BindSampler( const shader&          Shader,
                          shader_stage           Stage,
                          const char*            pName,
                          const shader_resource* pTexture,
                          const rstate_sampler*  pSampler )
{
    u32 Slot = 0;

    if( !shader_FindSampledTextureSlot( Shader, pName, Slot ) )
    {
        return FALSE;
    }

    shader_sampler_binding Binding( Stage, Slot, pTexture, pSampler );

    return shader_BindSampler( Binding );
}

//==============================================================================

xbool shader_BindSamplers( const shader_sampler_binding* pBindings, u32 Count )
{
    if( Count == 0 )
    {
        return TRUE;
    }

    if( !pBindings )
    {
        return FALSE;
    }

    if( pBindings[0].Stage == SHADER_STAGE_COMPUTE )
    {
        for( u32 i = 0; i < Count; ++i )
        {
            if( pBindings[i].Stage != SHADER_STAGE_COMPUTE || !shader_BindSampler( pBindings[i] ) )
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    u32   MaxSlots[2] = { 0, 0 };
    xbool HasStage[2] = { FALSE, FALSE };

    for( u32 i = 0; i < Count; i++ )
    {
        const shader_sampler_binding& Binding = pBindings[i];

        if( !sdlshader_IsValidStage( Binding.Stage ) ||
            ( Binding.Slot >= SDLENG_MAX_SAMPLER_SLOTS ) )
        {
            return FALSE;
        }

        if( Binding.Stage == SHADER_STAGE_COMPUTE )
        {
            return FALSE;
        }

        if( !Binding.pTexture || !shader_ValidateResourceAccess( *Binding.pTexture, SHADER_RESOURCE_ACCESS_SAMPLED ) )
        {
            return FALSE;
        }

        SDL_GPUTexture* pTexture = sdleng_GetGPUTexture( Binding.pTexture );
        SDL_GPUSampler* pSampler = sdleng_GetGPUSampler( Binding.pSampler );

        if( !pTexture || !pSampler )
        {
            return FALSE;
        }

        if( sdlshader_IsRenderAttachment( pTexture ) )
        {
            x_DebugMsg( "SDLShader: texture cannot be sampled while bound as a render attachment slot=%u stage=%d\n",
                        Binding.Slot,
                        static_cast<s32>( Binding.Stage ) );

            ASSERTS( FALSE, "Render attachment and shader input alias" );
            return FALSE;
        }

        const u32 StageIndex = sdlshader_StageCacheIndex( Binding.Stage );
        HasStage[StageIndex] = TRUE;

        if( Binding.Slot > MaxSlots[StageIndex] )
        {
            MaxSlots[StageIndex] = Binding.Slot;
        }
    }

    SDL_GPURenderPass* pRenderPass = NULL;

    if( !sdlshader_GetRenderPass( pRenderPass ) )
    {
        return FALSE;
    }

    for( u32 StageIndex = 0; StageIndex < 2; ++StageIndex )
    {
        if( HasStage[StageIndex] )
        {
            sdlshader_PrepareBatch( s_SamplerBatchUsed[StageIndex],
                                    MaxSlots[StageIndex] + 1 );
        }
    }

    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );

    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );

    for( u32 i = 0; i < Count; i++ )
    {
        const shader_sampler_binding& Binding    = pBindings[i];
        SDL_GPUTexture*               pTexture   = sdleng_GetGPUTexture( Binding.pTexture );
        SDL_GPUSampler*               pSampler   = sdleng_GetGPUSampler( Binding.pSampler );
        const u32                     StageIndex = sdlshader_StageCacheIndex( Binding.Stage );
        const xbool                   bIssued    = sdlshader_CacheSamplerBinding( Binding.Stage,
                                                             Binding.Slot,
                                                             pTexture,
                                                             pSampler );

        ResourceBindRequestMetric.Add();

        if( bIssued )
        {
            ResourceBindIssuedMetric.Add();
        }

        sdleng_RecordSamplerBindingDebug( Binding.Stage, Binding.Slot );

        if( bIssued )
        {
            s_SamplerBatchBindings[StageIndex][Binding.Slot].texture = pTexture;
            s_SamplerBatchBindings[StageIndex][Binding.Slot].sampler = pSampler;
            s_SamplerBatchUsed[StageIndex][Binding.Slot]             = TRUE;
        }
    }

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;

    for( u32 StageIndex = 0; StageIndex < 2; ++StageIndex )
    {
        if( HasStage[StageIndex] )
        {
            sdlshader_SubmitSamplerBatch( pRenderPass,
                                          StageIndex == 0 ? SHADER_STAGE_VERTEX : SHADER_STAGE_PIXEL,
                                          s_SamplerBatchBindings[StageIndex],
                                          s_SamplerBatchUsed[StageIndex],
                                          MaxSlots[StageIndex] + 1 );
        }
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );

        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================
//  STORAGE TEXTURE BINDING
//==============================================================================

xbool shader_BindStorageTexture( const shader_storage_texture_binding& Binding )
{
    if( !sdlshader_IsValidStage( Binding.Stage ) ||
        ( Binding.Slot >= SDLENG_MAX_STORAGE_TEXTURE_SLOTS ) )
    {
        return FALSE;
    }

    if( !Binding.pTexture || !shader_ValidateResourceAccess( *Binding.pTexture, SHADER_RESOURCE_ACCESS_STORAGE ) )
    {
        return FALSE;
    }

    SDL_GPUTexture* pTexture = sdleng_GetGPUTexture( Binding.pTexture );

    if( !pTexture )
    {
        return FALSE;
    }

    if( sdlshader_IsRenderAttachment( pTexture ) )
    {
        x_DebugMsg( "SDLShader: texture cannot be storage-bound while used as a render attachment slot=%u stage=%d\n",
                    Binding.Slot,
                    static_cast<s32>( Binding.Stage ) );

        ASSERTS( FALSE, "Render attachment and storage input alias" );
        return FALSE;
    }

    if( Binding.Stage == SHADER_STAGE_COMPUTE )
    {
        if( !( Binding.pTexture->pBackend->UsageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ ) ||
            !sdlshader_ValidateComputeInput( SHADER_BINDING_STORAGE_TEXTURE, Binding.Slot, pTexture, NULL ) )
        {
            return FALSE;
        }

        SDL_BindGPUComputeStorageTextures( sdleng_GetComputePass(), Binding.Slot, &pTexture, 1 );
        s_ComputeState.TextureMask |= 1u << Binding.Slot;
        return TRUE;
    }

    SDL_GPURenderPass* pRenderPass = NULL;

    if( !sdlshader_GetRenderPass( pRenderPass ) )
    {
        return FALSE;
    }

    const xbool bIssued = sdlshader_CacheStorageTextureBinding( Binding.Stage,
                                                                Binding.Slot,
                                                                pTexture );

    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );

    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );

    ResourceBindRequestMetric.Add();

    if( bIssued )
    {
        ResourceBindIssuedMetric.Add();
    }

    sdleng_RecordStorageTextureBindingDebug( Binding.Stage, Binding.Slot );

    if( !bIssued )
    {
        return TRUE;
    }

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;

    if( Binding.Stage == SHADER_STAGE_VERTEX )
    {
        SDL_BindGPUVertexStorageTextures( pRenderPass, Binding.Slot, &pTexture, 1 );
    }
    else
    {
        SDL_BindGPUFragmentStorageTextures( pRenderPass, Binding.Slot, &pTexture, 1 );
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );

        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool shader_BindStorageTexture( const shader&          Shader,
                                 shader_stage           Stage,
                                 const char*            pName,
                                 const shader_resource* pTexture )
{
    u32 Slot = 0;

    if( !shader_FindStorageTextureSlot( Shader, pName, Slot ) )
    {
        return FALSE;
    }

    shader_storage_texture_binding Binding( Stage, Slot, pTexture );

    return shader_BindStorageTexture( Binding );
}

//==============================================================================

xbool shader_BindStorageTextures( const shader_storage_texture_binding* pBindings, u32 Count )
{
    if( Count == 0 )
    {
        return TRUE;
    }

    if( !pBindings )
    {
        return FALSE;
    }

    if( pBindings[0].Stage == SHADER_STAGE_COMPUTE )
    {
        for( u32 i = 0; i < Count; ++i )
        {
            if( pBindings[i].Stage != SHADER_STAGE_COMPUTE || !shader_BindStorageTexture( pBindings[i] ) )
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    u32   MaxSlots[2] = { 0, 0 };
    xbool HasStage[2] = { FALSE, FALSE };

    for( u32 i = 0; i < Count; i++ )
    {
        const shader_storage_texture_binding& Binding = pBindings[i];

        if( !sdlshader_IsValidStage( Binding.Stage ) ||
            ( Binding.Slot >= SDLENG_MAX_STORAGE_TEXTURE_SLOTS ) )
        {
            return FALSE;
        }

        if( Binding.Stage == SHADER_STAGE_COMPUTE )
        {
            return FALSE;
        }

        if( !Binding.pTexture || !shader_ValidateResourceAccess( *Binding.pTexture, SHADER_RESOURCE_ACCESS_STORAGE ) )
        {
            return FALSE;
        }

        SDL_GPUTexture* pTexture = sdleng_GetGPUTexture( Binding.pTexture );

        if( !pTexture )
        {
            return FALSE;
        }

        if( sdlshader_IsRenderAttachment( pTexture ) )
        {
            x_DebugMsg( "SDLShader: texture cannot be storage-bound while used as a render attachment slot=%u stage=%d\n",
                        Binding.Slot,
                        static_cast<s32>( Binding.Stage ) );

            ASSERTS( FALSE, "Render attachment and storage input alias" );
            return FALSE;
        }

        const u32 StageIndex = sdlshader_StageCacheIndex( Binding.Stage );
        HasStage[StageIndex] = TRUE;

        if( Binding.Slot > MaxSlots[StageIndex] )
        {
            MaxSlots[StageIndex] = Binding.Slot;
        }
    }

    SDL_GPURenderPass* pRenderPass = NULL;

    if( !sdlshader_GetRenderPass( pRenderPass ) )
    {
        return FALSE;
    }

    for( u32 StageIndex = 0; StageIndex < 2; ++StageIndex )
    {
        if( HasStage[StageIndex] )
        {
            sdlshader_PrepareBatch( s_StorageTextureBatchUsed[StageIndex],
                                    MaxSlots[StageIndex] + 1 );
        }
    }

    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );

    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );

    for( u32 i = 0; i < Count; i++ )
    {
        const shader_storage_texture_binding& Binding    = pBindings[i];
        SDL_GPUTexture*                       pTexture   = sdleng_GetGPUTexture( Binding.pTexture );
        const u32                             StageIndex = sdlshader_StageCacheIndex( Binding.Stage );
        const xbool                           bIssued    = sdlshader_CacheStorageTextureBinding( Binding.Stage,
                                                                    Binding.Slot,
                                                                    pTexture );

        ResourceBindRequestMetric.Add();

        if( bIssued )
        {
            ResourceBindIssuedMetric.Add();
        }

        sdleng_RecordStorageTextureBindingDebug( Binding.Stage, Binding.Slot );

        if( bIssued )
        {
            s_StorageTextureBatchBindings[StageIndex][Binding.Slot] = pTexture;
            s_StorageTextureBatchUsed[StageIndex][Binding.Slot]     = TRUE;
        }
    }

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;

    for( u32 StageIndex = 0; StageIndex < 2; ++StageIndex )
    {
        if( HasStage[StageIndex] )
        {
            sdlshader_SubmitStorageTextureBatch( pRenderPass,
                                                 StageIndex == 0 ? SHADER_STAGE_VERTEX : SHADER_STAGE_PIXEL,
                                                 s_StorageTextureBatchBindings[StageIndex],
                                                 s_StorageTextureBatchUsed[StageIndex],
                                                 MaxSlots[StageIndex] + 1 );
        }
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );

        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================
//  STORAGE BUFFER BINDING
//==============================================================================

xbool shader_BindStorageBuffer( const shader_storage_buffer_binding& Binding )
{
    if( !sdlshader_IsValidStage( Binding.Stage ) ||
        ( Binding.Slot >= SDLENG_MAX_STORAGE_BUFFER_SLOTS ) )
    {
        return FALSE;
    }

    if( !Binding.pBuffer || !shader_ValidateResourceAccess( *Binding.pBuffer, SHADER_RESOURCE_ACCESS_STORAGE ) )
    {
        return FALSE;
    }

    SDL_GPUBuffer* pBuffer = sdleng_GetGPUBuffer( Binding.pBuffer );

    if( !pBuffer )
    {
        return FALSE;
    }

    if( Binding.Stage == SHADER_STAGE_COMPUTE )
    {
        if( !( Binding.pBuffer->pBackend->UsageFlags & SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ ) ||
            !sdlshader_ValidateComputeInput( SHADER_BINDING_STORAGE_BUFFER, Binding.Slot, NULL, pBuffer ) )
        {
            return FALSE;
        }

        SDL_BindGPUComputeStorageBuffers( sdleng_GetComputePass(), Binding.Slot, &pBuffer, 1 );
        s_ComputeState.BufferMask |= 1u << Binding.Slot;
        return TRUE;
    }

    if( !( Binding.pBuffer->pBackend->UsageFlags & SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ ) )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = NULL;

    if( !sdlshader_GetRenderPass( pRenderPass ) )
    {
        return FALSE;
    }

    const xbool bIssued = sdlshader_CacheStorageBufferBinding( Binding.Stage,
                                                               Binding.Slot,
                                                               pBuffer );

    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );

    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );

    ResourceBindRequestMetric.Add();

    if( bIssued )
    {
        ResourceBindIssuedMetric.Add();
    }

    sdleng_RecordStorageBufferBindingDebug( Binding.Stage, Binding.Slot );

    if( !bIssued )
    {
        return TRUE;
    }

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;

    if( Binding.Stage == SHADER_STAGE_VERTEX )
    {
        SDL_BindGPUVertexStorageBuffers( pRenderPass, Binding.Slot, &pBuffer, 1 );
    }
    else
    {
        SDL_BindGPUFragmentStorageBuffers( pRenderPass, Binding.Slot, &pBuffer, 1 );
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );

        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool shader_BindStorageBuffer( const shader&          Shader,
                                shader_stage           Stage,
                                const char*            pName,
                                const shader_resource* pBuffer )
{
    u32 Slot = 0;

    if( !shader_FindStorageBufferSlot( Shader, pName, Slot ) )
    {
        return FALSE;
    }

    shader_storage_buffer_binding Binding( Stage, Slot, pBuffer );

    return shader_BindStorageBuffer( Binding );
}

//==============================================================================

xbool shader_BindStorageBuffers( const shader_storage_buffer_binding* pBindings, u32 Count )
{
    if( Count == 0 )
    {
        return TRUE;
    }

    if( !pBindings )
    {
        return FALSE;
    }

    if( pBindings[0].Stage == SHADER_STAGE_COMPUTE )
    {
        for( u32 i = 0; i < Count; ++i )
        {
            if( pBindings[i].Stage != SHADER_STAGE_COMPUTE || !shader_BindStorageBuffer( pBindings[i] ) )
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    u32   MaxSlots[2] = { 0, 0 };
    xbool HasStage[2] = { FALSE, FALSE };

    for( u32 i = 0; i < Count; i++ )
    {
        const shader_storage_buffer_binding& Binding = pBindings[i];

        if( !sdlshader_IsValidStage( Binding.Stage ) ||
            ( Binding.Slot >= SDLENG_MAX_STORAGE_BUFFER_SLOTS ) )
        {
            return FALSE;
        }

        if( Binding.Stage == SHADER_STAGE_COMPUTE )
        {
            return FALSE;
        }

        if( !Binding.pBuffer || !shader_ValidateResourceAccess( *Binding.pBuffer, SHADER_RESOURCE_ACCESS_STORAGE ) )
        {
            return FALSE;
        }

        SDL_GPUBuffer* pBuffer = sdleng_GetGPUBuffer( Binding.pBuffer );

        if( !pBuffer || !( Binding.pBuffer->pBackend->UsageFlags & SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ ) )
        {
            return FALSE;
        }

        const u32 StageIndex = sdlshader_StageCacheIndex( Binding.Stage );
        HasStage[StageIndex] = TRUE;

        if( Binding.Slot > MaxSlots[StageIndex] )
        {
            MaxSlots[StageIndex] = Binding.Slot;
        }
    }

    SDL_GPURenderPass* pRenderPass = NULL;

    if( !sdlshader_GetRenderPass( pRenderPass ) )
    {
        return FALSE;
    }

    for( u32 StageIndex = 0; StageIndex < 2; ++StageIndex )
    {
        if( HasStage[StageIndex] )
        {
            sdlshader_PrepareBatch( s_StorageBufferBatchUsed[StageIndex],
                                    MaxSlots[StageIndex] + 1 );
        }
    }

    static xprofile_counter ResourceBindRequestMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindRequests", "Renderer" );

    static xprofile_counter ResourceBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "ResourceBindIssued", "Renderer" );

    for( u32 i = 0; i < Count; i++ )
    {
        const shader_storage_buffer_binding& Binding    = pBindings[i];
        SDL_GPUBuffer*                       pBuffer    = sdleng_GetGPUBuffer( Binding.pBuffer );
        const u32                            StageIndex = sdlshader_StageCacheIndex( Binding.Stage );
        const xbool                          bIssued    = sdlshader_CacheStorageBufferBinding( Binding.Stage,
                                                                   Binding.Slot,
                                                                   pBuffer );

        ResourceBindRequestMetric.Add();

        if( bIssued )
        {
            ResourceBindIssuedMetric.Add();
        }

        sdleng_RecordStorageBufferBindingDebug( Binding.Stage, Binding.Slot );

        if( bIssued )
        {
            s_StorageBufferBatchBindings[StageIndex][Binding.Slot] = pBuffer;
            s_StorageBufferBatchUsed[StageIndex][Binding.Slot]     = TRUE;
        }
    }

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;

    for( u32 StageIndex = 0; StageIndex < 2; ++StageIndex )
    {
        if( HasStage[StageIndex] )
        {
            sdlshader_SubmitStorageBufferBatch( pRenderPass,
                                                StageIndex == 0 ? SHADER_STAGE_VERTEX : SHADER_STAGE_PIXEL,
                                                s_StorageBufferBatchBindings[StageIndex],
                                                s_StorageBufferBatchUsed[StageIndex],
                                                MaxSlots[StageIndex] + 1 );
        }
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone ResourceBindMetric =
            x_GetProfiler().RegisterZone( "ResourceBindAPI", "RendererAPI" );

        ResourceBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================
//  BINDING SET
//==============================================================================

const rbuffer* shader_GetResourceBuffer( const shader_resource& Resource )
{
    return Resource.pBackend ? Resource.pBackend->pBufferOwner : NULL;
}

//==============================================================================

const rtarget* shader_GetResourceTarget( const shader_resource& Resource )
{
    return Resource.pBackend ? Resource.pBackend->pTargetOwner : NULL;
}

//==============================================================================

const vram_texture* shader_GetResourceTexture( const shader_resource& Resource )
{
    return Resource.pBackend ? Resource.pBackend->pTextureOwner : NULL;
}

//==============================================================================

void shader_SetResourceAccessValidator( shader_resource_access_validator Validator, void* pContext )
{
    s_ResourceAccessValidator = Validator;
    s_pResourceAccessContext  = pContext;
}

//==============================================================================

xbool shader_ValidateResourceAccess( const shader_resource& Resource, shader_resource_access Access )
{
    if( Access == SHADER_RESOURCE_ACCESS_SAMPLED &&
        ( !Resource.pBackend || Resource.pBackend->Kind != SDLENG_SHADER_RESOURCE_TEXTURE ||
          !( Resource.pBackend->UsageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER ) ) )
    {
        return FALSE;
    }

    return !s_ResourceAccessValidator ||
           s_ResourceAccessValidator( Resource, Access, s_pResourceAccessContext );
}

//==============================================================================

xbool shader_BindSet( const shader_binding_set_desc& Desc )
{
    for( u32 i = 0; i < Desc.UniformCount; i++ )
    {
        if( !Desc.pUniforms || !shader_PushUniformData( Desc.pUniforms[i] ) )
        {
            return FALSE;
        }
    }

    if( !shader_BindSamplers( Desc.pSamplers, Desc.SamplerCount ) )
    {
        return FALSE;
    }

    if( !shader_BindStorageTextures( Desc.pStorageTextures, Desc.StorageTextureCount ) )
    {
        return FALSE;
    }

    if( !shader_BindStorageBuffers( Desc.pStorageBuffers, Desc.StorageBufferCount ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  BINDING REFLECTION LOOKUP
//==============================================================================

xbool shader_FindBindingSlot( const shader&       Shader,
                              shader_binding_kind Kind,
                              const char*         pName,
                              u32&                Slot )
{
    Slot = 0;

    if( !Shader.pBackend || !pName || !pName[0] )
    {
        return FALSE;
    }

    static xprofile_counter BindingLookupMetric =
        x_GetProfiler().RegisterCounter( "ShaderBindingLookups", "Renderer" );

    BindingLookupMetric.Add();

    const shader_backend& Backend     = *Shader.pBackend;
    const u32             LookupCount = Backend.BindingLookupCount;

    if( LookupCount == 0 )
    {
        return FALSE;
    }

    u32 LookupIndex = sdlshader_HashBinding( Kind, pName ) & Backend.BindingLookupMask;

    for( u32 Probe = 0; Probe < LookupCount; ++Probe )
    {
        const s32 BindingIndex = Backend.BindingLookup[LookupIndex];

        if( BindingIndex < 0 )
        {
            return FALSE;
        }

        const shader_binding_record& Binding = Backend.Bindings[BindingIndex];

        if( ( Binding.Kind == Kind ) && ( x_strcmp( Binding.Name, pName ) == 0 ) )
        {
            Slot = Binding.Slot;
            return TRUE;
        }

        LookupIndex = ( LookupIndex + 1 ) & Backend.BindingLookupMask;
    }

    return FALSE;
}

//==============================================================================

xbool shader_FindSampledTextureSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_SAMPLED_TEXTURE, pName, Slot );
}

//==============================================================================

xbool shader_FindUniformSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_UNIFORM_BUFFER, pName, Slot );
}

//==============================================================================

xbool shader_FindStorageTextureSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_STORAGE_TEXTURE, pName, Slot );
}

//==============================================================================

xbool shader_FindStorageBufferSlot( const shader& Shader, const char* pName, u32& Slot )
{
    return shader_FindBindingSlot( Shader, SHADER_BINDING_STORAGE_BUFFER, pName, Slot );
}

//==============================================================================
//  SDL BACKEND ACCESSORS
//==============================================================================

u32 sdleng_GetNativeShaderCount( void )
{
    SDL_LockSpinlock( &s_NativeStageLock );
    u32 Count = 0;

    for( s32 Bucket = 0; Bucket < SDLENG_NATIVE_STAGE_BUCKETS; ++Bucket )
    {
        for( s32 i = 0; i < s_NativeStages[Bucket].GetCount(); ++i )
        {
            ++Count;
        }
    }

    SDL_UnlockSpinlock( &s_NativeStageLock );

    return Count;
}

//==============================================================================

ecs_block_cache_stats sdleng_GetShaderPackageCacheStats( void )
{
    return s_ShaderPackages.GetStats();
}

//==============================================================================

SDL_GPUSampler* sdleng_GetGPUSampler( const rstate_sampler* pSampler )
{
    if( !pSampler || !pSampler->pBackend || ( pSampler->pBackend->pOwner != pSampler ) )
    {
        return NULL;
    }

    return pSampler->pBackend->pSampler;
}

//==============================================================================

SDL_GPUShader* sdleng_GetGPUShader( const shader* pShader )
{
    if( !pShader || !pShader->pBackend || ( pShader->pBackend->pOwner != pShader ) )
    {
        return NULL;
    }

    return pShader->pBackend->pShader;
}

//==============================================================================

const shader_resource_counts* sdleng_GetShaderResourceCounts( const shader* pShader )
{
    if( !pShader || !pShader->pBackend )
    {
        return NULL;
    }

    return &pShader->pBackend->Resources;
}

//==============================================================================

u32 sdleng_GetShaderCount( void )
{
    return s_ShaderCount;
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
