//=========================================================================
//
//  GeometryModule.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "GeometryModule.hpp"
#include "GeometryCompileJob.hpp"
#include "GeometryDecompileJob.hpp"
#include "GeometrySource.hpp"
#include "GeometryDescriptor.hpp"
#include "ResourcePipeline.hpp"
#include "DependencyGraph.hpp"
#include "ArtifactManifest.hpp"
#include "Diagnostics.hpp"
#include "OutputTransaction.hpp"
#include "BitmapProduct.hpp"
#include "GeometryProductReader.hpp"

#include <filesystem>
#include <algorithm>
#include <memory>
#include <string>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

std::filesystem::path MakePathAbsolute( const char* pFileName, const std::filesystem::path& BaseDirectory )
{
    std::filesystem::path Path( pFileName ? pFileName : "" );
    if( Path.is_relative() )
        Path = BaseDirectory / Path;
    return Path.lexically_normal();
}

//=========================================================================

xbool SamePath( const std::filesystem::path& A, const std::filesystem::path& B )
{
    return std::filesystem::absolute( A ).lexically_normal() == std::filesystem::absolute( B ).lexically_normal();
}

//=========================================================================

void SetResolvedDescriptorPaths( geometry_descriptor& Settings, const std::filesystem::path& DescriptorPath )
{
    const std::filesystem::path BaseDirectory = DescriptorPath.parent_path();

    const std::filesystem::path SourcePath = MakePathAbsolute( Settings.GetSourceFileName(), BaseDirectory );
    Settings.SetSourceFileName( SourcePath.generic_string().c_str() );

    if( Settings.GetOutputFile()[0] )
    {
        const std::filesystem::path OutputPath = MakePathAbsolute( Settings.GetOutputFile(), BaseDirectory );
        Settings.SetOutputFile( OutputPath.generic_string().c_str() );
    }
    if( Settings.GetGeneratedRawGeom()[0] )
    {
        const std::filesystem::path RawGeomPath = MakePathAbsolute( Settings.GetGeneratedRawGeom(), BaseDirectory );
        Settings.SetGeneratedRawGeom( RawGeomPath.generic_string().c_str() );
    }
    if( Settings.GetSkeletonSource()[0] )
    {
        const std::filesystem::path SkeletonPath = MakePathAbsolute( Settings.GetSkeletonSource(), BaseDirectory );
        Settings.SetSkeletonSource( SkeletonPath.generic_string().c_str() );
    }
    if( Settings.GetPhysicsSource()[0] )
    {
        const std::filesystem::path PhysicsPath = MakePathAbsolute( Settings.GetPhysicsSource(), BaseDirectory );
        Settings.SetPhysicsSource( PhysicsPath.generic_string().c_str() );
    }
    if( Settings.GetCollisionSource()[0] )
    {
        const std::filesystem::path CollisionPath = MakePathAbsolute( Settings.GetCollisionSource(), BaseDirectory );
        Settings.SetCollisionSource( CollisionPath.generic_string().c_str() );
    }
}

//=========================================================================

void ValidateCoordinateSystem( const geometry_descriptor& Settings )
{
    const char* pCoordinateSystem = Settings.GetCoordinateSystem();
    if( pCoordinateSystem[0] && x_stricmp( pCoordinateSystem, "GltfNative" ) != 0 )
    {
        x_throw( xfs( "Unsupported geometry coordinate system [%s]", pCoordinateSystem ) );
    }
    if( Settings.GetUnitScale() <= 0.0f )
        x_throw( "Geometry UnitScale must be greater than zero" );
}

//=========================================================================

