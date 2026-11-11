function( a51_add_astc_encoder )
    foreach( _isa AVX2 SSE41 SSE2 SVE_256 SVE_128 NEON NATIVE )
        set( ASTCENC_ISA_${_isa} OFF )
    endforeach()
    set( ASTCENC_ISA_NONE ON )
    set( ASTCENC_CLI OFF )
    set( ASTCENC_UNITTEST OFF )
    set( ASTCENC_WERROR OFF )
    set( ASTCENC_DECOMPRESSOR OFF )
    set( ASTCENC_SHAREDLIB OFF )
    add_subdirectory( "${CMAKE_SOURCE_DIR}/xCore/3rdParty/ASTC-Encoder"
                      "${CMAKE_BINARY_DIR}/thirdparty/ASTC-Encoder" EXCLUDE_FROM_ALL )
endfunction()

if( NOT TARGET astcenc-none-static )
    a51_add_astc_encoder()
endif()

set( RESOURCE_TOOL_ROOT "${CMAKE_CURRENT_LIST_DIR}" )

a51_add_project( ResourceToolCore STATIC
    SOURCE_ROOT "${RESOURCE_TOOL_ROOT}"
    SOURCES
        "${RESOURCE_TOOL_ROOT}/Core/ArtifactManifest.cpp"
        "${RESOURCE_TOOL_ROOT}/Core/ArtifactManifest.hpp"
        "${RESOURCE_TOOL_ROOT}/Core/DependencyGraph.cpp"
        "${RESOURCE_TOOL_ROOT}/Core/DependencyGraph.hpp"
        "${RESOURCE_TOOL_ROOT}/Core/Diagnostics.cpp"
        "${RESOURCE_TOOL_ROOT}/Core/Diagnostics.hpp"
        "${RESOURCE_TOOL_ROOT}/Core/ResourceHandler.cpp"
        "${RESOURCE_TOOL_ROOT}/Core/ResourceHandler.hpp"
        "${RESOURCE_TOOL_ROOT}/Core/ResourceRegistry.cpp"
        "${RESOURCE_TOOL_ROOT}/Core/ResourceRegistry.hpp"
        "${RESOURCE_TOOL_ROOT}/Core/ResourceUtilities.cpp"
        "${RESOURCE_TOOL_ROOT}/Core/ResourceUtilities.hpp"
        "${RESOURCE_TOOL_ROOT}/Core/OutputTransaction.cpp"
        "${RESOURCE_TOOL_ROOT}/Core/OutputTransaction.hpp"
        "${RESOURCE_TOOL_ROOT}/Pipeline/ResourcePipeline.cpp"
        "${RESOURCE_TOOL_ROOT}/Pipeline/ResourcePipeline.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${RESOURCE_TOOL_ROOT}/Core"
        "${RESOURCE_TOOL_ROOT}/Pipeline"
    DEPENDENCIES
        x_files
    )

a51_add_project( ResourceToolGltf STATIC
    SOURCE_ROOT "${RESOURCE_TOOL_ROOT}"
    SOURCES
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/Cgltf.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfDocument.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfDocument.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Model"
        "${CMAKE_SOURCE_DIR}/xCore/3rdParty/Cgltf"
    DEPENDENCIES
        x_files
)

a51_add_project( ResourceToolCommonFormats STATIC
    SOURCE_ROOT "${RESOURCE_TOOL_ROOT}"
    SOURCES
        "${RESOURCE_TOOL_ROOT}/Formats/Common/PropertyTree/PropertyTree.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Common/PropertyTree/PropertyTree.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${RESOURCE_TOOL_ROOT}/Formats/Common/PropertyTree"
)

