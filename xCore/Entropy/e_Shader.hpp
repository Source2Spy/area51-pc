//==============================================================================
//
//  e_Shader.hpp
//
//  Shader declarations for explicit PSO render backends.
//
//==============================================================================

#ifndef E_SHADER_HPP
#define E_SHADER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

//==============================================================================
//  ENUMS
//==============================================================================

enum shader_stage
{
    SHADER_STAGE_VERTEX = 0,
    SHADER_STAGE_PIXEL,
    SHADER_STAGE_COMPUTE,
    SHADER_STAGE_COUNT
};

//------------------------------------------------------------------------------

enum shader_format
{
    SHADER_FORMAT_UNKNOWN = 0,
    SHADER_FORMAT_SPIRV,
    SHADER_FORMAT_DXIL,
    SHADER_FORMAT_MSL,
    SHADER_FORMAT_METALLIB,
    SHADER_FORMAT_PRIVATE
};

//------------------------------------------------------------------------------

enum shader_topology
{
    SHADER_TOPOLOGY_UNDEFINED = 0,
    SHADER_TOPOLOGY_POINT_LIST,
    SHADER_TOPOLOGY_LINE_LIST,
    SHADER_TOPOLOGY_LINE_STRIP,
    SHADER_TOPOLOGY_TRIANGLE_LIST,
    SHADER_TOPOLOGY_TRIANGLE_STRIP
};

//------------------------------------------------------------------------------

enum shader_vertex_format
{
    SHADER_VERTEX_FORMAT_FLOAT1 = 0,
    SHADER_VERTEX_FORMAT_FLOAT2,
    SHADER_VERTEX_FORMAT_FLOAT3,
    SHADER_VERTEX_FORMAT_FLOAT4,
    SHADER_VERTEX_FORMAT_UBYTE4N_BGRA,
    SHADER_VERTEX_FORMAT_UINT1,
    SHADER_VERTEX_FORMAT_COUNT
};

//==============================================================================
//  STRUCTURES
//==============================================================================

struct shader_backend;
struct shader_resource_backend;
struct rstate_sampler;

//------------------------------------------------------------------------------

struct shader_blob
{
    const void* pData;
    u32         Size;

    shader_blob( void ) :
        pData( NULL ),
        Size ( 0 )
    {
    }

