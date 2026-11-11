//=========================================================================
//
//  GeometryProductWriter_Materials.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"
#include "BitmapProcessing.hpp"
#include "BitmapSource.hpp"
#include "BitmapProduct.hpp"
#include "Model/GeometryNames.hpp"

#include <filesystem>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

const s32 MaxUVKeys = 10000;
const s32 MaxTextureWidth = 512;
const s32 MaxTextureHeight = 512;

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void geometry_product_writer::ExportDiffuse( const resource_tool::bitmap::image& Bitmap, const char* pName,
                                             pref_bpp PrefBPP, s32 nMips )
{
    (void)PrefBPP;
    (void)nMips;
    if( !Context().m_pBitmapSink )
        ThrowError( "Geometry bitmap product sink is not configured" );

    resource_tool::bitmap::product_request Request;
    Request.OutputPath = pName;
    Request.Profile.Format = resource_tool::bitmap::output_format::ARGB8888;
    Request.Profile.Mips = resource_tool::bitmap::mip_policy::Preserve;
    Request.Profile.Alpha = Bitmap.AlphaMode;
    Request.Profile.Color = Bitmap.ColorSpace;
    Request.Role = "geometry-bitmap";
    std::string Error;
    if( !Context().m_pBitmapSink->Write( Request, Bitmap, Error ) )
        ThrowError( xfs( "Unable to write geometry bitmap [%s]: %s", pName, Error.c_str() ) );
}

//=========================================================================

void geometry_product_writer::ExportEnvironment( const resource_tool::bitmap::image& Bitmap, const char* pName )
{
    if( !Context().m_pBitmapSink )
        ThrowError( "Geometry bitmap product sink is not configured" );

    resource_tool::bitmap::product_request Request;
    Request.OutputPath = pName;
    Request.Profile.Format = resource_tool::bitmap::output_format::ARGB8888;
    Request.Profile.Mips = resource_tool::bitmap::mip_policy::Preserve;
    Request.Profile.Alpha = Bitmap.AlphaMode;
    Request.Profile.Color = Bitmap.ColorSpace;
    Request.Role = "geometry-environment-bitmap";
    std::string Error;
    if( !Context().m_pBitmapSink->Write( Request, Bitmap, Error ) )
        ThrowError( xfs( "Unable to write environment bitmap [%s]: %s", pName, Error.c_str() ) );
}

//=========================================================================

void geometry_product_writer::ExportDetail( const resource_tool::bitmap::image& Bitmap, const char* pName )
{
    if( !Context().m_pBitmapSink )
        ThrowError( "Geometry bitmap product sink is not configured" );

    resource_tool::bitmap::product_request Request;
    Request.OutputPath = pName;
    Request.Profile.Format = resource_tool::bitmap::output_format::ARGB8888;
    Request.Profile.Mips = resource_tool::bitmap::mip_policy::Preserve;
    Request.Profile.Alpha = Bitmap.AlphaMode;
    Request.Profile.Color = Bitmap.ColorSpace;
    Request.Role = "geometry-detail-bitmap";
    std::string Error;
    if( !Context().m_pBitmapSink->Write( Request, Bitmap, Error ) )
        ThrowError( xfs( "Unable to write detail bitmap [%s]: %s", pName, Error.c_str() ) );
}

//=========================================================================

void geometry_product_writer::ProcessPunchThruMap( map_info& DiffuseMap, const char* pPunchThruMap )
{
    // Load punch-through map.
    resource_tool::bitmap::image PunchMap;
    std::filesystem::path ResolvedPath;
    std::string Error;
    if( !resource_tool::bitmap::LoadSourceImage( pPunchThruMap, PunchMap, ResolvedPath, Error ) )
    {
        ThrowError( xfs( "Unable to load punch-through map (%s): %s", pPunchThruMap, Error.c_str() ) );
    }

    if( !resource_tool::bitmap::ApplyPunchThrough( DiffuseMap.Bitmap, PunchMap, Error ) )
    {
        ThrowError( xfs( "Unable to process punch-through texture [%s] for diffuse [%s]: %s", pPunchThruMap,
                         (const char*)DiffuseMap.InputBitmapName, Error.c_str() ) );
    }
}

//=========================================================================

