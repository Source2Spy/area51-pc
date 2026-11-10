#==========================================================================
#
#  CMakeLists.txt
#
#==========================================================================

cmake_minimum_required( VERSION 3.23 )

if( "$ENV{GITHUB_ACTIONS}" STREQUAL "true" )
    set( CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "" FORCE )
    set( CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE )
endif()

# Set CMP0091 before project().
cmake_policy( SET CMP0091 NEW )

# Android Gradle Plugin names its release native configuration
# "RelWithDebInfo".  Keep the engine's configuration model identical to
# desktop builds: Android release is the regular Area 51 Release build.
if( CMAKE_SYSTEM_NAME STREQUAL "Android" AND
    CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" )
    set( CMAKE_BUILD_TYPE Release CACHE STRING
        "Area 51 build configuration" FORCE )
endif()

project( Area51 LANGUAGES C CXX )

if( "$ENV{GITHUB_ACTIONS}" STREQUAL "true" )
    if( MSVC )
        add_compile_options( /w )
    else()
        add_compile_options( -w )
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        $<$<CONFIG:Release>:-g>
    )
endif()

# Must precede include( A51ThirdParty ), which add_subdirectory()'s the bundled libs.
if( MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY )
    set( CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<OR:$<STREQUAL:$<CONFIG>,Debug>,$<STREQUAL:$<CONFIG>,EDITOR-Debug>>:Debug>"
        CACHE STRING "MSVC runtime library selection" )
endif()

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )
include( A51Project )
include( A51Platform )

#==========================================================================
#  OPTIONS AND CONFIGURATIONS
#==========================================================================

set( A51_CONFIGURATION_TYPES Debug OptDebug QA Release EDITOR-Debug CACHE STRING "Supported Area 51 configurations" )
if( CMAKE_CONFIGURATION_TYPES )
    set( CMAKE_CONFIGURATION_TYPES "${A51_CONFIGURATION_TYPES}" CACHE STRING "Supported Area 51 configurations" FORCE )
elseif( NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "" )
    if( CMAKE_SYSTEM_NAME STREQUAL "Android" )
        set( CMAKE_BUILD_TYPE Release CACHE STRING
            "Default Area 51 configuration for Android builds" FORCE )
    else()
        set( CMAKE_BUILD_TYPE Debug CACHE STRING
            "Default Area 51 configuration for single-config generators" FORCE )
    endif()
endif()

foreach( _a51_configuration IN LISTS A51_CONFIGURATION_TYPES )
    string( TOUPPER "${_a51_configuration}" _a51_configuration_upper )
    foreach( _a51_linker_kind EXE SHARED MODULE )
        set( _a51_linker_flags_variable
            "CMAKE_${_a51_linker_kind}_LINKER_FLAGS_${_a51_configuration_upper}" )
        if( NOT DEFINED "${_a51_linker_flags_variable}" )
            set( "${_a51_linker_flags_variable}" ""
                CACHE STRING "${_a51_configuration} ${_a51_linker_kind} linker flags" )
        endif()
    endforeach()
endforeach()

option( A51_BUILD_SELECTED_APPS "Build the selected application targets" ON )
option( A51_ENABLE_QT "Enable Qt integration for XBMPViewer when Qt6 is available" ON )
option( A51_EXPORT_COMPILE_COMMANDS "Generate compile_commands.json for tooling" ON )
option( A51_BUILD_SHADER_TOOL "Build the host shader compiler and preparation target" OFF )
option( A51_SKIP_SHADER_KIT_VALIDATION "Skip shader kit validation for partial C++ build diagnostics (not a runnable build)" OFF )
set( A51_SHADER_KIT "${CMAKE_SOURCE_DIR}/build/shaders/GameApp/release"
     CACHE PATH "Prepared GameApp shader kit" )
set( A51_SHADER_TOOLS_ONLY OFF )
if( A51_BUILD_SHADER_TOOL AND NOT A51_BUILD_SELECTED_APPS )
    set( A51_SHADER_TOOLS_ONLY ON )
