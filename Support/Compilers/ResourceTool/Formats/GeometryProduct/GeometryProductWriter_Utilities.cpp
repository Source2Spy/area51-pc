//=========================================================================
//
//  GeometryProductWriter_Utilities.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void geometry_product_writer::CompileDictionary( geom& Geom )
{
    s32 i;

    // Map source strings to output offsets.
    s32* StringRemap = new s32[Context().m_Dictionary.GetCount()];
    s32 Offset = 0;
    for( i = 0; i < Context().m_Dictionary.GetCount(); i++ )
    {
        StringRemap[i] = Offset;
        Offset += 1 + x_strlen( Context().m_Dictionary.GetString( i ) );
    }

    // Copy strings.
    Geom.m_StringDataSize = Offset;
    Geom.m_pStringData = new char[Geom.m_StringDataSize];
    for( i = 0; i < Context().m_Dictionary.GetCount(); i++ )
    {
        x_strcpy( &Geom.m_pStringData[StringRemap[i]], Context().m_Dictionary.GetString( i ) );
    }

    // Remap texture names.
    for( i = 0; i < Geom.m_nTextures; i++ )
    {
        Geom.m_pTexture[i].DescOffset = StringRemap[Geom.m_pTexture[i].DescOffset];
        Geom.m_pTexture[i].FileNameOffset = StringRemap[Geom.m_pTexture[i].FileNameOffset];
    }

    // Remap geometry names.
    for( i = 0; i < Geom.m_nMeshes; i++ )
    {
        Geom.m_pMesh[i].NameOffset = StringRemap[Geom.m_pMesh[i].NameOffset];
    }

    // Remap virtual-mesh names.
    for( i = 0; i < Geom.m_nVirtualMeshes; i++ )
    {
        Geom.m_pVirtualMeshes[i].NameOffset = StringRemap[Geom.m_pVirtualMeshes[i].NameOffset];
    }

    // Remap virtual-texture names.
    for( i = 0; i < Geom.m_nVirtualTextures; i++ )
    {
        Geom.m_pVirtualTextures[i].NameOffset = StringRemap[Geom.m_pVirtualTextures[i].NameOffset];
    }

    // Remap bone-mask names.
    for( i = 0; i < Geom.m_nBoneMasks; i++ )
    {
        Geom.m_pBoneMasks[i].NameOffset = StringRemap[Geom.m_pBoneMasks[i].NameOffset];
    }

    // Remap rigid-body names.
    for( i = 0; i < Geom.m_nRigidBodies; i++ )
    {
        Geom.m_pRigidBodies[i].NameOffset = StringRemap[Geom.m_pRigidBodies[i].NameOffset];
    }

    // Remap property-section strings.
    for( i = 0; i < Geom.m_nPropertySections; i++ )
    {
        geom::property_section& Section = Geom.m_pPropertySections[i];
        Section.NameOffset = StringRemap[Section.NameOffset];
    }

    // Remap property strings.
    for( i = 0; i < Geom.m_nProperties; i++ )
    {
        geom::property& Prop = Geom.m_pProperties[i];
        Prop.NameOffset = StringRemap[Prop.NameOffset];
        if( Prop.Type == geom::property::TYPE_STRING )
            Prop.Value.StringOffset = StringRemap[Prop.Value.StringOffset];
    }

    // Clean up.
    delete[] StringRemap;
}

//=========================================================================

void geometry_product_writer::PrintSummary( geom& Geom )
{
    s32 i, j, k;

    // Print the summary.
    x_printf( "\n--Mesh Summary--------------------------------\n" );
    x_printf( "%d VMeshes\n", Geom.m_nVirtualMeshes );
    x_printf( "%d Meshes\n", Geom.m_nMeshes );
    x_printf( "%d Verts\n", Geom.m_nVertices );
    x_printf( "%d Textures\n", Geom.m_nTextures );
    x_printf( "%d Materials\n", Geom.m_nMaterials );

    // Print virtual-mesh details.
    x_printf( "\n--VMesh Breakdown-----------------------------\n" );
    for( i = 0; i < Geom.m_nVirtualMeshes; i++ )
    {
        geom::virtual_mesh& VMesh = Geom.m_pVirtualMeshes[i];
        x_printf( "%s\n", Geom.GetVMeshName( i ) );
        for( j = VMesh.iLOD; j < VMesh.iLOD + VMesh.nLODs; j++ )
        {
            u64 LODMask = Geom.m_pLODMasks[j];
            for( k = 0; k < Geom.m_nMeshes; k++ )
            {
                if( LODMask & ( (u64)1 << k ) )
                {
                    x_printf( "    %s\n", Geom.GetMeshName( k ) );
                }
            }
        }
    }

    // Print material details.
    x_printf( "\n--Material Breakdown--------------------------\n" );
    for( i = 0; i < Geom.m_nMaterials; i++ )
    {
        geom::material& Mat = Geom.m_pMaterial[i];
        x_printf( "Material #%d\n", i );
        x_printf( "  nTextures: %d\n", Mat.nTextures );

        s32 iDiffuse = Mat.iTexture;
        s32 iEnvironment = iDiffuse + Mat.nVirtualMats;
        s32 iDetail = iEnvironment + ( ( Mat.Flags & geom::material::FLAG_HAS_ENV_MAP ) ? 1 : 0 );
        for( j = 0; j < Mat.nVirtualMats; j++ )
        {
            x_printf( "  Diffuse: %s\n", Geom.GetTextureName( Mat.iTexture + j ) );
        }
        if( Mat.Flags & geom::material::FLAG_HAS_ENV_MAP )
        {
            x_printf( "  EnvMap: %s\n", Geom.GetTextureName( iEnvironment ) );
        }
        if( Mat.Flags & geom::material::FLAG_HAS_DETAIL_MAP )
        {
            x_printf( "  DetailMap: %s\n", Geom.GetTextureName( iDetail ) );
        }
    }

    x_printf( "\n--Texture Breakdown----------------------------\n" );
    for( i = 0; i < Geom.m_nTextures; i++ )
    {
        geom::texture& Tex = Geom.m_pTexture[i];
        const char* pName = Geom.GetTextureName( i );
        const char* pDesc = Geom.GetTextureDesc( i );
        if( pDesc[0] == '\0' )
            pDesc = "NO DESC";
        x_printf( "Texture %d: Name(%s) Desc(%s)\n", i, pName, pDesc );
    }
}

//=========================================================================