void geometry_product_writer::ExportUVAnimation( const geometry_material& RawMat, geom::material& Material, geom& Geom )
{
    geom::material::uvanim& UVAnim = Material.UVAnim;

    x_memset( &UVAnim, 0, sizeof( UVAnim ) );

    // UV keys are normalized by rawgeom import.
    if( RawMat.UVAnimation.Keys.empty() )
        return;

    UVAnim.Type = (s8)RawMat.UVAnimation.Type;
    UVAnim.StartFrame = (s8)RawMat.UVAnimation.StartFrame;
    UVAnim.FPS = (s8)RawMat.UVAnimation.FPS;
    UVAnim.iKey = Geom.m_nUVKeys;
    UVAnim.nKeys = (s32)RawMat.UVAnimation.Keys.size();

    // Copy UV keys.
    for( const geometry_uv_key& Key : RawMat.UVAnimation.Keys )
    {
        if( Geom.m_nUVKeys >= MaxUVKeys )
            ThrowError( "UV animation exceeds the compiled key limit" );
        Geom.m_pUVKey[Geom.m_nUVKeys].OffsetU = Key.U;
        Geom.m_pUVKey[Geom.m_nUVKeys].OffsetV = Key.V;

        Geom.m_nUVKeys++;
    }
}

//=========================================================================

void geometry_product_writer::ReadBitmap( map_info& MapInfo )
{
    std::filesystem::path ResolvedPath;
    std::string Error;
    if( !resource_tool::bitmap::LoadSourceImage( std::filesystem::path( (const char*)MapInfo.InputBitmapName ),
                                                 MapInfo.Bitmap, ResolvedPath, Error ) )
        ThrowError( xfs( "Unable to load texture [%s]: %s", (const char*)MapInfo.InputBitmapName, Error.c_str() ) );
    MapInfo.InputBitmapName = ResolvedPath.generic_string().c_str();

    if( !resource_tool::bitmap::ValidateTextureDimensions( MapInfo.Bitmap, MaxTextureWidth, MaxTextureHeight, TRUE,
                                                           ResolvedPath, Error ) )
        ThrowError( Error.c_str() );
}

//=========================================================================