a51_add_project( ResourceToolAnimation STATIC
    SOURCE_ROOT "${RESOURCE_TOOL_ROOT}"
    SOURCES
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/AnimationModule.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/AnimationModule.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/Compile/AnimationCompileJob.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/Compile/AnimationCompileJob.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/AnimationPaths.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/AnimationPaths.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/Decompile/AnimationDecompileJob.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/Decompile/AnimationDecompileJob.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Validation/AnimationValidation.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Validation/AnimationValidation.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Model/AnimationAsset.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/AnimationProduct/AnimationProductReader.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/AnimationProduct/AnimationProductReader.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/AnimationProduct/AnimationProductWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/AnimationProduct/AnimationProductWriter.hpp"
        "${CMAKE_SOURCE_DIR}/Support/Compilers/Shared/SkeletonContract.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/AnimationGltfWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/AnimationGltfWriter.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/AnimationGltfReader.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/AnimationGltfReader.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawAnim/RawAnimWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawAnim/RawAnimWriter.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawAnim/RawAnimReader.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawAnim/RawAnimReader.hpp"
        "${CMAKE_SOURCE_DIR}/Support/Animation/AnimEvent.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Animation/AnimCompress.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Animation/AnimDecompress.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Animation/AnimGroup.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Animation/AnimInfo.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Animation/AnimKeyData.cpp"
        "${CMAKE_SOURCE_DIR}/Support/ResourceMgr/ResourceMgr.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${RESOURCE_TOOL_ROOT}/Core"
        "${CMAKE_SOURCE_DIR}/Support/Compilers/Shared"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/Compile"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Pipeline/Decompile"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Validation"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Model"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation"
        "${RESOURCE_TOOL_ROOT}/Formats/AnimationProduct"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf"
        "${RESOURCE_TOOL_ROOT}/Formats/RawAnim"
        "${RESOURCE_TOOL_ROOT}/Formats/Common/PropertyTree"
        "${CMAKE_SOURCE_DIR}/xCore/3rdParty/Cgltf"
        "${CMAKE_SOURCE_DIR}/Support/Animation"
        "${CMAKE_SOURCE_DIR}/Support/ResourceMgr"
    DEPENDENCIES
        ResourceToolCore
        ResourceToolCommonFormats
        ResourceToolGltf
        x_files
    COMPILE_DEFINITIONS
        RESOURCE_TOOL_ANIMATION_ENCODER
)

a51_add_project( ResourceToolGeometry STATIC
    SOURCE_ROOT "${RESOURCE_TOOL_ROOT}"
    SOURCES
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryAsset.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryAsset.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryCollision.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryLod.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryMaterial.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryMesh.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryPhysics.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryNames.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryMetadata.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometrySkeleton.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model/GeometryTextureMap.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/GeometryModule.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/GeometryModule.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/IdentityStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/IdentityStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/CollisionImportStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/CollisionImportStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/GeometryCompileJob.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/GeometryCompileJob.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/MaterialStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/MaterialStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/LodStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/LodStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/NormalizeStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/NormalizeStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/PhysicsStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/PhysicsStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/RawGeomImportStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/RawGeomImportStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/SkeletonStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/SkeletonStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/ValidationStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile/ValidationStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/SkeletonSource.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/SkeletonSource.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/GeometryDecompileJob.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/GeometryDecompileJob.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/GeometryTextureExport.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/GeometryTextureExport.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/RawGeomProjection.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/RawGeomProjection.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/RenderGeometryStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/RenderGeometryStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/CollisionGeometryStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/CollisionGeometryStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/RawGeomWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile/RawGeomWriter.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometrySemanticDiff.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometrySemanticDiff.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometryValidationInternal.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometryValidationInternal.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometryRenderValidation.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometryMaterialValidation.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometrySkeletonPhysicsValidation.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Validation/GeometryLodCollisionValidation.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfWriter.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfDocument.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfCollisionWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GeometrySource.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GeometrySource.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfReader.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf/GltfReader.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawGeom/GeometryDescriptor.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawGeom/GeometryDescriptor.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/TextureInfo.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/TextureInfo.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductSnapshot.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductSnapshot.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductData.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductData.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductFormat.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductFormat.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductReader.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductReader.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductReconstruction.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductReconstruction.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductCollisionStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductCollisionStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductLodStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductLodStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductMaterialStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductMaterialStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductPhysicsStage.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductPhysicsStage.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductTypes.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductFileWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductFileWriter.hpp"
        "${CMAKE_SOURCE_DIR}/Support/Compilers/Shared/SkeletonContract.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductEncodingPipeline.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductEncodingPipeline.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductEncodingStages.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductEncodingStages.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductEncodingTypes.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter_Collision.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter_Materials.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter_Model.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter_Rigid.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter_Skeleton.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter_Skin.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct/GeometryProductWriter_Utilities.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Render/GeomFile.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Render/GeomProperties.cpp"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/MiscUtils/dictionary.cpp"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/MiscUtils/Guid.cpp"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/MiscUtils/mem_stream.cpp"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/MiscUtils/Property.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${RESOURCE_TOOL_ROOT}/Core"
        "${RESOURCE_TOOL_ROOT}/Pipeline"
        "${CMAKE_SOURCE_DIR}/Support/Compilers/Shared"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Compile"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Model"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation/Model"
        "${RESOURCE_TOOL_ROOT}/Formats/Gltf"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry/Pipeline/Decompile"
        "${RESOURCE_TOOL_ROOT}/Formats/AnimationProduct"
        "${RESOURCE_TOOL_ROOT}/Formats/RawGeom"
        "${RESOURCE_TOOL_ROOT}/Formats/Common/PropertyTree"
        "${RESOURCE_TOOL_ROOT}/Formats/GeometryProduct"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Model"
        "${CMAKE_SOURCE_DIR}/xCore/3rdParty/Cgltf"
    DEPENDENCIES
        ResourceToolCore
        ResourceToolCommonFormats
        ResourceToolBitmap
        ResourceToolAnimation
        ResourceToolGltf
        Parsing
        x_files
)