void ValidateSkeletonSource( const geometry_descriptor& Settings )
{
    if( Settings.GetType() != geometry_descriptor::GEOMETRY_TYPE_SKIN )
        return;

    if( !Settings.GetSkeletonSource()[0] )
        x_throw( "Skin geometry requires ResDesc\\SkeletonSource pointing to a compiled .ANIM" );

    const std::filesystem::path SkeletonPath( Settings.GetSkeletonSource() );
    if( x_stricmp( SkeletonPath.extension().string().c_str(), ".anim" ) != 0 )
        x_throw( "Skin geometry SkeletonSource must be a compiled .ANIM" );

    std::error_code ErrorCode;
    if( !std::filesystem::is_regular_file( SkeletonPath, ErrorCode ) || ErrorCode )
        x_throw( xfs( "Skin geometry skeleton source does not exist [%s]", SkeletonPath.generic_string().c_str() ) );
}

//=========================================================================

void LoadResolvedSettings( const resource_tool::resource_context& Context, geometry_descriptor& Settings )
{
    const std::string DescriptorFile = Context.GetInputPath().generic_string();
    Settings.LoadDescriptor( DescriptorFile.c_str() );
    SetResolvedDescriptorPaths( Settings, Context.GetInputPath() );
    ValidateCoordinateSystem( Settings );
    ValidateSkeletonSource( Settings );
}

//=========================================================================

void AddDescriptorDependency( resource_tool::resource_context& Context, const std::filesystem::path& DescriptorPath,
                              const char* pPath )
{
    if( !pPath || !pPath[0] )
        return;

    Context.Dependencies().Add( resource_tool::dependency_kind::Source,
                                MakePathAbsolute( pPath, DescriptorPath.parent_path() ) );
}

//=========================================================================

void RegisterDescriptorDependencies( resource_tool::resource_context& Context, const geometry_descriptor& Settings )
{
    AddDescriptorDependency( Context, Context.GetInputPath(), Settings.GetSourceFileName() );
    AddDescriptorDependency( Context, Context.GetInputPath(), Settings.GetSkeletonSource() );
    AddDescriptorDependency( Context, Context.GetInputPath(), Settings.GetPhysicsSource() );
    AddDescriptorDependency( Context, Context.GetInputPath(), Settings.GetCollisionSource() );
    for( s32 iVirtualTexture = 0; iVirtualTexture < Settings.GetVirtualTextureCount(); ++iVirtualTexture )
    {
        const geometry_descriptor::virtual_texture& VirtualTexture = Settings.GetVirtualTexture( iVirtualTexture );
        for( s32 iTexture = 0; iTexture < VirtualTexture.Textures.GetCount(); ++iTexture )
        {
            AddDescriptorDependency( Context, Context.GetInputPath(), VirtualTexture.Textures[iTexture].FileName );
        }
    }
    for( s32 iMaterial = 0; iMaterial < Settings.GetMaterialCount(); ++iMaterial )
    {
        const geometry_descriptor::material& Material = Settings.GetMaterial( iMaterial );
        for( s32 iTexture = 0; iTexture < Material.Textures.GetCount(); ++iTexture )
        {
            AddDescriptorDependency( Context, Context.GetInputPath(), Material.Textures[iTexture].FileName );
        }
    }
}

//=========================================================================

void RegisterSourceTextureDependencies( resource_tool::resource_context& Context, const geometry_descriptor& Settings )
{
    if( !geometry_source::IsSourceFile( Settings.GetSourceFileName() ) )
        return;

    geometry_asset Asset;
    std::string Error;
    if( !geometry_source::LoadMesh( Settings.GetSourceFileName(), Asset, geometry_source::ASSET_ROLE_RENDER,
                                    Settings.GetUnitScale(), Error ) )
    {
        x_throw( Error.c_str() );
    }

    for( const geometry_texture& Texture : Asset.Materials.Textures )
    {
        if( !Texture.FileName.empty() )
        {
            Context.Dependencies().Add( resource_tool::dependency_kind::Source, Texture.FileName );
        }
    }
}

//=========================================================================