void geometry_product_writer::ComputeMapNames( geometry_product_writer::map_slot& Map, s32 MapType, u32 CheckSum,
                                               pref_bpp PrefBPP, const char* pInputFile )
{
    // Build the output path.
    char OutputDrive[X_MAX_DRIVE];
    char OutputDir[X_MAX_DIR];
    x_splitpath( Context().m_OutputFile, OutputDrive, OutputDir, NULL, NULL );

    // Strip the path for virtual-texture comparisons.
    char InputDrive[X_MAX_DRIVE];
    char InputDir[X_MAX_DIR];
    char InputFName[X_MAX_FNAME];
    char InputExt[X_MAX_EXT];
    char InputFNameExt[X_MAX_PATH];
    x_splitpath( pInputFile, InputDrive, InputDir, InputFName, InputExt );
    x_makepath( InputFNameExt, NULL, NULL, InputFName, InputExt );

    // Only diffuse slots can use a virtual texture.
    const geometry_virtual_texture* pVirtualTexture = NULL;
    if( MapType == Max_Diffuse1 )
    {
        for( const geometry_virtual_texture& Candidate : Context().m_ActiveAsset.Materials.VirtualTextures )
        {
            // Skip empty virtual textures.
            if( Candidate.Textures.empty() )
                continue;

            // Match the source filename.
            char VTextureDrive[X_MAX_DRIVE];
            char VTextureDir[X_MAX_DIR];
            char VTextureName[X_MAX_FNAME];
            char VTextureExt[X_MAX_EXT];
            char VTextureNameExt[X_MAX_PATH];
            x_splitpath( Candidate.Textures[0].FileName.c_str(), VTextureDrive, VTextureDir, VTextureName,
                         VTextureExt );
            x_makepath( VTextureNameExt, NULL, NULL, VTextureName, VTextureExt );
            if( !x_stricmp( VTextureNameExt, InputFNameExt ) )
            {
                pVirtualTexture = &Candidate;
                break;
            }
        }
    }

    // The first texture comes from the geometry.
    map_info& MapInfo = Map.MapList.Append();

    // Preserve the input name.
    MapInfo.InputBitmapName = pInputFile;

    // Build the compiled-name suffix.
    char NameDecoration[X_MAX_PATH];
    x_strcpy( NameDecoration, "[" );
    if( MapType == Max_DetailMap )
    {
        x_strcat( NameDecoration, "D" );
    }
    else if( MapType == Max_Environment )
    {
        x_strcat( NameDecoration, "E" );
    }
    else
    {
        switch( PrefBPP )
        {
        default:
        case PREF_BPP_32:
            x_strcat( NameDecoration, "32_" );
            break;
        case PREF_BPP_16:
            x_strcat( NameDecoration, "16_" );
            break;
        case PREF_BPP_8:
            x_strcat( NameDecoration, "8_" );
            break;
        case PREF_BPP_4:
            x_strcat( NameDecoration, "4_" );
            break;
        case PREF_BPP_DEFAULT:
            x_strcat( NameDecoration, "8_" );
            break;
        }
    }
    x_strcat( NameDecoration, xfs( "%04X", CheckSum & 0xffff ) );
    x_strcat( NameDecoration, "]" );

    // Build the pathless compiled name.
    char CompiledName[X_MAX_PATH];
    x_makepath( CompiledName, NULL, NULL, xfs( "%s%s", InputFName, NameDecoration ), "xbmp" );
    MapInfo.CompiledBitmapName = CompiledName;

    // Build the output path.
    char OutputName[X_MAX_PATH];
    x_makepath( OutputName, OutputDrive, OutputDir, xfs( "%s%s", InputFName, NameDecoration ), "xbmp" );
    MapInfo.OutputBitmapName = OutputName;

    // Append virtual-texture maps.
    if( pVirtualTexture )
    {
        char Drive[X_MAX_DRIVE];
        char Dir[X_MAX_DIR];
        char FName[X_MAX_FNAME];
        char Ext[X_MAX_EXT];

        // Use the override when the default texture is replaced.
        const geometry_virtual_texture& VTexture = *pVirtualTexture;
        if( VTexture.OverrideDefault )
        {
            if( VTexture.OverrideFileName.empty() )
            {
                ThrowError(
                    xfs( "Diffuse override filename is invalid for virtual texture [%s]", VTexture.Name.c_str() ) );
            }

            // Preserve the override name.
            MapInfo.InputBitmapName = VTexture.OverrideFileName.c_str();

            // Build the pathless compiled name.
            x_splitpath( MapInfo.InputBitmapName, Drive, Dir, FName, Ext );
            x_makepath( CompiledName, NULL, NULL, xfs( "%s%d", FName, CheckSum ), "xbmp" );
            MapInfo.CompiledBitmapName = CompiledName;

            // Build the output path.
            x_makepath( OutputName, OutputDrive, OutputDir, xfs( "%s%d", FName, CheckSum ), "xbmp" );
            MapInfo.OutputBitmapName = OutputName;
        }

        // Add the remaining virtual textures.
        s32 i;
        for( s32 i = 1; i < (s32)VTexture.Textures.size(); ++i )
        {
            const geometry_texture& TexInfo = VTexture.Textures[i];

            // Add this virtual texture.
            map_info& VMapInfo = Map.MapList.Append();

            // Preserve the input name.
            VMapInfo.InputBitmapName = TexInfo.FileName.c_str();

            if( VMapInfo.InputBitmapName[0] == '\0' )
            {
                ThrowError( xfs( "Invalid diffuse map for virtual texture[%s,%d]", VTexture.Name.c_str(), i ) );
            }

            // Build the pathless compiled name.
            x_splitpath( TexInfo.FileName.c_str(), Drive, Dir, FName, Ext );
            x_makepath( CompiledName, NULL, NULL, xfs( "%s%d", FName, CheckSum ), "xbmp" );
            VMapInfo.CompiledBitmapName = CompiledName;

            // Build the output path.
            x_makepath( OutputName, OutputDrive, OutputDir, xfs( "%s%d", FName, CheckSum ), "xbmp" );
            VMapInfo.OutputBitmapName = OutputName;
        }
    }
}

//=========================================================================

