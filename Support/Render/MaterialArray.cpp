//=========================================================================
//
//  MaterialArray.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Render.hpp"

#define RENDER_PRIVATE
#include "MaterialArray.hpp"
#undef RENDER_PRIVATE

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

MaterialArray::MaterialArray( void )
    : m_isSorted( FALSE ), m_Capacity( FALSE ), m_nNodes( 0 ), m_pNodes( NULL ), m_pIndices( NULL )
{
}

//=========================================================================

MaterialArray::~MaterialArray( void )
{
    Clear();
    if ( m_pNodes )
    {
        x_free( m_pNodes );
    }
}

//=========================================================================

s32 NodeCompareFn( void const* pA, void const* pB )
{
    MaterialArray::node_info const* pNodeA = static_cast<MaterialArray::node_info const*>( pA );
    MaterialArray::node_info const* pNodeB = static_cast<MaterialArray::node_info const*>( pB );
    material const*                 pMatA = &pNodeA->Item;
    material const*                 pMatB = &pNodeB->Item;

    if ( pMatA->GetSortPriority() > pMatB->GetSortPriority() )
    {
        return 1;
    }
    if ( pMatA->GetSortPriority() < pMatB->GetSortPriority() )
    {
        return -1;
    }
    if ( pMatA->m_DiffuseMap.GetIndex() > pMatB->m_DiffuseMap.GetIndex() )
    {
        return 1;
    }
    if ( pMatA->m_DiffuseMap.GetIndex() < pMatB->m_DiffuseMap.GetIndex() )
    {
        return -1;
    }
    if ( pMatA->m_EnvironmentMap.GetIndex() > pMatB->m_EnvironmentMap.GetIndex() )
    {
        return 1;
    }
    if ( pMatA->m_EnvironmentMap.GetIndex() < pMatB->m_EnvironmentMap.GetIndex() )
    {
        return -1;
    }
    if ( pMatA->m_DetailMap.GetIndex() > pMatB->m_DetailMap.GetIndex() )
    {
        return 1;
    }
    if ( pMatA->m_DetailMap.GetIndex() < pMatB->m_DetailMap.GetIndex() )
    {
        return -1;
    }

    return 0;
}

//=========================================================================

void MaterialArray::Sort( void )
{
    if ( m_isSorted )
    {
        return;
    }

    if ( m_nNodes )
    {
        // sort the materials
        x_qsort( m_pNodes, m_nNodes, sizeof( node_info ), NodeCompareFn );

        // remap the material indices
        for ( s32 i = 0; i < m_nNodes; i++ )
        {
            ASSERT( ( m_pNodes[i].Handle.Handle >= 0 ) && ( m_pNodes[i].Handle.Handle < m_Capacity ) );
            m_pIndices[m_pNodes[i].Handle] = i;
        }
    }

    // sanity check
    if ( 0 )
    {
        for ( s32 iNode = 0; iNode < m_nNodes - 1; iNode++ )
        {
            ASSERT( NodeCompareFn( &m_pNodes[iNode + 1], &m_pNodes[iNode] ) >= 0 );
        }
    }

    m_isSorted = TRUE;
}

//=========================================================================

void MaterialArray::Clear( void )
{
    s32 i;

    // destruct all of the objects
    for ( i = 0; i < m_nNodes; i++ )
    {
        ( reinterpret_cast<destructor*>( &m_pNodes[i].Item ) )->~destructor();
    }

    // clear all of the handle indices
    for ( i = 0; i < m_Capacity; i++ )
    {
        m_pIndices[i] = -1;
    }

    m_nNodes = 0;
    m_FreeHandles.SetCount( 0 );
    for ( s32 Handle = m_Capacity - 1; Handle >= 0; --Handle )
        m_FreeHandles.Append() = Handle;
    m_isSorted = FALSE;
}

//=========================================================================

void MaterialArray::GrowListBy( s32 nNodes )
{
    ASSERT( nNodes > 0 );

    // increase the capacity
    s32 NewCapacity = m_Capacity + nNodes;

    // allocate the new arrays
    node_info* pNewNodes = static_cast<node_info*>( x_malloc( ( sizeof( node_info ) + sizeof( s32 ) ) * NewCapacity ) );
    ASSERT( pNewNodes );

    s32* pNewIndices = reinterpret_cast<s32*>( pNewNodes + NewCapacity );
    ASSERT( pNewIndices );

    // copy all the previous nodes to the new arrays
    if ( m_nNodes )
    {
        ASSERT( m_pNodes );
        x_memcpy( pNewNodes, m_pNodes, sizeof( node_info ) * m_nNodes );
    }
    if ( m_Capacity )
    {
        ASSERT( m_pIndices );
        x_memcpy( pNewIndices, m_pIndices, sizeof( s32 ) * m_Capacity );
    }

    // set the new indices to -1
    for ( s32 i = m_Capacity; i < NewCapacity; i++ )
    {
        pNewIndices[i] = -1;
    }

    if ( m_FreeHandles.GetCapacity() < NewCapacity )
        m_FreeHandles.SetCapacity( NewCapacity );
    for ( s32 Handle = NewCapacity - 1; Handle >= m_Capacity; --Handle )
        m_FreeHandles.Append() = Handle;

    // update the arrays
    if ( m_pNodes )
    {
        x_free( m_pNodes );
    }
    m_pNodes = pNewNodes;
    m_pIndices = pNewIndices;
    m_Capacity = NewCapacity;
}

