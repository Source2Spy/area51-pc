//=========================================================================
//
//  GeometryRepository.hpp
//
//=========================================================================

#ifndef RENDER_GEOMETRY_REPOSITORY_HPP
#define RENDER_GEOMETRY_REPOSITORY_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../RigidGeom.hpp"
#include "../../SkinGeom.hpp"
#include "../Storage/StaticGeometryStorage.hpp"
#include "../Storage/SkinnedGeometryStorage.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

// Registration handles contain a 20-bit slot and an 11-bit generation.
// A slot is retired when its generation is exhausted.
class GeometryRepository
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    using RigidMeshView = StaticGeometryStorage::mesh_range;
    using SkinMeshView  = SkinnedGeometryStorage::MeshView;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    GeometryRepository( void );
    ~GeometryRepository( void );

    xbool Init( void );
    void  Kill( void );
    xbool IsInitialized( void ) const;

    xhandle RegisterRigid( const rigid_geom& Geom );
    xhandle RegisterSkin( const skin_geom& Geom );
    xbool   UnregisterRigid( xhandle hGeom );
    xbool   UnregisterSkin( xhandle hGeom );

    xhandle                GetRigidSurface( xhandle hGeom, s32 iSurface ) const;
    xhandle                GetSkinSurface( xhandle hGeom, s32 iSurface ) const;
    xbool                  GetRigidMeshView( xhandle hMesh, RigidMeshView& View ) const;
    xbool                  GetSkinMeshView( xhandle hMesh, SkinMeshView& View ) const;
    const shader_resource* GetSkinBoneRemapResource( void ) const;
    void DeclareRigidResources( FrameGraphBuilder& Graph, u32 Pass, const RigidMeshView& View ) const;
    void DeclareSkinResources( FrameGraphBuilder& Graph, u32 Pass, const StaticGeometryStorage::mesh_range& Range ) const;
    xbool                  BindRigidMesh( const RigidMeshView& View ) const;
    xbool                  BindRigidVertexIndices( const RigidMeshView& View, u32 SlotIndex ) const;
    xbool                  BindSkinMesh( const SkinMeshView& View ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum
    {
        SLOT_BITS        = 20,
        SLOT_LIMIT       = 1 << SLOT_BITS,
        GENERATION_LIMIT = 1 << 11
    };

    //-------------------------------------------------------------------------

    struct Entry
    {
        xarray<xhandle> Surfaces;
        u32             Generation;
        xbool           Active;
        xbool           Retired;

        Entry( void );
    };

    //-------------------------------------------------------------------------

    struct Registry
    {
        xarray<Entry> Entries;
        xarray<u32>   FreeSlots;
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    static const Entry* Resolve( const Registry& RegistryData, xhandle Handle );
    static Entry*       Resolve( Registry& RegistryData, xhandle Handle );
    static xhandle      Allocate( Registry& RegistryData, const xarray<xhandle>& Surfaces );
    static void         Release( Registry& RegistryData, xhandle Handle );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    Registry               m_Rigid;
    Registry               m_Skin;
    StaticGeometryStorage  m_RigidStorage;
    SkinnedGeometryStorage m_SkinStorage;
    xbool                  m_isInitialized;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
GeometryRepository::Entry::Entry( void )
    : Surfaces(),
      Generation( 0 ),
      Active( FALSE ),
      Retired( FALSE )
{
}

} // namespace render

#endif // RENDER_GEOMETRY_REPOSITORY_HPP