void geometry_product_writer::AddTextureMaps( geometry_product_writer::material& Material,
                                              const geometry_material& Source )
{
    if( Source.Textures.empty() )
        return;

    // Rebuild compiled slots from descriptor order.
    Material.Maps[Max_Diffuse1].MapList.Clear();
    Material.Maps[Max_Environment].MapList.Clear();
    Material.Maps[Max_DetailMap].MapList.Clear();

    for( const geometry_texture& Texture : Source.Textures )
    {
        s32 iMap = -1;
        switch( Texture.Slot )
        {
        case GEOMETRY_TEXTURE_DIFFUSE:
            iMap = Max_Diffuse1;
            break;
        case GEOMETRY_TEXTURE_ENVIRONMENT:
            iMap = Max_Environment;
            break;
        case GEOMETRY_TEXTURE_DETAIL:
            iMap = Max_DetailMap;
            break;
        default:
            break;
        }
        if( iMap < 0 )
            continue;

        map_info& Map = Material.Maps[iMap].MapList.Append();
        std::filesystem::path FileName( Texture.FileName );
        if( FileName.is_relative() )
        {
            FileName = std::filesystem::path( Context().m_ActiveAsset.Source.SourceFile ).parent_path() / FileName;
        }
        FileName = FileName.lexically_normal();
        Map.InputBitmapName = FileName.generic_string().c_str();

        char Extension[X_MAX_EXT];
        x_splitpath( Texture.FileName.c_str(), NULL, NULL, NULL, Extension );
        if( x_stricmp( Extension, ".xbmp" ) == 0 )
        {
            Map.InputBitmapName = FileName.filename().generic_string().c_str();
            Map.CompiledBitmapName = Map.InputBitmapName;
            Map.bPassThrough = TRUE;
            continue;
        }

        Map.bPassThrough = FALSE;

        char OutputDrive[X_MAX_DRIVE];
        char OutputDirectory[X_MAX_DIR];
        x_splitpath( Context().m_OutputFile, OutputDrive, OutputDirectory, NULL, NULL );

        char InputDrive[X_MAX_DRIVE];
        char InputDirectory[X_MAX_DIR];
        char InputName[X_MAX_FNAME];
        x_splitpath( FileName.generic_string().c_str(), InputDrive, InputDirectory, InputName, Extension );

        const char* pDecoration = "[8_0000]";
        if( iMap == Max_Environment )
            pDecoration = "[E0000]";
        else if( iMap == Max_DetailMap )
            pDecoration = "[D0000]";

        char ProductName[X_MAX_PATH];
        x_strcpy( ProductName, InputName );
        x_strcat( ProductName, pDecoration );

        char CompiledName[X_MAX_PATH];
        x_makepath( CompiledName, NULL, NULL, ProductName, "xbmp" );
        Map.CompiledBitmapName = CompiledName;

        char OutputName[X_MAX_PATH];
        x_makepath( OutputName, OutputDrive, OutputDirectory, ProductName, "xbmp" );
        Map.OutputBitmapName = OutputName;
    }
}

//=========================================================================