endif()
if( WIN32 )
    set( _a51_default_windows_targets ON )
else()
    set( _a51_default_windows_targets OFF )
endif()
option( A51_BUILD_WINDOWS_TARGETS "Enable targets whose current implementation requires Windows-only dependencies" ${_a51_default_windows_targets} )

if( A51_EXPORT_COMPILE_COMMANDS )
    set( CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
        "Generate compile_commands.json for tooling" FORCE )
endif()

add_library( a51_build_options INTERFACE )
add_library( a51::build_options ALIAS a51_build_options )

target_compile_features( a51_build_options INTERFACE cxx_std_17 )
target_compile_definitions( a51_build_options INTERFACE
    "$<$<PLATFORM_ID:Windows>:NOMINMAX>"
    "$<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>"
    "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:CONFIG_DEBUG>"
    "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:X_EDITOR>"
)

if( MSVC )
    # Allow parallel cl.exe invocations.
    target_compile_options( a51_build_options INTERFACE /FS )

    target_compile_options( a51_build_options INTERFACE
        "$<$<CONFIG:Debug>:/Od>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:/Od>"
        "$<$<CONFIG:OptDebug>:/O2>"
        "$<$<CONFIG:QA>:/O2>"
        "$<$<CONFIG:Release>:/O2>"
        "$<$<CONFIG:Debug>:/Zi>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:/Zi>"
        "$<$<CONFIG:OptDebug>:/Zi>"
        "$<$<CONFIG:QA>:/Zi>"
        "$<$<CONFIG:Release>:/Zi>"
    )
    target_link_options( a51_build_options INTERFACE
        "$<$<CONFIG:Debug>:/DEBUG>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:/DEBUG>"
        "$<$<CONFIG:OptDebug>:/DEBUG>"
        "$<$<CONFIG:QA>:/DEBUG>"
        "$<$<CONFIG:Release>:/DEBUG>"
    )
endif()

if( WIN32 )
    target_link_libraries( a51_build_options INTERFACE
        ws2_32
        user32
        gdi32
        ole32
        comctl32
        comdlg32
    )
endif()

include( CheckIncludeFileCXX )
if( WIN32 )
    check_include_file_cxx( "windows.h" A51_HAS_WINDOWS_H )
    if( NOT A51_HAS_WINDOWS_H )
        message( WARNING "The active Windows toolchain cannot include windows.h. Configure succeeds, but Windows targets require a Windows SDK toolchain before they can be built." )
    endif()
endif()

if( A51_ENABLE_QT )
    find_package( Qt6 COMPONENTS Core Gui Widgets QUIET )
endif()

#==========================================================================
#  PROJECT GRAPH
#==========================================================================

include( A51ThirdParty )
add_subdirectory( xCore )