void RegisterTextureDependencies( resource_tool::resource_context& Context, const geometry_descriptor& Settings )
{
    if( geometry_source::IsSourceFile( Settings.GetSourceFileName() ) )
        return;

    resource_tool::geometry_product_format::geometry_product_snapshot Snapshot;
    resource_tool::geometry_product_format::geometry_product_reader Reader;
    std::string Error;
    const resource_tool::geometry_product_format::geometry_product_kind Kind =
        Settings.GetType() == geometry_descriptor::GEOMETRY_TYPE_RIGID
            ? resource_tool::geometry_product_format::geometry_product_kind::Rigid
            : resource_tool::geometry_product_format::geometry_product_kind::Skin;
    if( !Reader.Load( Settings.GetSourceFileName(), Kind, Snapshot, Error ) )
        x_throw( Error.c_str() );

    const std::filesystem::path ProductPath = Settings.GetSourceFileName();
    for( s32 iTexture = 0; iTexture < Snapshot.TextureCount(); ++iTexture )
    {
        const char* pTextureName = Snapshot.TextureName( iTexture );
        if( !pTextureName || !pTextureName[0] )
            continue;

        AddDescriptorDependency( Context, ProductPath, pTextureName );
    }
}

//=========================================================================

xbool ResolveGeometryProductType( const std::filesystem::path& InputPath, geometry_descriptor::geometry_type& Type )
{
    const std::string Extension = InputPath.extension().string();
    if( x_stricmp( Extension.c_str(), ".rigidgeom" ) == 0 )
    {
        Type = geometry_descriptor::GEOMETRY_TYPE_RIGID;
        return TRUE;
    }
    if( x_stricmp( Extension.c_str(), ".skingeom" ) == 0 )
    {
        Type = geometry_descriptor::GEOMETRY_TYPE_SKIN;
        return TRUE;
    }
    Type = geometry_descriptor::GEOMETRY_TYPE_NONE;
    return FALSE;
}

//=========================================================================

void BuildProductDecompileSettings( const std::filesystem::path& InputPath, const std::filesystem::path& AnimationPath,
                                    geometry_descriptor& Settings )
{
    geometry_descriptor::geometry_type Type;
    if( !ResolveGeometryProductType( InputPath, Type ) )
        x_throw( xfs( "Unsupported compiled geometry product [%s]", InputPath.generic_string().c_str() ) );

    std::filesystem::path GeometryOutput = InputPath;
    GeometryOutput.replace_extension( ".gltf" );
    std::filesystem::path RawGeomOutput = InputPath;
    RawGeomOutput.replace_extension( ".rawgeom" );

    Settings.SetSourceFileName( InputPath.generic_string().c_str() );
    Settings.SetOutputFile( GeometryOutput.generic_string().c_str() );
    Settings.SetGeneratedRawGeom( RawGeomOutput.generic_string().c_str() );
    Settings.SetType( Type );
    if( Type == geometry_descriptor::GEOMETRY_TYPE_SKIN )
        Settings.SetSkeletonSource( AnimationPath.generic_string().c_str() );
}

//=========================================================================