void geometry_product_writer::FillMapSlots( mesh& Mesh )
{
    s32 i, j;
    for( i = 0; i < Mesh.Material.GetCount(); i++ )
    {
        // Get the source material.
        material& Mat = Mesh.Material[i];
        const geometry_asset& RawMesh = Context().m_ActiveAsset;
        const geometry_material& RawMat = RawMesh.Materials.Items[Mat.iAssetMaterial];

        // Check environment-map usage.
        xbool bNeedsEnviroment = FALSE;

        switch( RawMat.Type )
        {
        case Material_Diff:
        case Material_Alpha:
        case Material_Diff_PerPixelIllum:
        case Material_Alpha_PerPixelIllum:
        case Material_Alpha_PerPolyIllum:
        case Material_Distortion:
            break;

        case Material_Distortion_PerPolyEnv:
            bNeedsEnviroment = TRUE;
            break;

        case Material_Diff_PerPixelEnv:
        case Material_Alpha_PerPolyEnv:
            if( RawMat.Constants[ENV_TYPE].Current[0] != 0.0f )
            {
                bNeedsEnviroment = TRUE;
            }
            break;

        default:
            ThrowError( xfs( "Unknown Material Type %d", RawMat.Type ) );
            break;
        }

        // Check for punch-through.
        xbool bHasPunchThrough = ( RawMat.Map[Max_PunchThrough].iTexture ) != -1;

        // Resolve texture names.
        for( j = 0; j < NumMaps; j++ )
        {
            // Skip unsupported map slots.
            if( ( j == Max_Diffuse2 ) || ( j == Max_Blend ) || ( j == Max_LightMap ) || ( j == Max_Opacity ) ||
                ( j == Max_Intensity ) || ( j == Max_SelfIllumination ) || ( j == Max_PunchThrough ) )
            {
                continue;
            }

            // Get the texture index.
            s32 iTexture = RawMat.Map[j].iTexture;

            // Validate the texture index.
            if( iTexture >= (s32)RawMesh.Materials.Textures.size() )
                ThrowError(
                    xfs( "Invalid texture index %d (max %d)\n", iTexture, (s32)RawMesh.Materials.Textures.size() ) );

            // Skip unused maps.
            if( iTexture < 0 )
                continue;

            // Skip an unused environment map.
            if( !bNeedsEnviroment && ( j == Max_Environment ) )
            {
                Mat.Maps[j].MapList.Clear();
                continue;
            }

            // Validate the texture path.
            const char* pFileName = RawMesh.Materials.Textures[iTexture].FileName.c_str();
            xbool bIsCompiledTexture = FALSE;
            for( const geometry_material& Candidate : Context().m_ActiveAsset.Materials.Items )
            {
                for( const geometry_texture& ProductTexture : Candidate.Textures )
                {
                    if( !x_stricmp( ProductTexture.FileName.c_str(), pFileName ) )
                    {
                        bIsCompiledTexture = TRUE;
                        break;
                    }
                }
                if( bIsCompiledTexture )
                    break;
            }
            if( !std::filesystem::path( pFileName ).is_absolute() &&
                x_stristr( pFileName, Context().m_TexturePath ) == 0 && !bIsCompiledTexture )
            {
                ThrowError( xfs( "Texture [%s] is not in path [%s]", pFileName, Context().m_TexturePath ) );
            }

            // Add a checksum when punch-through is baked into diffuse output.
            u32 CheckSum = 0;
            if( ( j == Max_Diffuse1 ) && bHasPunchThrough )
            {
                s32 Index = RawMat.Map[Max_PunchThrough].iTexture;
                char PathName[256];
                x_strcpy( PathName, RawMesh.Materials.Textures[Index].FileName.c_str() );
                x_strtoupper( PathName );
                CheckSum += x_chksum( PathName, x_strlen( PathName ) );
            }

            // Fill in source and output names.
            Mat.Maps[j].MapList.Clear();
            if( j == Max_Diffuse1 )
            {
                ComputeMapNames( Mat.Maps[j], j, CheckSum, Mat.TexInfo.PreferredBPP,
                                 RawMesh.Materials.Textures[iTexture].FileName.c_str() );
            }
            else
            {
                ComputeMapNames( Mat.Maps[j], j, CheckSum, Mat.TexInfo.PreferredBPP,
                                 RawMesh.Materials.Textures[iTexture].FileName.c_str() );
            }
        }

        // Preserve references to precompiled XBMPs.
        AddTextureMaps( Mat, RawMat );

        // Process diffuse maps.
        if( Mat.Maps[Max_Diffuse1].MapList.GetCount() == 0 )
        {
            // Keep texture-less materials; the renderer supplies the default colour.
        }
        else
        {
            for( j = 0; j < Mat.Maps[Max_Diffuse1].MapList.GetCount(); j++ )
            {
                map_info& MapInfo = Mat.Maps[Max_Diffuse1].MapList[j];
                if( MapInfo.bPassThrough )
                    continue;
                ReadBitmap( MapInfo );

                if( bHasPunchThrough )
                {
                    s32 Index = RawMat.Map[Max_PunchThrough].iTexture;
                    ProcessPunchThruMap( MapInfo, RawMesh.Materials.Textures[Index].FileName.c_str() );
                }

                ExportDiffuse( MapInfo.Bitmap, MapInfo.OutputBitmapName, Mat.TexInfo.PreferredBPP,
                               Mat.TexInfo.nMipsToBuild );
            }
        }

        // Process the environment map.
        if( bNeedsEnviroment )
        {
            if( Mat.Maps[Max_Environment].MapList.GetCount() == 0 )
            {
                ThrowError( "No environment map specified!" );
            }
            else
            {
                map_info& MapInfo = Mat.Maps[Max_Environment].MapList[0];
                if( MapInfo.bPassThrough )
                    continue;
                ReadBitmap( MapInfo );
                ExportEnvironment( MapInfo.Bitmap, MapInfo.OutputBitmapName );
            }
        }

        // Process the detail map.
        if( Mat.Maps[Max_DetailMap].MapList.GetCount() )
        {
            map_info& MapInfo = Mat.Maps[Max_DetailMap].MapList[0];
            if( MapInfo.bPassThrough )
                continue;
            ReadBitmap( MapInfo );
            std::string Error;
            if( !resource_tool::bitmap::ApplyDetailMap( MapInfo.Bitmap, Error ) )
                ThrowError( xfs( "Unable to process detail bitmap [%s]: %s", (const char*)MapInfo.InputBitmapName,
                                 Error.c_str() ) );
            ExportDetail( MapInfo.Bitmap, MapInfo.OutputBitmapName );
        }
    }
}

//=========================================================================

