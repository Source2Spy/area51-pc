//=========================================================================
//
//  FrameGraph_Transfers.cpp
//
//=========================================================================

#include "FrameGraph_Private.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

struct FrameGraphBuilder::TransferData
{
    RenderResourceHandle      Resource;
    rbuffer*                  pBuffer = NULL;
    vram_texture*             pTexture = NULL;
    rbuffer_upload_desc       BufferUpload;
    vram_texture_upload_desc  TextureUpload;
    xarray<u8>                Bytes;
    xbool                     GenerateMips = FALSE;
    xbool*                    pPending = NULL;
};

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool FrameGraphBuilder::ExecuteTransfer( RenderPassContext& Context, TransferData Data )
{
    if( !Context.GetFrame().pGraph->GetShaderResource( Data.Resource ) )
    {
        return FALSE;
    }

    if( Data.GenerateMips )
    {
        return vram_GenerateMipmaps( *Data.pTexture );
    }

    if( Data.pBuffer )
    {
        Data.BufferUpload.pData = Data.Bytes.GetPtr();
        return rbuffer_Upload( *Data.pBuffer, Data.BufferUpload );
    }

    Data.TextureUpload.pData = Data.Bytes.GetPtr();
    if( !vram_UploadTexture( *Data.pTexture, Data.TextureUpload ) )
    {
        return FALSE;
    }
    if( Data.pPending )
    {
        *Data.pPending = FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::QueueUpload( rbuffer& Buffer, const rbuffer_upload_desc& Upload )
{
    if( !CanDeclare( "QueueUpload" ) || !Buffer || !Upload.pData || !Upload.Size || Upload.Size > 0x7fffffffu ||
        Upload.Offset > Buffer.Desc.Size || Upload.Size > Buffer.Desc.Size - Upload.Offset )
    {
        Fail( "invalid buffer upload" );
        return FALSE;
    }

    TransferData Data;
    Data.Resource      = ImportExternal( Buffer, Buffer.Desc.pDebugName ? Buffer.Desc.pDebugName : "ExternalBuffer" );
    Data.pBuffer       = &Buffer;
    Data.BufferUpload  = Upload;
    Data.Bytes.SetCount( static_cast<s32>( Upload.Size ) );
    x_memcpy( Data.Bytes.GetPtr(), Upload.pData, Upload.Size );

    RenderPassDesc Desc;
    Desc.pDebugName = "UploadBuffer";
    Desc.Type       = RENDER_PASS_TRANSFER;
    const RenderPassId Pass = AddPass( Desc, ExecuteTransfer, Data );
    if( Pass == INVALID_RENDER_PASS || !Data.Resource.IsValid() )
    {
        return FALSE;
    }

    AddWriteAccess( Pass, Data.Resource, RENDER_RESOURCE_ACCESS_COPY_DEST );
    if( !Upload.bCycle && ( Upload.Offset || Upload.Size != Buffer.Desc.Size ) )
    {
        AddReadAccess( Pass, Data.Resource, RENDER_RESOURCE_ACCESS_PRESERVE );
    }
    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::QueueUpload( rbuffer& Buffer, const void* pData, u32 Size, u32 Offset, xbool Cycle )
{
    rbuffer_upload_desc Upload;
    Upload.pData = pData;
    Upload.Size = Size;
    Upload.Offset = Offset;
    Upload.bCycle = Cycle;
    return QueueUpload( Buffer, Upload );
}

//=========================================================================

xbool FrameGraphBuilder::QueueUploadBatch( const rbuffer_upload_batch_item* pItems, u32 Count )
{
    if( Count && !pItems )
    {
        Fail( "buffer upload batch has no items" );
        return FALSE;
    }
    for( u32 i = 0; i < Count; ++i )
    {
        if( !pItems[i].pBuffer || !QueueUpload( *pItems[i].pBuffer, pItems[i].Upload ) )
        {
            return FALSE;
        }
    }
    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::QueueUpload( vram_texture& Texture, const vram_texture_upload_desc& Upload, xbool* pPending )
{
    if( !CanDeclare( "QueueUpload" ) || !Texture || !Upload.pData || !Upload.Size || Upload.Size > 0x7fffffffu )
    {
        Fail( "invalid texture upload" );
        return FALSE;
    }

    if( pPending && m_PendingTextureUploads.Find( pPending ) >= 0 )
    {
        return TRUE;
    }

    TransferData Data;
    Data.Resource      = ImportExternal( Texture, Texture.Desc.pDebugName ? Texture.Desc.pDebugName : "ExternalTexture" );
    Data.pTexture      = &Texture;
    Data.pPending      = pPending;
    Data.TextureUpload = Upload;
    Data.Bytes.SetCount( static_cast<s32>( Upload.Size ) );
    x_memcpy( Data.Bytes.GetPtr(), Upload.pData, Upload.Size );

    RenderPassDesc Desc;
    Desc.pDebugName = "UploadTexture";
    Desc.Type       = RENDER_PASS_TRANSFER;
    const RenderPassId Pass = AddPass( Desc, ExecuteTransfer, Data );
    if( Pass == INVALID_RENDER_PASS || !Data.Resource.IsValid() )
    {
        return FALSE;
    }

    AddWriteAccess( Pass, Data.Resource, RENDER_RESOURCE_ACCESS_COPY_DEST );
    if( !Upload.bCycle )
    {
        AddReadAccess( Pass, Data.Resource, RENDER_RESOURCE_ACCESS_PRESERVE );
    }
    if( pPending )
    {
        m_PendingTextureUploads.Append() = pPending;
    }
    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::QueueUploadBatch( const vram_texture_upload_batch_item* pItems, u32 Count,
                                          xbool* const* ppPending )
{
    if( Count && !pItems )
    {
        Fail( "texture upload batch has no items" );
        return FALSE;
    }
    for( u32 i = 0; i < Count; ++i )
    {
        if( !pItems[i].pTexture ||
            !QueueUpload( *pItems[i].pTexture, pItems[i].Upload, ppPending ? ppPending[i] : NULL ) )
        {
            return FALSE;
        }
    }
    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::QueueGenerateMipmaps( vram_texture& Texture )
{
    if( !CanDeclare( "QueueGenerateMipmaps" ) || !Texture )
    {
        Fail( "invalid mip generation resource" );
        return FALSE;
    }
    if( Texture.Desc.MipCount <= 1 )
    {
        return TRUE;
    }

    TransferData Data;
    Data.Resource      = ImportExternal( Texture, Texture.Desc.pDebugName ? Texture.Desc.pDebugName : "MipTexture" );
    Data.pTexture      = &Texture;
    Data.GenerateMips  = TRUE;

    RenderPassDesc Desc;
    Desc.pDebugName = "GenerateMipmaps";
    Desc.Type       = RENDER_PASS_TRANSFER;
    const RenderPassId Pass = AddPass( Desc, ExecuteTransfer, Data );
    if( Pass == INVALID_RENDER_PASS || !Data.Resource.IsValid() )
    {
        return FALSE;
    }

    AddReadAccess( Pass, Data.Resource, RENDER_RESOURCE_ACCESS_COPY_SOURCE | RENDER_RESOURCE_ACCESS_PRESERVE );
    AddWriteAccess( Pass, Data.Resource, RENDER_RESOURCE_ACCESS_COPY_DEST );
    return TRUE;
}

//=========================================================================

} // namespace render