# Shader preparation runs on the host independently of the game build.
if( A51_BUILD_SHADER_TOOL )
    if( CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Windows)$" )
        message( FATAL_ERROR "ShaderTool requires a native Linux or Windows build." )
    endif()
    set( A51_DXC_SOURCE_DIR
         "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/DXC"
         CACHE PATH "Bundled DirectX Shader Compiler source directory" )

    if( NOT EXISTS "${A51_DXC_SOURCE_DIR}/CMakeLists.txt" )
        message( FATAL_ERROR
            "A51_BUILD_SHADER_TOOL is enabled, but DXC was not found at "
            "${A51_DXC_SOURCE_DIR}." )
    endif()

    set( _a51_dxc_external_dir "${A51_DXC_SOURCE_DIR}/external" )
    if( NOT WIN32 AND
        NOT EXISTS "${_a51_dxc_external_dir}/DirectX-Headers/include/directx/d3d12shader.h" )
        message( FATAL_ERROR
            "DXC requires the DirectX-Headers dependency at "
            "${_a51_dxc_external_dir}/DirectX-Headers. "
            "Populate DXC external dependencies before configuring Area51." )
    endif()
    if( NOT EXISTS "${_a51_dxc_external_dir}/SPIRV-Headers/include/spirv/unified1/spirv.hpp" OR
        NOT EXISTS "${_a51_dxc_external_dir}/SPIRV-Tools/CMakeLists.txt" )
        message( FATAL_ERROR
            "DXC SPIR-V dependencies are missing under ${_a51_dxc_external_dir}. "
            "Populate SPIRV-Headers and SPIRV-Tools before configuring Area51." )
    endif()

    # DXC is a large LLVM project.  Only its compiler targets are needed by
    # ShaderTool, so keep the generated graph and default build small.
    set( ENABLE_SPIRV_CODEGEN ON CACHE BOOL "Enable DXC SPIR-V code generation" FORCE )
    set( HLSL_DISABLE_SOURCE_GENERATION ON CACHE BOOL "Use checked-in DXC generated sources" FORCE )
    set( LLVM_TARGETS_TO_BUILD None CACHE STRING "LLVM targets used by DXC" FORCE )
    set( LLVM_ENABLE_EH ON CACHE BOOL "Enable DXC exception handling" FORCE )
    set( LLVM_ENABLE_RTTI ON CACHE BOOL "Enable DXC RTTI" FORCE )
    set( CLANG_ENABLE_ARCMT OFF CACHE BOOL "Build unused Clang ARCMT support" FORCE )
    set( CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "Build unused Clang static analyzer" FORCE )
    set( SPIRV_BUILD_TESTS OFF CACHE BOOL "Build DXC SPIR-V tests" FORCE )
    set( HLSL_INCLUDE_TESTS OFF CACHE BOOL "Generate DXC HLSL tests" FORCE )
    set( LLVM_INCLUDE_TESTS OFF CACHE BOOL "Generate LLVM tests" FORCE )
    set( LLVM_BUILD_TESTS OFF CACHE BOOL "Build LLVM tests" FORCE )
    set( LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "Generate LLVM examples" FORCE )
    set( LLVM_BUILD_EXAMPLES OFF CACHE BOOL "Build LLVM examples" FORCE )
    set( LLVM_INCLUDE_DOCS OFF CACHE BOOL "Generate LLVM documentation" FORCE )
    set( LLVM_BUILD_DOCS OFF CACHE BOOL "Build LLVM documentation" FORCE )
    set( LLVM_INCLUDE_UTILS OFF CACHE BOOL "Generate LLVM utilities" FORCE )
    set( LLVM_DISTRIBUTION_COMPONENTS "" CACHE STRING "DXC distribution components" FORCE )
    set( DIRECTX_HEADER_INCLUDE_DIR
         "${_a51_dxc_external_dir}/DirectX-Headers/include"
         CACHE PATH "DirectX-Headers include directory" FORCE )

    add_subdirectory( "${A51_DXC_SOURCE_DIR}"
                      "${CMAKE_BINARY_DIR}/DXC"
                      EXCLUDE_FROM_ALL )
endif()

if( NOT A51_SHADER_TOOLS_ONLY )
    add_subdirectory( Support )
endif()
add_subdirectory( Apps )

set( _a51_tool_targets
    xbmptool
    Art2Code
    dfsTool
    XBMPViewer
    # SoundPackager
    ShaderTool
    DecalCompiler
    ResourceTool
)

set( _a51_configured_tool_targets )
foreach( _a51_tool_target IN LISTS _a51_tool_targets )
    if( TARGET ${_a51_tool_target} )
        list( APPEND _a51_configured_tool_targets ${_a51_tool_target} )
    endif()
endforeach()

if( _a51_configured_tool_targets )
    add_custom_target( a51_tools )
    add_dependencies( a51_tools ${_a51_configured_tool_targets} )
endif()

#==========================================================================
#  PLATFORM DEPENDENCIES
#==========================================================================