void geometry_product_writer::ExportMaterial( mesh& Mesh, geom& Geom )
{
    s32 i, j, k;

    // Build compiled bitmaps and material slots.
    FillMapSlots( Mesh );

    // Allocate materials.
    Geom.m_nMaterials = Mesh.Material.GetCount();
    Geom.m_pMaterial = new geom::material[Geom.m_nMaterials];

    // Allocate UV animation storage.
    Geom.m_nUVKeys = 0;
    Geom.m_pUVKey = new geom::uvkey[MaxUVKeys];

    // Build materials.
    s32 nTotalTextures = 0;
    s32 nTotalVMats = 0;
    for( i = 0; i < Mesh.Material.GetCount(); i++ )
    {
        material& MeshMat = Mesh.Material[i];
        const geometry_material& RawMat = Context().m_ActiveAsset.Materials.Items[MeshMat.iAssetMaterial];
        geom::material& Mat = Geom.m_pMaterial[i];

        // Fill basic material data.
        Mat.DetailScale = RawMat.Constants[DETAIL_SCALE].Current[0];
        Mat.FixedAlpha = RawMat.Constants[FIXED_ALPHA].Current[0];
        Mat.Flags = 0;
        Mat.Type = RawMat.Type;
        Mat.nTextures = MeshMat.Maps[Max_Diffuse1].MapList.GetCount() +
                        MeshMat.Maps[Max_Environment].MapList.GetCount() +
                        MeshMat.Maps[Max_DetailMap].MapList.GetCount();
        Mat.iTexture = nTotalTextures;
        Mat.nVirtualMats = MeshMat.Maps[Max_Diffuse1].MapList.GetCount();
        Mat.iVirtualMat = nTotalVMats;
        if( RawMat.bTwoSided )
            Mat.Flags |= geom::material::FLAG_DOUBLE_SIDED;
        if( MeshMat.Maps[Max_Environment].MapList.GetCount() )
            Mat.Flags |= geom::material::FLAG_HAS_ENV_MAP;
        if( MeshMat.Maps[Max_DetailMap].MapList.GetCount() )
            Mat.Flags |= geom::material::FLAG_HAS_DETAIL_MAP;
        if( RawMat.Constants[ENV_TYPE].Current[0] == 0.0f )
            Mat.Flags |= geom::material::FLAG_ENV_CUBE_MAP;
        else if( RawMat.Constants[ENV_TYPE].Current[0] == 1.0f )
            Mat.Flags |= geom::material::FLAG_ENV_VIEW_SPACE;
        else
            Mat.Flags |= geom::material::FLAG_ENV_WORLD_SPACE;
        if( RawMat.Constants[FORCE_ZFILL].Current[0] )
            Mat.Flags |= geom::material::FLAG_FORCE_ZFILL;
        if( RawMat.Constants[USE_DIFFUSE].Current[0] )
            Mat.Flags |= geom::material::FLAG_ILLUM_USES_DIFFUSE;
        // Preserve the baked punch-through flag.
        if( RawMat.bPunchthrough || RawMat.Map[Max_PunchThrough].iTexture >= 0 )
            Mat.Flags |= geom::material::FLAG_IS_PUNCH_THRU;
        if( RawMat.Constants[ENV_BLEND].Current[0] == 1.0f )
            Mat.Flags |= geom::material::FLAG_IS_ADDITIVE;
        else if( RawMat.Constants[ENV_BLEND].Current[0] == 2.0f )
            Mat.Flags |= geom::material::FLAG_IS_SUBTRACTIVE;

        // Copy UV animation data.
        ExportUVAnimation( RawMat, Mat, Geom );

        // Update totals.
        nTotalTextures += Mat.nTextures;
        nTotalVMats += Mat.nVirtualMats;
    }

    // Allocate virtual materials.
    Geom.m_nVirtualMaterials = nTotalVMats;

    // Allocate textures.
    Geom.m_nTextures = nTotalTextures;
    Geom.m_pTexture = new geom::texture[nTotalTextures];

    // Set texture names.
    nTotalTextures = 0;
    for( i = 0; i < Mesh.Material.GetCount(); i++ )
    {
        material& MeshMat = Mesh.Material[i];
        geom::material& Mat = Geom.m_pMaterial[i];

        // Set diffuse textures.
        for( j = 0; j < Mat.nVirtualMats; j++ )
        {
            Geom.m_pTexture[nTotalTextures].DescOffset = Context().m_Dictionary.Add( "" );
            Geom.m_pTexture[nTotalTextures].FileNameOffset =
                Context().m_Dictionary.Add( MeshMat.Maps[Max_Diffuse1].MapList[j].CompiledBitmapName );
            nTotalTextures++;
        }

        // Set the environment map.
        if( MeshMat.Maps[Max_Environment].MapList.GetCount() )
        {
            Geom.m_pTexture[nTotalTextures].DescOffset = Context().m_Dictionary.Add( "" );
            Geom.m_pTexture[nTotalTextures].FileNameOffset =
                Context().m_Dictionary.Add( MeshMat.Maps[Max_Environment].MapList[0].CompiledBitmapName );
            nTotalTextures++;
        }

        // Set the detail map.
        if( MeshMat.Maps[Max_DetailMap].MapList.GetCount() )
        {
            Geom.m_pTexture[nTotalTextures].DescOffset = Context().m_Dictionary.Add( "" );
            Geom.m_pTexture[nTotalTextures].FileNameOffset =
                Context().m_Dictionary.Add( MeshMat.Maps[Max_DetailMap].MapList[0].CompiledBitmapName );
            nTotalTextures++;
        }
    }
    ASSERT( nTotalTextures == Geom.m_nTextures );

    // Build virtual textures from materials.
    xarray<geom::virtual_texture> VirtualTextures;
    VirtualTextures.Clear();
    for( const geometry_virtual_texture& VTexture : Context().m_ActiveAsset.Materials.VirtualTextures )
    {
        u32 MatMask = 0;

        if( VTexture.Textures.empty() )
            continue;

        // Get the source filename.
        char RscFileName[X_MAX_FNAME];
        x_splitpath( VTexture.Textures[0].FileName.c_str(), NULL, NULL, RscFileName, NULL );

        // Find materials using this virtual texture.
        for( j = 0; j < Geom.m_nMaterials; j++ )
        {
            material& MeshMat = Mesh.Material[j];
            if( ( MeshMat.Maps[Max_Diffuse1].MapList.GetCount() > 1 ) && ( VTexture.Textures.size() > 1 ) )
            {
                char MapFileName[X_MAX_FNAME];
                x_splitpath( MeshMat.Maps[Max_Diffuse1].MapList[0].InputBitmapName, NULL, NULL, MapFileName, NULL );
                if( !x_stricmp( RscFileName, MapFileName ) )
                {
                    // Found a material match.
                    MatMask |= ( 1 << j );

                    // Validate the texture count.
                    if( VTexture.Textures.size() != (usize)MeshMat.Maps[Max_Diffuse1].MapList.GetCount() )
                    {
                        ThrowError(
                            "Virtual texture count mismatch. Two virtual textures effecting the same diffuse?" );
                    }

                    // Copy descriptive texture names.
                    for( k = 0; k < (s32)VTexture.Textures.size(); k++ )
                    {
                        Geom.m_pTexture[Geom.m_pMaterial[j].iTexture + k].DescOffset =
                            Context().m_Dictionary.Add( VTexture.Textures[k].Name.c_str() );
                    }
                }
            }
        }

        // Add used virtual textures to the geometry.
        if( MatMask != 0 )
        {
            geom::virtual_texture& GeomVTex = VirtualTextures.Append();
            GeomVTex.MaterialMask = MatMask;
            GeomVTex.NameOffset = Context().m_Dictionary.Add( VTexture.Name.c_str() );
        }
    }

    // Build the virtual-texture array.
    Geom.m_nVirtualTextures = VirtualTextures.GetCount();
    Geom.m_pVirtualTextures = new geom::virtual_texture[Geom.m_nVirtualTextures];
    for( i = 0; i < Geom.m_nVirtualTextures; i++ )
    {
        Geom.m_pVirtualTextures[i] = VirtualTextures[i];
    }

#ifdef X_DEBUG
    for( i = 0; i < Geom.m_nMaterials; i++ )
    {
        geom::material& GeomMat = Geom.m_pMaterial[i];
        ASSERT( GeomMat.nTextures == 0 || ( ( GeomMat.iTexture >= 0 ) && ( GeomMat.iTexture < Geom.m_nTextures ) ) );
    }
#endif
}