//=========================================================================

material& MaterialArray::Add( xhandle& hHandle )
{
    // do we need to grow?
    if ( m_nNodes >= m_Capacity )
    {
        GrowListBy( MAX( m_Capacity / 2, 100 ) );
    }

    ASSERT( m_FreeHandles.GetCount() > 0 );
    s32 const LastFree = m_FreeHandles.GetCount() - 1;
    s32 const iHandle = m_FreeHandles[LastFree];
    m_FreeHandles.SetCount( LastFree );

    // fill in the index information
    m_pIndices[iHandle] = m_nNodes;
    hHandle.Handle = iHandle;

    // construct the node info
    xConstruct( &m_pNodes[m_nNodes].Item );
    m_pNodes[m_nNodes].Handle.Handle = iHandle;
    m_nNodes++;

    // we'll need to re-sort
    m_isSorted = FALSE;

    return m_pNodes[m_pIndices[iHandle]].Item;
}

//=========================================================================

void MaterialArray::DeleteByHandle( xhandle hHandle )
{
    s32 index = GetIndexByHandle( hHandle );
    ASSERT( ( index >= 0 ) && ( index < m_nNodes ) );

    // call destructor
    ( reinterpret_cast<destructor*>( &m_pNodes[index].Item ) )->~destructor();

    // clear out the index
    m_pIndices[hHandle.Handle] = -1;
    m_FreeHandles.Append() = hHandle.Handle;

    // copy the last node into the deleted node
    if ( index != ( m_nNodes - 1 ) )
    {
        xConstruct( &m_pNodes[index].Item );
        m_pNodes[index].Item = m_pNodes[m_nNodes - 1].Item;
        m_pNodes[index].Handle = m_pNodes[m_nNodes - 1].Handle;
        m_pIndices[m_pNodes[index].Handle.Handle] = index;
        ( reinterpret_cast<destructor*>( &m_pNodes[m_nNodes - 1].Item ) )->~destructor();
    }
    m_nNodes--;

    // flag that we'll need to sort again
    m_isSorted = FALSE;
}

//=========================================================================

xbool MaterialArray::SanityCheck( void ) const
{
    s32 i;

    for ( i = 0; i < m_nNodes; i++ )
    {
        ASSERT( m_pIndices[m_pNodes[i].Handle.Handle] == i );
    }

    for ( i = 0; i < m_Capacity; i++ )
    {
        if ( m_pIndices[i] != -1 )
        {
            ASSERT( m_pNodes[m_pIndices[i]].Handle.Handle == i );
        }
    }

    return TRUE;
}

//=========================================================================

void MaterialArray::Update( f32 DeltaTime )
{
    for ( s32 i = 0; i < GetCount(); i++ )
    {
        material&         mat = operator[]( i );
        material::uvanim& UvAnim = mat.m_UVAnim;
        if ( UvAnim.nFrames <= 1 )
        {
            continue;
        }

        // Artists duplicate the first and last frame to be consistent
        // with the sketal animations (this is to make blending and
        // looped anims work). We really don't want to do this for
        // uv anims, so consider nFrames to be one smaller.
        s32 nFrames = UvAnim.nFrames - 1;

        // calculate the next frame
        UvAnim.CurrentFrame += DeltaTime * UvAnim.FPS * UvAnim.Dir;

        // determine the type of animation
        switch ( UvAnim.Type )
        {
            case geom::material::uvanim::FIXED:
                {
                    UvAnim.Dir = 0;
                    UvAnim.CurrentFrame = static_cast<f32>( UvAnim.StartFrame );
                }
                break;
            case geom::material::uvanim::LOOPED:
                {
                    UvAnim.Dir = 1;
                    if ( UvAnim.CurrentFrame >= static_cast<f32>( nFrames ) )
                    {
                        UvAnim.CurrentFrame = static_cast<f32>( UvAnim.StartFrame );
                    }
                }
                break;
            case geom::material::uvanim::PINGPONG:
                {
                    if ( UvAnim.Dir > 0 )
                    {
                        UvAnim.Dir = 1;
                        if ( UvAnim.CurrentFrame >= static_cast<f32>( nFrames ) )
                        {
                            UvAnim.CurrentFrame = static_cast<f32>( nFrames - 1 );
                            UvAnim.Dir = -UvAnim.Dir;
                        }
                    }
                    else
                    {
                        UvAnim.Dir = -1;
                        if ( UvAnim.CurrentFrame <= 0.0f )
                        {
                            UvAnim.CurrentFrame = 0.0f;
                            UvAnim.Dir = -UvAnim.Dir;
                        }
                    }
                }
                break;
            case geom::material::uvanim::ONESHOT:
                {
                    UvAnim.Dir = 1;
                    if ( UvAnim.CurrentFrame >= static_cast<f32>( nFrames ) )
                    {
                        UvAnim.CurrentFrame = static_cast<f32>( nFrames - 1 );
                        UvAnim.Dir = 0;
                    }
                }
                break;
        }

        UvAnim.iFrame = static_cast<s8>( x_floor( UvAnim.CurrentFrame ) );
    }
}

//=========================================================================
