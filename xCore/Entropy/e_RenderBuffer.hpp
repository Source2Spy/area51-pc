//==============================================================================
//
//  e_RenderBuffer.hpp
//
//  Explicit GPU buffer API for PSO render backends.
//
//==============================================================================

#ifndef E_RENDERBUFFER_HPP
#define E_RENDERBUFFER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#include "e_Shader.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum rbuffer_usage_flags
{
    RBUFFER_USAGE_VERTEX                = (1 << 0),
    RBUFFER_USAGE_INDEX                 = (1 << 1),
    RBUFFER_USAGE_INDIRECT              = (1 << 2),
    RBUFFER_USAGE_GRAPHICS_STORAGE_READ = (1 << 3),
    RBUFFER_USAGE_COMPUTE_STORAGE_READ  = (1 << 4),
    RBUFFER_USAGE_COMPUTE_STORAGE_WRITE = (1 << 5)
};

//------------------------------------------------------------------------------

enum rbuffer_index_format
{
    RBUFFER_INDEX_FORMAT_U16 = 0,
    RBUFFER_INDEX_FORMAT_U32
};

//==============================================================================
//  RENDER BUFFER STRUCTURES
//==============================================================================

struct rbuffer_backend;

//------------------------------------------------------------------------------

struct rbuffer_desc
{
    u32         Size;
    u32         Stride;
    u32         UsageFlags;
    const char* pDebugName;

    rbuffer_desc( void ) :
        Size      ( 0 ),
        Stride    ( 0 ),
        UsageFlags( 0 ),
        pDebugName( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rbuffer
{
    rbuffer_desc     Desc;
    rbuffer_backend* pBackend;

    rbuffer( void ) :
        Desc    (),
        pBackend( NULL )
    {
    }

    operator xbool( void ) const { return pBackend != NULL; }
};

//------------------------------------------------------------------------------

struct rbuffer_upload_desc
{
    const void* pData;
    u32         Offset;
    u32         Size;
    xbool       bCycle;

    rbuffer_upload_desc( void ) :
        pData ( NULL ),
        Offset( 0 ),
        Size  ( 0 ),
        bCycle( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct rbuffer_upload_batch_item
{
    rbuffer*             pBuffer;
    rbuffer_upload_desc  Upload;

    rbuffer_upload_batch_item( void ) :
        pBuffer( NULL ),
        Upload ()
    {
    }
};

//------------------------------------------------------------------------------

struct rbuffer_vertex_binding
{
    const rbuffer* pBuffer;
    u32            Slot;
    u32            Offset;

    rbuffer_vertex_binding( void ) :
        pBuffer( NULL ),
        Slot   ( 0 ),
        Offset ( 0 )
    {
    }

    rbuffer_vertex_binding( const rbuffer* pBindBuffer, u32 BindSlot, u32 BindOffset = 0 ) :
        pBuffer( pBindBuffer ),
        Slot   ( BindSlot ),
        Offset ( BindOffset )
    {
    }
};

//==============================================================================
//  RENDER BUFFER FUNCTIONS
//==============================================================================

xbool                   rbuffer_Create              ( rbuffer& Buffer,
                                                      const rbuffer_desc& Desc,
                                                      const void* pInitialData = NULL );
void                    rbuffer_Swap                ( rbuffer& A, rbuffer& B );
void                    rbuffer_Destroy             ( rbuffer& Buffer );

xbool                   rbuffer_Upload              ( rbuffer& Buffer,
                                                      const rbuffer_upload_desc& Upload );
xbool                   rbuffer_UploadBatch         ( const rbuffer_upload_batch_item* pItems,
                                                      u32 Count );
xbool                   rbuffer_Upload              ( rbuffer& Buffer,
                                                      const void* pData,
                                                      u32 Size,
                                                      u32 Offset = 0,
                                                      xbool bCycle = FALSE );

xbool                   rbuffer_BindVertex          ( const rbuffer& Buffer,
                                                      u32 Slot,
                                                      u32 Offset = 0 );
xbool                   rbuffer_BindVertex          ( const rbuffer_vertex_binding* pBindings,
                                                      u32 Count );
xbool                   rbuffer_BindIndex           ( const rbuffer& Buffer,
                                                      rbuffer_index_format Format,
                                                      u32 Offset = 0 );

xbool                   rbuffer_IsValid             ( const rbuffer& Buffer );
const rbuffer_desc*     rbuffer_GetDesc             ( const rbuffer& Buffer );
const shader_resource*  rbuffer_GetResource         ( const rbuffer& Buffer );

//==============================================================================
#endif // E_RENDERBUFFER_HPP
//==============================================================================
