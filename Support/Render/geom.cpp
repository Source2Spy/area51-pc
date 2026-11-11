//=========================================================================
//
//  Geom.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "geom.hpp"
#include "Material.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

geom::geom( void )
{
    x_memset( this, 0, sizeof( geom ) );
}

//=========================================================================

geom::~geom( void )
{
    delete[] m_pBone;
    delete[] m_pBoneMasks;
    delete[] m_pPropertySections;
    delete[] m_pProperties;
    delete[] m_pRigidBodies;
    delete[] m_pMesh;
    delete[] m_pSubMesh;
    delete[] m_pMaterial;
    delete[] m_pPreparedMaterials;
    delete[] m_pTexture;
    delete[] m_pUVKey;
    delete[] m_pLODSizes;
    delete[] m_pLODMasks;
    delete[] m_pVirtualMeshes;
    delete[] m_pVirtualTextures;
    delete[] m_pStringData;
}

//=========================================================================

geom::bone_masks const* geom::FindBoneMasks( char const* pName ) const
{
    // Loop through all bone masks
    for ( s32 i = 0; i < m_nBoneMasks; i++ )
    {
        // Lookup name of bone masks entry
        char const* pBoneMasksName = &m_pStringData[m_pBoneMasks[i].NameOffset];

        // Match?
        if ( x_strcmp( pBoneMasksName, pName ) == 0 )
        {
            return &m_pBoneMasks[i];
        }
    }

    // Not found
    return NULL;
}

//=========================================================================

xbool geom::GetPropertyFloat( geom::property_section const* pSection, char const* pName, f32* pValue ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_FLOAT );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    *pValue = pProp->Value.Float;
    return TRUE;
}

//=========================================================================

xbool geom::GetPropertyInteger( geom::property_section const* pSection, char const* pName, s32* pValue ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_INTEGER );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    *pValue = pProp->Value.Integer;
    return TRUE;
}

//=========================================================================

xbool geom::GetPropertyAngle( geom::property_section const* pSection, char const* pName, radian* pValue ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_ANGLE );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    *pValue = pProp->Value.Angle;
    return TRUE;
}

//=========================================================================

xbool geom::GetPropertyString( geom::property_section const* pSection, char const* pName, char* pValue,
                               s32 nChars ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_STRING );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    x_strsavecpy( pValue, &m_pStringData[pProp->Value.StringOffset], nChars );
    return TRUE;
}

//=========================================================================

// The serialized asset remains unchanged; the prepared surface is runtime data.
xbool geom::PrepareMaterials( void )
{
    if( m_pPreparedMaterials )
        return TRUE;

    ::material* Prepared = new ::material[m_nVirtualMaterials];

    for( s32 i = 0; i < m_nMaterials; ++i )
    {
        geom::material const& Source      = m_pMaterial[i];
        const s32             Environment = Source.iTexture + Source.nVirtualMats;
        const s32             Detail      = Environment + ( ( Source.Flags & MATERIAL_FLAG_HAS_ENV_MAP ) ? 1 : 0 );

        // Validate the serialized material's ranges before reading its variants.
        if( Source.nVirtualMats <= 0 || Source.iVirtualMat < 0 ||
            Source.iVirtualMat + Source.nVirtualMats > m_nVirtualMaterials ||
            Source.iTexture < 0 || Source.iTexture + Source.nVirtualMats > m_nTextures ||
            ( ( Source.Flags & MATERIAL_FLAG_HAS_ENV_MAP ) && Environment >= m_nTextures ) ||
            ( ( Source.Flags & MATERIAL_FLAG_HAS_DETAIL_MAP ) && Detail >= m_nTextures ) )
        {
            delete[] Prepared;
            return FALSE;
        }

        for( s32 Variant = 0; Variant < Source.nVirtualMats; ++Variant )
        {
            ::material& Destination = Prepared[Source.iVirtualMat + Variant];

            if( Destination.GetDesc().Valid )
            {
                x_DebugMsg( "Geom: duplicate virtual material %d\n", Source.iVirtualMat + Variant );
                delete[] Prepared;
                return FALSE;
            }

            Destination.m_Type              = Source.Type;
            Destination.m_Flags             = Source.Flags;
            Destination.m_DetailScale       = Source.DetailScale;
            Destination.m_FixedAlpha        = Source.FixedAlpha;
            Destination.m_UVAnim.iKey        = Source.UVAnim.iKey;
            Destination.m_UVAnim.Type        = Source.UVAnim.Type;
            Destination.m_UVAnim.nFrames     = Source.UVAnim.nKeys;
            Destination.m_UVAnim.FPS         = Source.UVAnim.FPS;
            Destination.m_UVAnim.StartFrame  = Source.UVAnim.StartFrame;

            Destination.m_DiffuseMap.SetName( GetTextureName( Source.iTexture + Variant ), FALSE );

            if( Source.Flags & MATERIAL_FLAG_HAS_ENV_MAP )
                Destination.m_EnvironmentMap.SetName( GetTextureName( Environment ), FALSE );

            if( Source.Flags & MATERIAL_FLAG_HAS_DETAIL_MAP )
                Destination.m_DetailMap.SetName( GetTextureName( Detail ), FALSE );

            Destination.Finalize();

            if( !Destination.GetDesc().Valid )
            {
                delete[] Prepared;
                return FALSE;
            }

            render::MaterialDesc const& Desc = Destination.GetDesc();

            if( ( ( Desc.Surface == render::SurfaceRecipe::Diffuse || Desc.AlphaTest ) &&
                  !GetTextureName( Source.iTexture + Variant )[0] ) ||
                ( Desc.Detail && !GetTextureName( Detail )[0] ) ||
                ( Desc.Environment == render::EnvironmentSource::Texture2D &&
                  ( !( Source.Flags & MATERIAL_FLAG_HAS_ENV_MAP ) || !GetTextureName( Environment )[0] ) ) )
            {
                x_DebugMsg( "Geom: material %d variant %d lacks a required texture declaration\n", i, Variant );
                delete[] Prepared;
                return FALSE;
            }
        }
    }

    // Publish the prepared array only when every virtual material is valid.
    for( s32 i = 0; i < m_nVirtualMaterials; ++i )
    {
        if( !Prepared[i].GetDesc().Valid )
        {
            x_DebugMsg( "Geom: virtual material %d has no material description\n", i );
            delete[] Prepared;
            return FALSE;
        }
    }

    m_pPreparedMaterials = Prepared;

    return TRUE;
}

//=========================================================================

::material const& geom::GetPreparedMaterial( s32 VirtualMaterial ) const
{
    ASSERT( m_pPreparedMaterials && VirtualMaterial >= 0 && VirtualMaterial < m_nVirtualMaterials );
    return m_pPreparedMaterials[VirtualMaterial];
}