a51_add_project( ResourceToolBitmap STATIC
    SOURCE_ROOT "${RESOURCE_TOOL_ROOT}"
    SOURCES
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Model/BitmapImage.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Model/BitmapImage.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Model/BitmapBuildProfile.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Model/BitmapBuildProfile.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapDependencyDiscovery.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapDependencyDiscovery.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapCompileJob.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapCompileJob.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapProductSink.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapProductSink.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapProcessing.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile/BitmapSource.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Decompile/BitmapDecompileJob.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Decompile/BitmapDecompileJob.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Validation/BitmapValidation.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Validation/BitmapValidation.hpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/BitmapModule.cpp"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/BitmapModule.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/BitmapFormat.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/BitmapFormat.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/BitmapProcessing.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/BitmapProduct.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/BitmapSource.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/AstcCodec.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/AstcCodec.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/DxtCodec.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap/DxtCodec.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawBitmap/RawBitmapDescriptor.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawBitmap/RawBitmapReader.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawBitmap/RawBitmapReader.hpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawBitmap/RawBitmapWriter.cpp"
        "${RESOURCE_TOOL_ROOT}/Formats/RawBitmap/RawBitmapWriter.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${RESOURCE_TOOL_ROOT}/Core"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Model"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Compile"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Pipeline/Decompile"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap/Validation"
        "${RESOURCE_TOOL_ROOT}/Formats/Bitmap"
        "${RESOURCE_TOOL_ROOT}/Formats/RawBitmap"
        "${RESOURCE_TOOL_ROOT}/Formats/Common/PropertyTree"
    DEPENDENCIES
        astcenc-none-static
        ResourceToolCore
        ResourceToolCommonFormats
        Parsing
        x_files
)

a51_add_project( ResourceTool EXECUTABLE
    SOURCE_ROOT "${RESOURCE_TOOL_ROOT}"
    SOURCES
        "${RESOURCE_TOOL_ROOT}/App/ModuleRegistration.cpp"
        "${RESOURCE_TOOL_ROOT}/App/ModuleRegistration.hpp"
        "${RESOURCE_TOOL_ROOT}/ResourceTool.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${RESOURCE_TOOL_ROOT}/Core"
        "${RESOURCE_TOOL_ROOT}/App"
        "${RESOURCE_TOOL_ROOT}/Modules/Geometry"
        "${RESOURCE_TOOL_ROOT}/Modules/Bitmap"
        "${RESOURCE_TOOL_ROOT}/Modules/Animation"
    DEPENDENCIES
        ResourceToolGeometry
        ResourceToolBitmap
        ResourceToolAnimation
)
