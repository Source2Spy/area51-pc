//=========================================================================
//
//  MaterialArray.hpp
//
//=========================================================================

#ifndef MATERIALARRAY_HPP
#define MATERIALARRAY_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Render.hpp"

#ifndef RENDER_PRIVATE
#error "This file is for internal use by the rendering system. Please don't include it!"
#endif

//=========================================================================
//  MATERIAL ARRAY
//=========================================================================
//
// This class is VERY similar to a standard xharray, except that while the
// xharray has a direct mapping between the handle and the data, ours will
// use an indirect mapping through indices. This will be better for sorting,
// and allow us to still store xhandles for the scene render sort key. Also
// this guarantees that we'll be accessing materials linearly for rendering.
// This class should only be used by the rendering system and the game should
// go through the interface provided in Render.hpp.
//
//=========================================================================

class MaterialArray
{
public:

    MaterialArray( void );
    ~MaterialArray( void );

    void      Sort( void );
    s32       GetCount( void ) const;
    void      Clear( void );
    void      GrowListBy( s32 nNodes );
    material& Add( xhandle& hHandle );
    material& operator[]( s32 index );
    material& operator()( xhandle hHandle );
    void      DeleteByHandle( xhandle hHandle );
    s32       GetIndexByHandle( xhandle hHandle );
    xhandle   GetHandleByIndex( s32 index );
    xbool     SanityCheck( void ) const;
    void      Update( f32 DeltaTime );

protected:

    struct destructor
    {
        material Item;
        inline ~destructor()
        {
        }
    };

    struct node_info
    {
        material Item;
        xhandle  Handle;
    };

    friend s32 NodeCompareFn( void const* pA, void const* pB );

    xbool       m_isSorted;  // is this guy sorted?
    s32         m_Capacity;
    s32         m_nNodes;
    node_info*  m_pNodes;    // indexes have a direct mapping into this array
    xarray<s32> m_FreeHandles;
    s32*        m_pIndices;  // handles have a direct mapping into this array
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline s32 MaterialArray::GetCount( void ) const
{
    return m_nNodes;
}

//=========================================================================

inline material& MaterialArray::operator[]( s32 index )
{
    ASSERT( ( index >= 0 ) && ( index < m_nNodes ) );
    return m_pNodes[index].Item;
}

//=========================================================================

inline material& MaterialArray::operator()( xhandle hHandle )
{
    ASSERT( ( hHandle.Handle >= 0 ) && ( hHandle.Handle < m_Capacity ) );
    return operator[]( m_pIndices[hHandle.Handle] );
}

//=========================================================================

inline s32 MaterialArray::GetIndexByHandle( xhandle hHandle )
{
    ASSERT( ( hHandle.Handle >= 0 ) && ( hHandle.Handle < m_Capacity ) );
    return m_pIndices[hHandle.Handle];
}

//=========================================================================

inline xhandle MaterialArray::GetHandleByIndex( s32 index )
{
    ASSERT( ( index >= 0 ) && ( index < m_nNodes ) );
    return m_pNodes[index].Handle;
}

//=========================================================================
#endif // MATERIALARRAY_HPP
//=========================================================================