std::filesystem::path ResolveDecompileSkeleton( const resource_tool::resource_context& Context,
                                                geometry_descriptor::geometry_type Type )
{
    if( Type != geometry_descriptor::GEOMETRY_TYPE_SKIN )
        return std::filesystem::path();

    const std::filesystem::path& Requested = Context.GetRequest().AnimationPath;
    if( Requested.empty() )
        x_throw( "Skin geometry decompile requires --animation <compiled .ANIM>" );

    std::filesystem::path Result = Requested;
    if( Result.is_relative() )
        Result = std::filesystem::absolute( Result );
    Result = Result.lexically_normal();
    if( x_stricmp( Result.extension().string().c_str(), ".anim" ) != 0 )
        x_throw( "Skin geometry decompile requires a compiled .ANIM skeleton source" );

    std::error_code ErrorCode;
    if( !std::filesystem::is_regular_file( Result, ErrorCode ) || ErrorCode )
        x_throw( xfs( "Skin geometry skeleton source does not exist [%s]", Result.generic_string().c_str() ) );
    return Result;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

namespace resource_tool::geometry
{

//=========================================================================

xbool module::CanHandle( const resource_tool::resource_request& Request ) const
{
    const std::string Extension = Request.InputPath.extension().string();
    if( Request.Operation == resource_tool::operation::Compile )
        return x_stricmp( Extension.c_str(), ".rawgeom" ) == 0;

    geometry_descriptor::geometry_type Type;
    return ResolveGeometryProductType( Request.InputPath, Type );
}

//=========================================================================

xbool module::DiscoverDependencies( resource_tool::resource_context& Context )
{
    if( Context.GetRequest().Operation == resource_tool::operation::Decompile )
    {
        geometry_descriptor::geometry_type Type;
        if( !ResolveGeometryProductType( Context.GetInputPath(), Type ) )
            x_throw( "Unsupported compiled geometry product" );
        Context.Dependencies().Add( resource_tool::dependency_kind::Source, Context.GetInputPath() );
        const std::filesystem::path AnimationPath = ResolveDecompileSkeleton( Context, Type );
        if( !AnimationPath.empty() )
            Context.Dependencies().Add( resource_tool::dependency_kind::Source, AnimationPath );
        return TRUE;
    }

    geometry_descriptor Settings;
    LoadResolvedSettings( Context, Settings );
    Context.Dependencies().Add( resource_tool::dependency_kind::Source, Context.GetInputPath() );
    RegisterDescriptorDependencies( Context, Settings );
    RegisterSourceTextureDependencies( Context, Settings );
    RegisterTextureDependencies( Context, Settings );
    return TRUE;
}

//=========================================================================

xbool module::Execute( resource_tool::resource_context& Context )
{
    if( Context.GetRequest().Operation == resource_tool::operation::Compile )
        return Compile( Context );
    return Decompile( Context );
}

//=========================================================================

xbool module::Compile( resource_tool::resource_context& Context )
{
    geometry_descriptor Settings;
    LoadResolvedSettings( Context, Settings );
    if( !Settings.GetOutputFile()[0] )
        x_throw( "rawgeom descriptor does not contain OutputFile" );

    const std::filesystem::path FinalOutputFile = Settings.GetOutputFile();
    const std::filesystem::path FinalRawGeomFile = Settings.GetGeneratedRawGeom()[0]
                                                       ? std::filesystem::path( Settings.GetGeneratedRawGeom() )
                                                       : std::filesystem::path();
    resource_tool::output_transaction& Transaction = Context.Transaction();
    const std::filesystem::path StagedOutputFile = Transaction.Stage( FinalOutputFile );
    Settings.SetOutputFile( StagedOutputFile.generic_string().c_str() );
    if( !FinalRawGeomFile.empty() )
    {
        const std::filesystem::path StagedRawGeomFile = Transaction.Stage( FinalRawGeomFile );
        Settings.SetGeneratedRawGeom( StagedRawGeomFile.generic_string().c_str() );
    }
    std::unique_ptr<resource_tool::bitmap::product_sink> BitmapSink =
        resource_tool::bitmap::CreateTransactionalProductSink( Transaction, Context.Artifacts() );

    resource_tool::geometry::geometry_compile_job CompileJob;
    if( !CompileJob.Execute( Settings, FinalOutputFile, Context.GetRequest().Verbose, Context.Diagnostics(),
                             BitmapSink.get() ) )
    {
        return FALSE;
    }

    Context.Artifacts().Add( resource_tool::artifact_kind::Main, "geometry-product", FinalOutputFile );
    if( !FinalRawGeomFile.empty() )
    {
        Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "rawgeom", FinalRawGeomFile );
    }
    std::string CommitError;
    if( !Transaction.Commit( CommitError ) )
    {
        Context.Diagnostics().Error( CommitError.c_str(), "OutputTransaction" );
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool module::Decompile( resource_tool::resource_context& Context )
{
    geometry_descriptor Settings;
    geometry_descriptor::geometry_type Type;
    if( !ResolveGeometryProductType( Context.GetRequest().InputPath, Type ) )
        x_throw( "Unsupported compiled geometry product" );
    const std::filesystem::path AnimationPath = ResolveDecompileSkeleton( Context, Type );
    BuildProductDecompileSettings( Context.GetRequest().InputPath, AnimationPath, Settings );
    const std::filesystem::path FinalOutputFile = Settings.GetOutputFile();
    const std::filesystem::path FinalRawGeomFile = Settings.GetGeneratedRawGeom()[0]
                                                       ? std::filesystem::path( Settings.GetGeneratedRawGeom() )
                                                       : [&FinalOutputFile]()
    {
        std::filesystem::path Path = FinalOutputFile;
        Path.replace_extension( ".rawgeom" );
        return Path;
    }();
    std::filesystem::path FinalCollisionFile = FinalOutputFile;
    FinalCollisionFile.replace_extension( ".collision.gltf" );
    const std::filesystem::path FinalOutputBinary = [&FinalOutputFile]()
    {
        std::filesystem::path Path = FinalOutputFile;
        Path.replace_extension( ".bin" );
        return Path;
    }();

    if( SamePath( FinalRawGeomFile, Context.GetInputPath() ) )
    {
        x_throw( "Decompile output rawgeom would overwrite its input descriptor; "
                 "set ResDesc\\GeneratedRawGeom to a separate file" );
    }

    resource_tool::output_transaction& Transaction = Context.Transaction();
    const std::filesystem::path StagedOutputFile = Transaction.Stage( FinalOutputFile );
    const std::filesystem::path StagedOutputBinary = Transaction.Stage( FinalOutputBinary );
    std::filesystem::path FinalCollisionBinary;
    std::filesystem::path StagedCollisionFile;
    xbool bCollisionOutput = FALSE;

    geometry_decompile_job::output_paths Outputs;
    Outputs.Geometry = StagedOutputFile;
    Outputs.GeometryBinary = StagedOutputBinary;
    Outputs.GeometryBinaryUri = FinalOutputBinary;
    Outputs.RawGeom = Transaction.Stage( FinalRawGeomFile );
    Outputs.DescriptorGeometry = FinalOutputFile;
    Outputs.Transaction = &Transaction;
    Outputs.Artifacts = &Context.Artifacts();
    Outputs.Diagnostics = &Context.Diagnostics();
    if( Settings.GetType() == geometry_descriptor::GEOMETRY_TYPE_RIGID )
    {
        StagedCollisionFile = Transaction.StageOptional( FinalCollisionFile );
        FinalCollisionBinary = [&FinalCollisionFile]()
        {
            std::filesystem::path Path = FinalCollisionFile;
            Path.replace_extension( ".bin" );
            return Path;
        }();
        Outputs.Collision = StagedCollisionFile;
        Outputs.CollisionBinary = Transaction.StageOptional( FinalCollisionBinary );
        Outputs.CollisionBinaryUri = FinalCollisionBinary;
        Outputs.DescriptorCollision = FinalCollisionFile;
    }

    geometry_decompile_job Decompiler;
    resource_tool::resource_pipeline Pipeline;
    Pipeline.AddStage( "GeometryDecompile",
                       [&Decompiler, &Settings, &Outputs]( resource_tool::resource_context& )
                       {
                           Decompiler.Decompile( Settings, &Outputs );
                           return TRUE;
                       } );
    if( !Pipeline.Execute( Context ) )
        return FALSE;
    if( !StagedCollisionFile.empty() )
        bCollisionOutput = Decompiler.HasCollisionOutput();

    Context.Artifacts().Add( resource_tool::artifact_kind::Main, "gltf", FinalOutputFile );
    Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "gltf-binary", FinalOutputBinary );
    Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "rawgeom", FinalRawGeomFile );
    if( bCollisionOutput )
    {
        Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "collision-gltf", FinalCollisionFile );
        Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "collision-gltf-binary",
                                 FinalCollisionBinary );
    }
    std::string CommitError;
    if( !Transaction.Commit( CommitError ) )
    {
        Context.Diagnostics().Error( CommitError.c_str(), "OutputTransaction" );
        return FALSE;
    }
    return TRUE;
}

} // namespace resource_tool::geometry
