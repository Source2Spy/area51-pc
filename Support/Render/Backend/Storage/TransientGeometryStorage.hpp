//=========================================================================
//
//  TransientGeometryStorage.hpp
//
//=========================================================================

#ifndef TRANSIENT_GEOMETRY_STORAGE_HPP
#define TRANSIENT_GEOMETRY_STORAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "e_RenderBuffer.hpp"
#include "e_RenderDraw.hpp"

namespace render
{

class FrameGraphBuilder;

//=========================================================================
//  CLASS
//=========================================================================

class TransientGeometryStorage
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct Primitive
    {
        const void* pVertex;
        const u16*  pIndex;
        s32         nVertices;
        s32         nIndices;

        Primitive( void );
    };

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    TransientGeometryStorage( void );

    xbool Init( s32 VertexStride, s32 InitialVertices = 8192, s32 InitialIndices = 24576 );
    void  Kill( void );
    xbool IsInitialized( void ) const;

    //-------------------------------------------------------------------------
    //  UPLOAD AND DRAW
    //-------------------------------------------------------------------------

    xbool Reserve( s32 nVertices, s32 nIndices );
    xbool UploadPrimitive( const Primitive& PrimitiveData, FrameGraphBuilder& Graph );
    xbool BindBuffers( void ) const;
    void DeclareResources( FrameGraphBuilder& Graph, u32 Pass ) const;
    xbool DrawIndexed( s32 IndexCount, s32 StartIndex, s32 BaseVertex ) const;

protected:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool EnsureCapacity( s32 RequiredVertices, s32 RequiredIndices );
    xbool CreateBuffers( s32 MaxVertices, s32 MaxIndices );
    void  DestroyBuffers( void );

protected:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    rbuffer m_VertexBuffer;
    rbuffer m_IndexBuffer;

    s32   m_VertexStride;
    s32   m_MaxVertices;
    s32   m_MaxIndices;
    s32   m_UploadedVertices;
    s32   m_UploadedIndices;
    xbool m_isInitialized;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
TransientGeometryStorage::Primitive::Primitive( void )
    : pVertex( NULL ),
      pIndex( NULL ),
      nVertices( 0 ),
      nIndices( 0 )
{
}

//=========================================================================

inline
xbool TransientGeometryStorage::IsInitialized( void ) const
{
    return m_isInitialized;
}

} // namespace render

#endif // TRANSIENT_GEOMETRY_STORAGE_HPP
//=========================================================================