//=========================================================================

void geometry_product_writer::ApplyTextureDescriptors( geom& Geom, const geometry_asset& Asset,
                                                       const s32* pAssetMaterialIndices, s32 nAssetMaterialIndices )
{
    // Apply semantic texture descriptors when available.
    for( s32 iMaterial = 0; iMaterial < Geom.m_nMaterials; ++iMaterial )
    {
        if( !pAssetMaterialIndices || iMaterial >= nAssetMaterialIndices )
            ThrowError( "Compiled material has no source material mapping" );

        const s32 iAssetMaterial = pAssetMaterialIndices[iMaterial];
        if( iAssetMaterial < 0 || iAssetMaterial >= (s32)Asset.Materials.Items.size() )
        {
            ThrowError( "Compiled material has an invalid source material mapping" );
        }

        const geometry_material& Source = Asset.Materials.Items[iAssetMaterial];
        if( Source.Textures.empty() )
            continue;

        const geom::material& Material = Geom.m_pMaterial[iMaterial];
        if( Material.iTexture < 0 || Material.nTextures <= 0 )
            continue;

        const xbool bEnvironmentCompiled = ( Material.Flags & geom::material::FLAG_HAS_ENV_MAP ) != 0;
        s32 EnvironmentIndex = -1;
        s32 DetailIndex = -1;
        for( s32 iTexture = 0; iTexture < (s32)Source.Textures.size(); ++iTexture )
        {
            const geometry_texture& Texture = Source.Textures[iTexture];
            if( Texture.Slot == GEOMETRY_TEXTURE_ENVIRONMENT )
                EnvironmentIndex = iTexture;
            else if( Texture.Slot == GEOMETRY_TEXTURE_DETAIL )
                DetailIndex = iTexture;
        }

        s32 DiffuseIndex = 0;
        for( s32 iTexture = 0; iTexture < (s32)Source.Textures.size(); ++iTexture )
        {
            const geometry_texture& Texture = Source.Textures[iTexture];
            s32 OutputIndex = -1;
            if( Texture.Slot == GEOMETRY_TEXTURE_DIFFUSE )
            {
                OutputIndex = DiffuseIndex++;
            }
            else if( Texture.Slot == GEOMETRY_TEXTURE_ENVIRONMENT && iTexture == EnvironmentIndex &&
                     bEnvironmentCompiled )
            {
                OutputIndex = Material.nVirtualMats;
            }
            else if( Texture.Slot == GEOMETRY_TEXTURE_DETAIL && iTexture == DetailIndex )
            {
                OutputIndex = Material.nVirtualMats + ( bEnvironmentCompiled ? 1 : 0 );
            }

            if( OutputIndex >= 0 && OutputIndex < Material.nTextures )
            {
                Geom.m_pTexture[Material.iTexture + OutputIndex].DescOffset =
                    Context().m_Dictionary.Add( Texture.Name.c_str() );
            }
        }
    }
}

