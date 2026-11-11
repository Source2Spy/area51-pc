a51_add_project( DecalCompiler EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../../Decals/DecalDefinition.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Decals/DecalPackage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Decals/DecalPackageFile.cpp"
        "${CMAKE_SOURCE_DIR}/Support/Compilers/Shared/BMPUtil.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Decals/DecalDefinition.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Decals/DecalPackage.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Decals/DecalPackageFile.hpp"
        "${CMAKE_SOURCE_DIR}/Support/Compilers/Shared/BMPUtil.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/xCore/MeshUtil"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/CommandLine"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
        "${CMAKE_SOURCE_DIR}/xCore/../Support"
    DEPENDENCIES
        aux_Bitmap
        CommandLine
        x_files
)
