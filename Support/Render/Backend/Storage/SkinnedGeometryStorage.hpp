//=========================================================================
//
//  SkinnedGeometryStorage.hpp
//
//=========================================================================

#ifndef SKINNED_GEOMETRY_STORAGE_HPP
#define SKINNED_GEOMETRY_STORAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "StaticGeometryStorage.hpp"

namespace render
{

class FrameGraphBuilder;

//=========================================================================
//  STRUCTS
//=========================================================================

struct SkinGpuVertex
{
    vector4 Position;
    vector4 Normal;
    vector4 UVWeights;
};

static_assert( sizeof( SkinGpuVertex ) == 48, "skin vertex layout must match HLSL" );

//=========================================================================
//  CLASSES
//=========================================================================

class SkinnedGeometryStorage
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum
    {
        INVALID_BONE_REMAP = 0xFFFF,
        MAX_BONE_PALETTE   = 96
    };

    //-------------------------------------------------------------------------

    struct SoftSection
    {
        s32 FirstIndex;
        s32 IndexCount;
        u16 BoneRemap[MAX_BONE_PALETTE];

        SoftSection( void );
    };

    //-------------------------------------------------------------------------

    struct PreparedMesh
    {
        StaticGeometryStorage::PreparedMesh VertexData;
        xarray<SoftSection>                 Sections;

        void Clear( void );
    };

    //-------------------------------------------------------------------------

    struct SectionInput
    {
        s32        FirstIndex;
        s32        IndexCount;
        const u16* pBoneRemap;
        s32        BoneCount;
    };

    //-------------------------------------------------------------------------

    struct MeshView
    {
        StaticGeometryStorage::mesh_range Geom;
        const SoftSection*                pSections;
        s32                               SectionCount;
        u32                               FirstBoneRemap;

        MeshView( void );
    };

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    SkinnedGeometryStorage( void );
    void  Init( void );
    void  Kill( void );
    xbool IsInitialized( void ) const;

    static xbool PrepareMesh( PreparedMesh&        Prepared,
                              const SkinGpuVertex* pVertex,
                              s32                  nVertices,
                              const u16*           pIndex,
                              s32                  nIndices,
                              s32                  nBones,
                              const SectionInput*  pSections,
                              s32                  nSections );

    xhandle AddMesh( const PreparedMesh& Prepared );
    void    RemoveMesh( xhandle hMesh );

    xbool                  BindPools( const StaticGeometryStorage::mesh_range& Range ) const;
    void DeclareResources( FrameGraphBuilder& Graph, u32 Pass, const StaticGeometryStorage::mesh_range& Range ) const;
    xbool                  GetMeshView( xhandle hMesh, MeshView& View ) const;
    const shader_resource* GetBoneRemapResource( void ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct SkinMesh
    {
        xhandle             hGeom;
        xarray<SoftSection> Sections;
        u32                 FirstBoneRemap;

        SkinMesh( void );
    };

    //-------------------------------------------------------------------------

    struct RemapRange
    {
        u32 First;
        u32 Count;
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool UploadBoneRemaps( void );
    void  FreeBoneRemaps( u32 First, u32 Count );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<u32>           m_BoneRemaps;
    xarray<RemapRange>    m_FreeBoneRemaps;
    rbuffer               m_BoneRemapBuffer;
    u32                   m_BoneRemapCapacity;
    StaticGeometryStorage m_StaticGeometryStorage;
    xharray<SkinMesh>     m_Meshes;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
SkinnedGeometryStorage::MeshView::MeshView( void )
    : Geom(),
      pSections( NULL ),
      SectionCount( 0 ),
      FirstBoneRemap( 0 )
{
}

//=========================================================================

inline
xbool SkinnedGeometryStorage::IsInitialized( void ) const
{
    return m_StaticGeometryStorage.IsInitialized();
}

//=========================================================================

inline
SkinnedGeometryStorage::SkinMesh::SkinMesh( void )
    : hGeom(),
      Sections(),
      FirstBoneRemap( 0xffffffffu )
{
    hGeom.Handle = HNULL;
}

} // namespace render

#endif // SKINNED_GEOMETRY_STORAGE_HPP
//=========================================================================
