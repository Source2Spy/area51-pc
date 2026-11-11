//=========================================================================
//
//  FrameGraph_Validation.cpp
//
//=========================================================================

#include "FrameGraph_Private.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void FrameGraphBuilder::BeginResourceValidation( const Pass& PassData )
{
    shader_SetResourceAccessValidator( NULL, NULL );

    if( !PassData.Desc.ValidateResources )
    {
        return;
    }

    u32 Capacity = 8;
    const u64 Required = static_cast<u64>( PassData.Accesses.GetCount() ) * 2;
    while( Capacity < Required )
    {
        if( Capacity >= 0x40000000u )
        {
            Fail( "pass resource validation exceeds array capacity" );
            return;
        }

        Capacity *= 2;
    }
    m_ResourceValidation.SetCount( Capacity );
    for( u32 i = 0; i < Capacity; ++i )
    {
        m_ResourceValidation[i].pResource = NULL;
        m_ResourceValidation[i].Access    = 0;
    }

    for( s32 i = 0; i < PassData.Accesses.GetCount(); ++i )
    {
        const PassAccess& Access = PassData.Accesses[i];
        const shader_resource* pResource = GetShaderResource( m_GraphResources[Access.Resource]->Handle );
        if( !pResource )
        {
            continue;
        }

        u32 Slot = static_cast<u32>( reinterpret_cast<uaddr>( pResource->pBackend ) >> 4 ) & ( Capacity - 1 );
        while( m_ResourceValidation[Slot].pResource &&
               m_ResourceValidation[Slot].pResource != pResource->pBackend )
        {
            Slot = ( Slot + 1 ) & ( Capacity - 1 );
        }
        m_ResourceValidation[Slot].pResource = pResource->pBackend;
        m_ResourceValidation[Slot].Access   |= Access.Flags;
    }

    shader_SetResourceAccessValidator( ValidateResourceAccess, this );
}

//=========================================================================

xbool FrameGraphBuilder::ValidateResourceAccess( const shader_resource& ResourceData,
                                                shader_resource_access Access, void* pContext )
{
    FrameGraphBuilder& Graph = *static_cast<FrameGraphBuilder*>( pContext );
    u32 Required = 0;
    switch( Access )
    {
        case SHADER_RESOURCE_ACCESS_SAMPLED:  Required = RENDER_RESOURCE_ACCESS_SAMPLED_READ;  break;
        case SHADER_RESOURCE_ACCESS_STORAGE:  Required = RENDER_RESOURCE_ACCESS_STORAGE_READ;  break;
        case SHADER_RESOURCE_ACCESS_VERTEX:   Required = RENDER_RESOURCE_ACCESS_VERTEX_READ;   break;
        case SHADER_RESOURCE_ACCESS_INDEX:    Required = RENDER_RESOURCE_ACCESS_INDEX_READ;    break;
        case SHADER_RESOURCE_ACCESS_INDIRECT: Required = RENDER_RESOURCE_ACCESS_INDIRECT_READ; break;
        case SHADER_RESOURCE_ACCESS_UPLOAD:   Required = RENDER_RESOURCE_ACCESS_COPY_DEST;     break;
    }

    const u32 Mask = static_cast<u32>( Graph.m_ResourceValidation.GetCount() ) - 1;
    u32 Slot = static_cast<u32>( reinterpret_cast<uaddr>( ResourceData.pBackend ) >> 4 ) & Mask;
    while( Graph.m_ResourceValidation[Slot].pResource )
    {
        const ResourceValidation& Existing = Graph.m_ResourceValidation[Slot];
        if( Existing.pResource == ResourceData.pBackend )
        {
            if( Required && ( Existing.Access & Required ) == Required )
            {
                return TRUE;
            }
            break;
        }
        Slot = ( Slot + 1 ) & Mask;
    }

    const rbuffer* pBuffer = shader_GetResourceBuffer( ResourceData );
    const rtarget* pTarget = shader_GetResourceTarget( ResourceData );
    const vram_texture* pTexture = shader_GetResourceTexture( ResourceData );
    Graph.m_pFailureResource = pBuffer ? pBuffer->Desc.pDebugName :
                                pTarget ? pTarget->Desc.pDebugName :
                                pTexture ? pTexture->Desc.pDebugName : "UnknownResource";
    Graph.Fail( "GPU resource access was not declared by the executing pass" );
    return FALSE;
}

//=========================================================================

} // namespace render