    shader_blob( const void* pBlobData, u32 BlobSize ) :
        pData( pBlobData ),
        Size ( BlobSize )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_resource_counts
{
    u32 SamplerCount;
    u32 StorageTextureCount;
    u32 StorageBufferCount;
    u32 UniformBufferCount;

    shader_resource_counts( void ) :
        SamplerCount       ( 0 ),
        StorageTextureCount( 0 ),
        StorageBufferCount ( 0 ),
        UniformBufferCount ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

enum shader_binding_kind
{
    SHADER_BINDING_SAMPLED_TEXTURE = 0,
    SHADER_BINDING_UNIFORM_BUFFER,
    SHADER_BINDING_STORAGE_TEXTURE,
    SHADER_BINDING_STORAGE_BUFFER,
    SHADER_BINDING_READ_WRITE_TEXTURE,
    SHADER_BINDING_READ_WRITE_BUFFER
};

//------------------------------------------------------------------------------

struct shader_binding_desc
{
    shader_binding_kind Kind;
    const char*         pName;
    u32                 Slot;
    u32                 Count;

    shader_binding_desc( void ) :
        Kind ( SHADER_BINDING_SAMPLED_TEXTURE ),
        pName( NULL ),
        Slot ( 0 ),
        Count( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct ecs_package_layout;

struct shader_compute_desc
{
    u32 ReadWriteTextureCount;
    u32 ReadWriteBufferCount;
    u32 ThreadCountX;
    u32 ThreadCountY;
    u32 ThreadCountZ;

    shader_compute_desc( void ) :
        ReadWriteTextureCount( 0 ),
        ReadWriteBufferCount ( 0 ),
        ThreadCountX         ( 0 ),
        ThreadCountY         ( 0 ),
        ThreadCountZ         ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_desc
{
    const ecs_package_layout* pInterface;
    shader_stage           Stage;
    shader_format          Format;
    shader_blob            Blob;
    const char*            pEntryPoint;
    shader_resource_counts Resources;
    shader_compute_desc    Compute;
    const shader_binding_desc*
                           pBindings;
    u32                    BindingCount;
    const char*            pDebugName;

    shader_desc( void ) :
        pInterface ( NULL ),
        Stage      ( SHADER_STAGE_VERTEX ),
        Format     ( SHADER_FORMAT_UNKNOWN ),
        Blob       (),
        pEntryPoint( NULL ),
        Resources  (),
        Compute    (),
        pBindings  ( NULL ),
        BindingCount( 0 ),
        pDebugName ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_file_desc
{
    const char*            pFileName;
    shader_stage           Stage;
    shader_format          Format;
    const char*            pEntryPoint;
    shader_resource_counts Resources;
    shader_compute_desc    Compute;
    const shader_binding_desc*
                           pBindings;
    u32                    BindingCount;
    const char*            pDebugName;

    shader_file_desc( void ) :
        pFileName  ( NULL ),
        Stage      ( SHADER_STAGE_VERTEX ),
        Format     ( SHADER_FORMAT_UNKNOWN ),
        pEntryPoint( NULL ),
        Resources  (),
        Compute    (),
        pBindings  ( NULL ),
        BindingCount( 0 ),
        pDebugName ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

enum shader_combo_kind
{
    SHADER_COMBO_KEYED = 0,
    SHADER_COMBO_FIXED = 1
};

struct shader_program_desc
{
    u64 ExpectedCatalogId = 0;
    u64 ProgramId = 0;
    const char* pFileName;
    const char* pDebugName;

    shader_program_desc( void ) :
        pFileName ( NULL ),
        pDebugName( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_program_load;

enum shader_program_load_status
{
    SHADER_PROGRAM_PENDING,
    SHADER_PROGRAM_READY,
    SHADER_PROGRAM_FAILED,
    SHADER_PROGRAM_CANCELLED
};

struct shader
{
    shader_stage    Stage;
    shader_format   Format;
    shader_backend* pBackend;
    shader_program_load* pLoad;
    u64 ProgramId;
    u32 ResourceSlots[128];

    shader( void ) :
        Stage   ( SHADER_STAGE_VERTEX ),
        Format  ( SHADER_FORMAT_UNKNOWN ),
        pBackend( NULL ),
        pLoad   ( NULL ),
        ProgramId( 0 )
    {
        for( u32& Slot : ResourceSlots ) Slot = 0xFFFFFFFFu;
    }

    operator xbool( void ) const { return pBackend != NULL; }
};

//==============================================================================
//  PIPELINE SHADER STATE
//==============================================================================

//------------------------------------------------------------------------------

inline xbool shader_GetResourceSlot( shader const& Shader, u32 Semantic, u32& Slot )
{
    Slot = Semantic < 128 ? Shader.ResourceSlots[Semantic] : 0xFFFFFFFFu;
    return Slot != 0xFFFFFFFFu;
}

struct shader_vertex_buffer_desc
{
    u32 Slot;
    u32 Stride;
    u32 InstanceStepRate;
    xbool bPerInstance;

    shader_vertex_buffer_desc( void ) :
        Slot            ( 0 ),
        Stride          ( 0 ),
        InstanceStepRate( 0 ),
        bPerInstance    ( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_vertex_element
{
    u32                  Location;
    u32                  InputSlot;
    shader_vertex_format Format;
    u32                  AlignedByteOffset;

    shader_vertex_element( void ) :
        Location         ( 0 ),
        InputSlot        ( 0 ),
        Format           ( SHADER_VERTEX_FORMAT_FLOAT3 ),
        AlignedByteOffset( 0 )
    {
    }

    shader_vertex_element( u32                  ShaderLocation,
                           u32                  Slot,
                           shader_vertex_format VertexFormat,
                           u32                  Offset ) :
        Location         ( ShaderLocation ),
        InputSlot        ( Slot ),
        Format           ( VertexFormat ),
        AlignedByteOffset( Offset )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_pipeline_desc
{
    const shader*                    pVertexShader;
    const shader*                    pPixelShader;
    const shader_vertex_buffer_desc* pVertexBuffers;
    u32                              VertexBufferCount;
    const shader_vertex_element*     pInputElements;
    u32                              InputElementCount;
    shader_topology                  Topology;

    shader_pipeline_desc( void ) :
        pVertexShader    ( NULL ),
        pPixelShader     ( NULL ),
        pVertexBuffers   ( NULL ),
        VertexBufferCount( 0 ),
        pInputElements   ( NULL ),
        InputElementCount( 0 ),
        Topology         ( SHADER_TOPOLOGY_UNDEFINED )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_resource
{
    shader_resource_backend* pBackend;

    shader_resource( void ) : pBackend( NULL ) {}
    operator xbool( void ) const { return pBackend != NULL; }
};

//------------------------------------------------------------------------------

struct rbuffer;
struct rtarget;
struct vram_texture;

//------------------------------------------------------------------------------

enum shader_resource_access
{
    SHADER_RESOURCE_ACCESS_SAMPLED,
    SHADER_RESOURCE_ACCESS_STORAGE,
    SHADER_RESOURCE_ACCESS_VERTEX,
    SHADER_RESOURCE_ACCESS_INDEX,
    SHADER_RESOURCE_ACCESS_INDIRECT,
    SHADER_RESOURCE_ACCESS_UPLOAD,
};

//------------------------------------------------------------------------------

typedef xbool ( *shader_resource_access_validator )( const shader_resource& Resource,
                                                     shader_resource_access Access, void* pContext );

const rbuffer*      shader_GetResourceBuffer( const shader_resource& Resource );
const rtarget*      shader_GetResourceTarget( const shader_resource& Resource );
const vram_texture* shader_GetResourceTexture( const shader_resource& Resource );
void                shader_SetResourceAccessValidator( shader_resource_access_validator Validator, void* pContext );
xbool               shader_ValidateResourceAccess( const shader_resource& Resource, shader_resource_access Access );

//==============================================================================
//  RESOURCE BINDING
//==============================================================================

struct shader_uniform_binding
{
    shader_stage Stage;
    u32          Slot;
    const void*  pData;
    u32          Size;

    shader_uniform_binding( void ) :
        Stage( SHADER_STAGE_VERTEX ),
        Slot ( 0 ),
        pData( NULL ),
        Size ( 0 )
    {
    }

    shader_uniform_binding( shader_stage BindStage,
                            u32          BindSlot,
                            const void*  pUniformData,
                            u32          UniformSize ) :
        Stage( BindStage ),
        Slot ( BindSlot ),
        pData( pUniformData ),
        Size ( UniformSize )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_sampler_binding
{
    shader_stage            Stage;
    u32                     Slot;
    const shader_resource*  pTexture;
    const rstate_sampler*   pSampler;

    shader_sampler_binding( void ) :
        Stage   ( SHADER_STAGE_PIXEL ),
        Slot    ( 0 ),
        pTexture( NULL ),
        pSampler( NULL )
    {
    }

    shader_sampler_binding( shader_stage           BindStage,
                            u32                    BindSlot,
                            const shader_resource* pBindTexture,
                            const rstate_sampler*  pBindSampler ) :
        Stage   ( BindStage ),
        Slot    ( BindSlot ),
        pTexture( pBindTexture ),
        pSampler( pBindSampler )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_storage_texture_binding
{
    shader_stage           Stage;
    u32                    Slot;
    const shader_resource* pTexture;

    shader_storage_texture_binding( void ) :
        Stage   ( SHADER_STAGE_PIXEL ),
        Slot    ( 0 ),
        pTexture( NULL )
    {
    }

    shader_storage_texture_binding( shader_stage           BindStage,
                                    u32                    BindSlot,
                                    const shader_resource* pBindTexture ) :
        Stage   ( BindStage ),
        Slot    ( BindSlot ),
        pTexture( pBindTexture )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_storage_buffer_binding
{
    shader_stage           Stage;
    u32                    Slot;
    const shader_resource* pBuffer;

    shader_storage_buffer_binding( void ) :
        Stage  ( SHADER_STAGE_PIXEL ),
        Slot   ( 0 ),
        pBuffer( NULL )
    {
    }

    shader_storage_buffer_binding( shader_stage           BindStage,
                                   u32                    BindSlot,
                                   const shader_resource* pBindBuffer ) :
        Stage  ( BindStage ),
        Slot   ( BindSlot ),
        pBuffer( pBindBuffer )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_binding_set_desc
{
    const shader_uniform_binding*         pUniforms;
    u32                                   UniformCount;
    const shader_sampler_binding*         pSamplers;
    u32                                   SamplerCount;
    const shader_storage_texture_binding* pStorageTextures;
    u32                                   StorageTextureCount;
    const shader_storage_buffer_binding*  pStorageBuffers;
    u32                                   StorageBufferCount;

    shader_binding_set_desc( void ) :
        pUniforms          ( NULL ),
        UniformCount       ( 0 ),
        pSamplers          ( NULL ),
        SamplerCount       ( 0 ),
        pStorageTextures   ( NULL ),
        StorageTextureCount( 0 ),
        pStorageBuffers    ( NULL ),
        StorageBufferCount ( 0 )
    {
    }
};

//==============================================================================
//  SHADER API
//==============================================================================

struct rbuffer;

struct shader_compute_texture_binding
{
    const shader_resource* pTexture;
    u32                    MipLevel;
    u32                    Layer;
    xbool                  bCycle;

    shader_compute_texture_binding( void ) :
        pTexture( NULL ),
        MipLevel( 0 ),
        Layer   ( 0 ),
        bCycle  ( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_compute_buffer_binding
{
    const rbuffer* pBuffer;
    xbool          bCycle;

    shader_compute_buffer_binding( void ) :
        pBuffer( NULL ),
        bCycle ( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_compute_pass_desc
{
    const shader_compute_texture_binding* pTextures;
    u32                                   TextureCount;
    const shader_compute_buffer_binding*  pBuffers;
    u32                                   BufferCount;

    shader_compute_pass_desc( void ) :
        pTextures   ( NULL ),
        TextureCount( 0 ),
        pBuffers    ( NULL ),
        BufferCount ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

xbool               shader_BeginComputePass ( const shader&                   Shader,
                                              const shader_compute_pass_desc& Desc );
xbool               shader_EndComputePass   ( void );
xbool               shader_Dispatch         ( u32 GroupCountX,
                                              u32 GroupCountY = 1,
                                              u32 GroupCountZ = 1 );
xbool               shader_DispatchIndirect ( const rbuffer& Buffer, u32 Offset = 0 );

void                shader_SetCatalogId     ( u64 CatalogId );
shader_format       shader_GetPreferredFormat( void );
void                shader_Init             ( void );
void                shader_Kill             ( void );

xbool               shader_Create           ( shader&            Shader,
                                              const shader_desc& Desc );
xbool               shader_LoadFromFile     ( shader&                 Shader,
                                              const shader_file_desc& Desc );
xbool               shader_LoadProgram      ( shader&                Shader,
                                              const shader_program_desc& Desc );
xbool               shader_LoadProgram      ( shader&      Shader,
                                              const char*  pEcsFileName,
                                              u64 ProgramId );
xbool               shader_RequestProgram   ( shader& Shader, const shader_program_desc& Desc );
shader_program_load_status shader_PollProgram( shader& Shader );
xbool               shader_RequestComboStage( shader& Shader, const char* pFileName,
                                               shader_combo_kind Kind, u64 Key, shader_stage Stage );
xbool               shader_LoadComboStage   ( shader& Shader, const char* pFileName,
                                              shader_combo_kind Kind, u64 Key, shader_stage Stage );
xbool               shader_Destroy          ( shader&            Shader );

xbool               shader_PushUniformData  ( shader_stage Stage,
                                              u32          Slot,
                                              const void*  pData,
                                              u32          Size );
xbool               shader_PushUniformData  ( const shader& Shader,
                                              shader_stage  Stage,
                                              const char*   pName,
                                              const void*   pData,
                                              u32           Size );
xbool               shader_PushUniformData  ( const shader_uniform_binding& Binding );

xbool               shader_BindSampler      ( const shader_sampler_binding& Binding );
xbool               shader_BindSampler      ( const shader&          Shader,
                                              shader_stage           Stage,
                                              const char*            pName,
                                              const shader_resource* pTexture,
                                              const rstate_sampler*  pSampler );
xbool               shader_BindSamplers     ( const shader_sampler_binding* pBindings,
                                              u32                           Count );

xbool               shader_BindStorageTexture
                                            ( const shader_storage_texture_binding& Binding );
xbool               shader_BindStorageTexture
                                            ( const shader&          Shader,
                                              shader_stage           Stage,
                                              const char*            pName,
                                              const shader_resource* pTexture );
xbool               shader_BindStorageTextures
                                            ( const shader_storage_texture_binding* pBindings,
                                              u32                                   Count );

xbool               shader_BindStorageBuffer
                                            ( const shader_storage_buffer_binding& Binding );
xbool               shader_BindStorageBuffer
                                            ( const shader&          Shader,
                                              shader_stage           Stage,
                                              const char*            pName,
                                              const shader_resource* pBuffer );
xbool               shader_BindStorageBuffers
                                            ( const shader_storage_buffer_binding* pBindings,
                                              u32                                   Count );

xbool               shader_BindSet          ( const shader_binding_set_desc& Desc );

xbool               shader_FindBindingSlot  ( const shader&       Shader,
                                              shader_binding_kind Kind,
                                              const char*         pName,
                                              u32&                Slot );
xbool               shader_FindSampledTextureSlot
                                            ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );
xbool               shader_FindUniformSlot  ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );
xbool               shader_FindStorageTextureSlot
                                            ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );
xbool               shader_FindStorageBufferSlot
                                            ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );

//==============================================================================
#endif // E_SHADER_HPP
//==============================================================================