if( TARGET Entropy )
    target_compile_definitions( Entropy PRIVATE ENTROPY_RENDER_SDL )
endif()

if( TARGET ScriptMgr )
    target_include_directories( ScriptMgr PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/Lua/5.5.0"
    )
endif()

if( TARGET a51_shader_compiler )
    if( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/DXC/include/dxc/dxcapi.h" )
        target_include_directories( a51_shader_compiler PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/DXC/include/dxc"
        )
    else()
        target_include_directories( a51_shader_compiler PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/DXC/inc"
        )
    endif()

    if( TARGET dxcompiler AND NOT WIN32 )
        target_compile_definitions( a51_shader_compiler PRIVATE DXC_USE_DIRECTX_HEADERS=1 )
        target_include_directories( a51_shader_compiler PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/DXC/external/DirectX-Headers/include/directx"
            "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/DXC/external/DirectX-Headers/include/wsl/stubs"
        )
    endif()

    if( TARGET dxcompiler )
        target_link_libraries( a51_shader_compiler PRIVATE dxcompiler )
        if( UNIX )
            set_property( TARGET ShaderTool APPEND PROPERTY BUILD_RPATH
                          "${CMAKE_BINARY_DIR}/lib" )
        endif()
    endif()
endif()

if( TARGET Render )
    include( A51ShaderKit )
    if( A51_SKIP_SHADER_KIT_VALIDATION )
        message( WARNING
            "Shader kit validation skipped for C++ build diagnostics. "
            "Without generated shader headers, shader consumers will fail to compile. "
            "This mode does not produce a verified runnable build." )
    else()
    a51_import_shader_kit( "${A51_SHADER_KIT}"
        generated/a51_compute_cloth_tessellate.hpp
        generated/a51_compute_cmaa2_init.hpp
        generated/a51_compute_cmaa2_edges.hpp
        generated/a51_compute_cmaa2_args.hpp
        generated/a51_compute_cmaa2_candidates.hpp
        generated/a51_compute_cmaa2_apply.hpp
        generated/a51_compute_xegtao_prepare.hpp
        generated/a51_compute_xegtao_prepare_msaa.hpp
        generated/a51_compute_xegtao_prepare_half.hpp
        generated/a51_compute_xegtao_prepare_half_msaa.hpp
        generated/a51_compute_xegtao_medium.hpp
        generated/a51_compute_xegtao_high.hpp
        generated/a51_compute_xegtao_ultra.hpp
        generated/a51_compute_xegtao_denoise.hpp
        generated/a51_compute_xegtao_denoise_final.hpp
        generated/a51_compute_xegtao_resolve.hpp )
    endif()
    target_include_directories( Render PUBLIC "${A51_SHADER_KIT}/generated" )
    foreach( _a51_shader_consumer Entropy UserInterface Gamelib FX A51 main )
        if( TARGET ${_a51_shader_consumer} )
            target_include_directories( ${_a51_shader_consumer} PRIVATE "${A51_SHADER_KIT}/generated" )
        endif()
    endforeach()
    target_compile_definitions( Entropy PRIVATE A51_GAME_SHADER_CONTRACT=1 )
    if( TARGET A51 OR TARGET main )
        install( DIRECTORY "${A51_SHADER_KIT}/"
            DESTINATION SHADERS
            COMPONENT Runtime
            FILES_MATCHING PATTERN "*.ecs" )
    endif()
endif()

if( TARGET ShaderTool )
    set( A51_SHADER_JOBS 8 CACHE STRING "Shader preparation worker count" )
    add_custom_target( a51_prepare_shaders
        COMMAND "$<TARGET_FILE:ShaderTool>" -jobs "${A51_SHADER_JOBS}"
            "${CMAKE_SOURCE_DIR}/Apps/GameApp/media/GameApp.shaderscript"
        DEPENDS ShaderTool
        USES_TERMINAL
        VERBATIM )
endif()

message( STATUS "Configured the native Area 51 CMake target graph." )
