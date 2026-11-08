#==========================================================================
#
#  A51ThirdParty.cmake
#
#  Build bundled third-party libraries.
#
#==========================================================================

include_guard( GLOBAL )

option( A51_USE_BUNDLED_DEPENDENCIES
    "Build the bundled third-party dependencies from source" ON )

if( NOT A51_USE_BUNDLED_DEPENDENCIES )
    message( FATAL_ERROR
        "A51_USE_BUNDLED_DEPENDENCIES=OFF is not supported by this build." )
endif()

set( A51_THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/xCore/3rdParty" )

set( BUILD_SHARED_LIBS OFF CACHE BOOL
    "Build bundled third-party libraries as static libraries" FORCE )
set( BUILD_TESTING OFF CACHE BOOL
    "Disable bundled third-party tests" FORCE )

set( ZSTD_BUILD_STATIC ON CACHE BOOL "Build bundled ZSTD static library" FORCE )
set( ZSTD_BUILD_SHARED OFF CACHE BOOL "Build bundled ZSTD shared library" FORCE )
set( ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Build bundled ZSTD programs" FORCE )
set( ZSTD_BUILD_TESTS OFF CACHE BOOL "Build bundled ZSTD tests" FORCE )
set( ZSTD_BUILD_CONTRIB OFF CACHE BOOL "Build bundled ZSTD contrib programs" FORCE )
set( ZSTD_BUILD_COMPRESSION ON CACHE BOOL "Build ZSTD compression" FORCE )
set( ZSTD_BUILD_DECOMPRESSION ON CACHE BOOL "Build ZSTD decompression" FORCE )
set( ZSTD_BUILD_DICTBUILDER OFF CACHE BOOL "Build ZSTD dictionary training" FORCE )
set( ZSTD_BUILD_DEPRECATED OFF CACHE BOOL "Build deprecated ZSTD API" FORCE )
set( ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "Build legacy ZSTD decoders" FORCE )
set( ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "Build ZSTD compression workers" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/ZSTD/build/cmake"
    "${CMAKE_BINARY_DIR}/thirdparty/ZSTD" EXCLUDE_FROM_ALL )
set_target_properties( libzstd_static PROPERTIES POSITION_INDEPENDENT_CODE ON )

if( A51_SHADER_TOOLS_ONLY )
    return()
endif()

# SDL
if( A51_TARGET_PLATFORM STREQUAL "Android" )
    # SDLActivity loads libSDL3.so before the application library.
    set( SDL_SHARED ON CACHE BOOL "Build SDL3 shared library" FORCE )
    set( SDL_STATIC OFF CACHE BOOL "Build SDL3 static library" FORCE )

    # The Android renderer uses SDL_GPU/Vulkan exclusively.  Do not build
    # SDL's GLES/OpenGL paths into this target: RenderDoc's Android GLES
    # layer can otherwise expose a second API and steal frame selection.
    set( SDL_OPENGL OFF CACHE BOOL "Include OpenGL support" FORCE )
    set( SDL_OPENGLES OFF CACHE BOOL "Include OpenGL ES support" FORCE )
    set( SDL_VULKAN ON CACHE BOOL "Enable Vulkan support" FORCE )
else()
    set( SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE )
    set( SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE )
endif()
set( SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE )
set( SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE )
set( SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE )
set( SDL_INSTALL OFF CACHE BOOL "Install SDL3" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/SDL3"
    "${CMAKE_BINARY_DIR}/thirdparty/SDL3" EXCLUDE_FROM_ALL )
if( A51_TARGET_PLATFORM STREQUAL "Android" )
    add_library( a51::SDL3 ALIAS SDL3-shared )
else()
    add_library( a51::SDL3 ALIAS SDL3-static )
endif()

# Monocypher
add_library( a51_monocypher STATIC
    "${A51_THIRDPARTY_ROOT}/Monocypher/monocypher.c"
    "${A51_THIRDPARTY_ROOT}/Monocypher/monocypher-ed25519.c" )
target_include_directories( a51_monocypher PUBLIC
    "${A51_THIRDPARTY_ROOT}/Monocypher" )
set_target_properties( a51_monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON )
add_library( a51::monocypher ALIAS a51_monocypher )

# Xiph libraries
set( INSTALL_DOCS OFF CACHE BOOL "Install bundled third-party docs" FORCE )
set( INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL
    "Install bundled third-party pkg-config files" FORCE )
set( INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL
    "Install bundled third-party CMake packages" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libogg"
    "${CMAKE_BINARY_DIR}/thirdparty/Libogg" EXCLUDE_FROM_ALL )
add_library( a51::ogg ALIAS ogg )

set( OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL
    "Build Opus shared library" FORCE )
set( OPUS_BUILD_TESTING OFF CACHE BOOL "Build Opus tests" FORCE )
set( OPUS_BUILD_PROGRAMS OFF CACHE BOOL "Build Opus programs" FORCE )
set( OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL
    "Install Opus pkg-config module" FORCE )
set( OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL
    "Install Opus CMake package module" FORCE )
set( OPUS_STATIC_RUNTIME ON CACHE BOOL
    "Build Opus with the static MSVC runtime" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libopus"
    "${CMAKE_BINARY_DIR}/thirdparty/Libopus" EXCLUDE_FROM_ALL )
add_library( a51::opus ALIAS opus )

add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libvorbis"
    "${CMAKE_BINARY_DIR}/thirdparty/Libvorbis" EXCLUDE_FROM_ALL )
add_library( a51::vorbis ALIAS vorbis )
add_library( a51::vorbisenc ALIAS vorbisenc )
add_library( a51::vorbisfile ALIAS vorbisfile )

# libvpx
set( A51_LIBVPX_ROOT "${A51_THIRDPARTY_ROOT}/Libvpx" )
if( NOT EXISTS "${A51_LIBVPX_ROOT}/CMakeLists.txt" )
    message( FATAL_ERROR "Bundled libvpx source tree is missing its CMakeLists.txt." )
endif()

set( LIBVPX_BUILD_SHARED OFF CACHE BOOL
    "Build bundled libvpx as a shared library" )
set( LIBVPX_BUILD_STATIC ON CACHE BOOL
    "Build bundled libvpx as a static library" )
set( LIBVPX_BUILD_ENCODER OFF CACHE BOOL
    "Build bundled libvpx encoders" )
set( LIBVPX_ENABLE_POSTPROC OFF CACHE BOOL
    "Build bundled libvpx VP8 postprocessing" )
set( LIBVPX_ENABLE_VP9_POSTPROC OFF CACHE BOOL
    "Build bundled libvpx VP9 postprocessing" )
set( LIBVPX_ASM_MODE AUTO CACHE STRING
    "Assembly policy for bundled libvpx" )
set( LIBVPX_ENABLE_SIMD ON CACHE BOOL
    "Build bundled libvpx architecture-specific C implementations" )
set( LIBVPX_ENABLE_RUNTIME_CPU_DETECT ON CACHE BOOL
    "Enable bundled libvpx runtime CPU detection" )
set( LIBVPX_ENABLE_VP9_HIGHBITDEPTH OFF CACHE BOOL
    "Enable bundled libvpx VP9 high bitdepth support" )

add_subdirectory( "${A51_LIBVPX_ROOT}"
    "${CMAKE_BINARY_DIR}/thirdparty/Libvpx" EXCLUDE_FROM_ALL )
add_library( a51::vpx ALIAS libvpx )

# opusfile is not used by the current movie backend.

# libwebm
set( ENABLE_WEBMTS OFF CACHE BOOL "Build libwebm MPEG-TS tools" FORCE )
set( ENABLE_WEBMINFO OFF CACHE BOOL "Build libwebm info tool" FORCE )
set( ENABLE_TESTS OFF CACHE BOOL "Build libwebm tests" FORCE )
set( ENABLE_SAMPLE_PROGRAMS OFF CACHE BOOL "Build libwebm samples" FORCE )
set( A51_CMAKE_CXX_FLAGS_BEFORE_LIBWEBM "${CMAKE_CXX_FLAGS}" )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libwebm"
    "${CMAKE_BINARY_DIR}/thirdparty/Libwebm" EXCLUDE_FROM_ALL )
