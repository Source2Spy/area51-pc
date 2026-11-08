//==============================================================================
//
//  ClothInst.hpp
//
//==============================================================================

#ifndef CLOTHINST_HPP
#define CLOTHINST_HPP

//==============================================================================
// INCLUDES
//==============================================================================
#include "Objects/Render/RigidInst.hpp"
#include "Objects/Cloth.hpp"
#include "Render/GeomDraw.hpp"
#include "Render/Features/Cloth/ClothTessellation.hpp"
#include "e_VRAM.hpp"


//==============================================================================
// CLASSES
//==============================================================================

// Renders a cloth simulation (Objects\Cloth.hpp) - owns the rigid instance for
// the non-cloth part of the mesh, plus the dynamic vertex/index buffers used to
// draw the simulated cloth part every frame.
class cloth_inst
{
// Functions
public:
             cloth_inst           ( void );
             ~cloth_inst          ( void );

            void        OnEnumProp           ( prop_enum&    List );
            xbool       OnProperty           ( prop_query&   I, cloth& Sim );

            void        Init                 ( cloth& Sim );
            void        Kill                 ( void );

    // Render functions
            void        RenderRigidGeometry  ( const cloth& Sim, xcolor Ambient, u32 Flags = render::CLIPPED );
            void        RenderClothGeometry  ( cloth& Sim, xcolor Ambient, s32 VTexture = 0, u32 Flags = 0 );
            void        RenderShadowCast     ( cloth& Sim, s32 VTexture, u64 ProjMask );

#if !defined( CONFIG_RETAIL )
            void        RenderSkeleton       ( const cloth& Sim );
#endif // !defined( CONFIG_RETAIL )

    // Query functions
          rigid_inst&   GetRigidInst         ( void );
          u64           GetRenderMask        ( void ) const;

private:
            xbool       InitDamageTexture    ( cloth& Sim, texture const* pTexture );
            xbool       PrepareDamageUpload  ( cloth& Sim );
            void        ComputeRenderNormals ( cloth const& Sim );
            void        BuildRenderVertices  ( cloth const& Sim );
            xbool       PrepareTessellation  ( cloth const& Sim );

// Data
public:

            rigid_inst                          m_RigidInst ;       // Rigid instance for the non-cloth part of the mesh
            u64                                 m_RenderMask ;      // Render instance mask
            s32                                 m_MaterialIndex ;   // Index of cloth material in rigid inst
            xarray<dynamic_geom_vertex>         m_RenderVertices ;  // Reused dynamic vertex data
            xarray<vector3>                     m_RenderNormals ;   // Reused per-vertex normals
            xarray<u16>                         m_RenderIndices ;   // Immutable topology
            cloth const*                        m_pRenderNormalsSource;
            u32                                 m_RenderNormalsRevision;
            render::ClothTessellation           m_Tessellation;
            xbool                               m_isTessellationInitialized;
            xarray<u8>                          m_DamageUpload ;    // Reused dirty-region upload data
            vram_texture                        m_DamageTexture ;   // GPU R8 damage mask
            xbool                               m_DamageUploadPending;
            s32                                 m_DamageUploadMinX;
            s32                                 m_DamageUploadMinY;
            s32                                 m_DamageUploadMaxX;
            s32                                 m_DamageUploadMaxY;
} ;

//==============================================================================
// Query functions
//==============================================================================

inline
rigid_inst& cloth_inst::GetRigidInst( void )
{
    return m_RigidInst;
}

//==============================================================================

inline
u64 cloth_inst::GetRenderMask( void ) const
{
    return m_RenderMask;
}

//==============================================================================


#endif  // #ifndef CLOTHINST_HPP
