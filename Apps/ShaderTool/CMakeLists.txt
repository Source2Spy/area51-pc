a51_add_project( a51_shader_compiler STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/DxcCompiler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ParseScript.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SpirvReflection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderContract.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderCache.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderRecipes.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderCombo.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderComboSchema.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderPackages.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderTool.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderConsole.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderTool.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
    DEPENDENCIES
        a51_ecs_codec
        CommandLine
        Parsing
        x_files
        dxcompiler
)

if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "13" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14" )
    # GCC 13 loop unswitching at -O3 drops alpha-tested ForceZFill shadow profiles.
    set_property( SOURCE "${CMAKE_CURRENT_LIST_DIR}/ShaderRecipes.cpp"
        APPEND PROPERTY COMPILE_OPTIONS -fno-unswitch-loops )
endif()

# Reflection and validation are host tools; none of these libraries enter Android.
add_library( a51_spirv_reflect STATIC
    "${CMAKE_SOURCE_DIR}/xCore/3rdParty/SPIRV-Reflect/spirv_reflect.c" )
target_include_directories( a51_spirv_reflect PUBLIC
    "${CMAKE_SOURCE_DIR}/xCore/3rdParty/SPIRV-Reflect" )
target_link_libraries( a51_shader_compiler PRIVATE a51_spirv_reflect SPIRV-Tools-static LLVMSupport )
target_include_directories( a51_shader_compiler PRIVATE
    "${CMAKE_SOURCE_DIR}/xCore/3rdParty/DXC/external/SPIRV-Tools/include" )

a51_add_project( ShaderTool EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/ShaderMain.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
    DEPENDENCIES a51_shader_compiler )

target_compile_definitions( a51_shader_compiler PRIVATE A51_DXC_LIBRARY_FILE="$<TARGET_FILE:dxcompiler>" )

if( WIN32 )
    add_custom_command( TARGET ShaderTool POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:dxcompiler>" "$<TARGET_FILE_DIR:ShaderTool>"
        VERBATIM )
endif()