//=========================================================================

void geometry_product_writer::ExportVirtualMeshes( const geometry_asset& Asset, geom& Geom )
{
    Geom.m_nVirtualMeshes = (s32)Asset.LODs.Groups.size();
    Geom.m_pVirtualMeshes = NULL;
    Geom.m_nLODs = 0;
    Geom.m_pLODSizes = NULL;
    Geom.m_pLODMasks = 0;

    if( Geom.m_nVirtualMeshes )
    {
        // Count required LODs.
        s32 i, j, k;
        for( i = 0; i < Geom.m_nVirtualMeshes; i++ )
        {
            Geom.m_nLODs += (s32)Asset.LODs.Groups[i].Levels.size();
        }

        // Allocate virtual-mesh data.
        Geom.m_pLODSizes = new u16[Geom.m_nLODs];
        Geom.m_pLODMasks = new u64[Geom.m_nLODs];
        Geom.m_pVirtualMeshes = new geom::virtual_mesh[Geom.m_nVirtualMeshes];

        // Fill virtual meshes.
        s32 nLODsAdded = 0;
        for( i = 0; i < Geom.m_nVirtualMeshes; i++ )
        {
            const geometry_lod_group& VMesh = Asset.LODs.Groups[i];
            geom::virtual_mesh& DstMesh = Geom.m_pVirtualMeshes[i];
            DstMesh.iLOD = nLODsAdded;
            DstMesh.nLODs = (s32)VMesh.Levels.size();
            DstMesh.NameOffset = Context().m_Dictionary.Add( VMesh.Name.c_str() );
            nLODsAdded += (s32)VMesh.Levels.size();

            // Add LODs.
            for( j = 0; j < (s32)VMesh.Levels.size(); j++ )
            {
                const geometry_lod_level& SrcLOD = VMesh.Levels[j];
                Geom.m_pLODSizes[DstMesh.iLOD + j] = (u16)SrcLOD.ScreenSize;
                Geom.m_pLODMasks[DstMesh.iLOD + j] = 0;
                if( !SrcLOD.MeshNames.empty() )
                {
                    for( k = 0; k < (s32)SrcLOD.MeshNames.size(); ++k )
                    {
                        s32 MeshId;
                        for( MeshId = 0; MeshId < Geom.m_nMeshes; MeshId++ )
                        {
                            if( geometry_names_match(
                                    Context().m_Dictionary.GetString( Geom.m_pMesh[MeshId].NameOffset ),
                                    SrcLOD.MeshNames[k].c_str() ) )
                            {
                                Geom.m_pLODMasks[DstMesh.iLOD + j] |= ( (u64)1 << MeshId );
                                break;
                            }
                        }
                        if( MeshId == Geom.m_nMeshes )
                        {
                            ReportWarning( xfs( "VMesh (%s) refers to mesh (%s) which isn't present in the geometry",
                                                VMesh.Name.c_str(), SrcLOD.MeshNames[k].c_str() ) );
                        }
                    }
                    continue;
                }
                if( SrcLOD.MeshNames.empty() )
                {
                    x_throw( xfs( "VMesh (%s) LOD %d has no mesh names", VMesh.Name.c_str(), j ) );
                }
            }
        }
    }
}