set( CMAKE_CXX_FLAGS "${A51_CMAKE_CXX_FLAGS_BEFORE_LIBWEBM}"
    CACHE STRING "C++ compiler flags" FORCE )
unset( A51_CMAKE_CXX_FLAGS_BEFORE_LIBWEBM )
foreach( A51_LIBWEBM_TARGET mkvmuxer mkvparser webvtt_common webm )
    if( TARGET ${A51_LIBWEBM_TARGET} )
        target_compile_features( ${A51_LIBWEBM_TARGET} PRIVATE cxx_std_17 )
        target_compile_definitions( ${A51_LIBWEBM_TARGET} PRIVATE
            __STDC_CONSTANT_MACROS
            __STDC_FORMAT_MACROS
            __STDC_LIMIT_MACROS
        )
    endif()
endforeach()
target_include_directories( webm PUBLIC
    "${A51_THIRDPARTY_ROOT}/Libwebm"
    "${A51_THIRDPARTY_ROOT}/Libwebm/webm_parser/include"
)
add_library( a51::webm ALIAS webm )

message( STATUS "Using bundled SDL3, Ogg, Opus, Vorbis, WebM and native CMake libvpx." )

# Font libraries
set( FT_DISABLE_ZLIB ON CACHE BOOL "Disable external zlib" FORCE )
set( FT_DISABLE_BZIP2 ON CACHE BOOL "Disable bzip2" FORCE )
set( FT_DISABLE_PNG ON CACHE BOOL "Disable PNG glyphs" FORCE )
set( FT_DISABLE_HARFBUZZ ON CACHE BOOL "Avoid a FreeType/HarfBuzz build cycle" FORCE )
set( FT_DISABLE_BROTLI ON CACHE BOOL "Disable WOFF2" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Freetype" "${CMAKE_BINARY_DIR}/thirdparty/Freetype" EXCLUDE_FROM_ALL )
add_library( a51::freetype ALIAS freetype )
set( HB_HAVE_FREETYPE ON CACHE BOOL "FreeType font functions" FORCE )
set( HB_HAVE_ICU OFF CACHE BOOL "Use HarfBuzz Unicode data" FORCE )
set( HB_BUILD_SUBSET OFF CACHE BOOL "Disable font subsetting" FORCE )
set( HB_BUILD_RASTER OFF CACHE BOOL "Use FreeType rasterization" FORCE )
set( HB_BUILD_VECTOR OFF CACHE BOOL "Disable vector backend" FORCE )
set( HB_BUILD_GPU OFF CACHE BOOL "Disable GPU backend" FORCE )
set( HB_BUILD_UTILS OFF CACHE BOOL "Disable command line tools" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Harfbuzz" "${CMAKE_BINARY_DIR}/thirdparty/Harfbuzz" EXCLUDE_FROM_ALL )
add_library( a51::harfbuzz ALIAS harfbuzz )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/ICU" "${CMAKE_BINARY_DIR}/thirdparty/ICU" EXCLUDE_FROM_ALL )
