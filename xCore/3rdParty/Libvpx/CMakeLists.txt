# Copyright (c) 2026 The WebM project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license that can be found
# in the LICENSE file in the root of the source tree. An additional intellectual
# property rights grant can be found in the PATENTS file in the same directory.

cmake_minimum_required(VERSION 3.23)

project(libvpx VERSION 1.16.0 LANGUAGES C)

include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(LIBVPX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(LIBVPX_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Target architecture
if(DEFINED A51_TARGET_ARCHITECTURE)
  set(_libvpx_target_arch "${A51_TARGET_ARCHITECTURE}")
else()
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _libvpx_target_processor)
  if(_libvpx_target_processor MATCHES "^(i[3-6]86|x86)$")
    set(_libvpx_target_arch x86)
  elseif(_libvpx_target_processor MATCHES "^(x86_64|amd64)$")
    set(_libvpx_target_arch x64)
  elseif(_libvpx_target_processor MATCHES "^(arm|arm32|armv[4-8].*)$")
    set(_libvpx_target_arch arm32)
  elseif(_libvpx_target_processor MATCHES "^(aarch64|arm64)$")
    set(_libvpx_target_arch arm64)
  elseif(_libvpx_target_processor MATCHES "^(mips64.*)$")
    set(_libvpx_target_arch mips64)
  elseif(_libvpx_target_processor MATCHES "^(mips.*)$")
    set(_libvpx_target_arch mips32)
  elseif(_libvpx_target_processor MATCHES "^(ppc64|powerpc64).*$")
    set(_libvpx_target_arch ppc64le)
  elseif(_libvpx_target_processor MATCHES "^loongarch64")
    set(_libvpx_target_arch loongarch64)
  elseif(_libvpx_target_processor MATCHES "^loongarch32")
    set(_libvpx_target_arch loongarch32)
  else()
    set(_libvpx_target_arch generic)
  endif()
endif()
string(TOLOWER "${_libvpx_target_arch}" _libvpx_target_arch)
set(_libvpx_supported_architectures
  generic x86 x64 arm32 arm64 mips32 mips64 ppc64le loongarch32 loongarch64)
if(NOT _libvpx_target_arch IN_LIST _libvpx_supported_architectures)
  message(FATAL_ERROR
    "Unsupported libvpx target architecture: ${_libvpx_target_arch}")
endif()

# Build options
if(NOT DEFINED LIBVPX_BUILD_STATIC)
  if(DEFINED BUILD_SHARED_LIBS)
    if(BUILD_SHARED_LIBS)
      set(_libvpx_default_static OFF)
    else()
      set(_libvpx_default_static ON)
    endif()
  else()
    set(_libvpx_default_static ON)
  endif()
  set(LIBVPX_BUILD_STATIC "${_libvpx_default_static}" CACHE BOOL
    "Build libvpx as a static library")
endif()
if(NOT DEFINED LIBVPX_BUILD_SHARED)
  if(DEFINED BUILD_SHARED_LIBS)
    set(_libvpx_default_shared "${BUILD_SHARED_LIBS}")
  else()
    set(_libvpx_default_shared OFF)
  endif()
  set(LIBVPX_BUILD_SHARED "${_libvpx_default_shared}" CACHE BOOL
    "Build libvpx as a shared library")
endif()
option(LIBVPX_BUILD_ENCODER "Build the VP8/VP9 encoders" ON)
option(LIBVPX_BUILD_DECODER "Build the VP8/VP9 decoders" ON)
option(LIBVPX_BUILD_VP8 "Build VP8 codec support" ON)
option(LIBVPX_BUILD_VP9 "Build VP9 codec support" ON)
if(NOT DEFINED LIBVPX_BUILD_VP8_ENCODER)
  set(LIBVPX_BUILD_VP8_ENCODER "${LIBVPX_BUILD_ENCODER}" CACHE BOOL
    "Build the VP8 encoder")
endif()
if(NOT DEFINED LIBVPX_BUILD_VP8_DECODER)
  set(LIBVPX_BUILD_VP8_DECODER "${LIBVPX_BUILD_DECODER}" CACHE BOOL
    "Build the VP8 decoder")
endif()
if(NOT DEFINED LIBVPX_BUILD_VP9_ENCODER)
  set(LIBVPX_BUILD_VP9_ENCODER "${LIBVPX_BUILD_ENCODER}" CACHE BOOL
    "Build the VP9 encoder")
endif()
if(NOT DEFINED LIBVPX_BUILD_VP9_DECODER)
  set(LIBVPX_BUILD_VP9_DECODER "${LIBVPX_BUILD_DECODER}" CACHE BOOL
    "Build the VP9 decoder")
endif()
set(LIBVPX_ASM_MODE "AUTO" CACHE STRING
  "Assembly policy: AUTO uses it when available, ON requires it, OFF disables it")
set_property(CACHE LIBVPX_ASM_MODE PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${LIBVPX_ASM_MODE}" LIBVPX_ASM_MODE)
if(NOT LIBVPX_ASM_MODE MATCHES "^(AUTO|ON|OFF)$")
  message(FATAL_ERROR
    "LIBVPX_ASM_MODE must be one of AUTO, ON, or OFF; got '${LIBVPX_ASM_MODE}'.")
endif()
if(LIBVPX_ASM_MODE STREQUAL "OFF")
  set(LIBVPX_ENABLE_ASM OFF)
else()
  set(LIBVPX_ENABLE_ASM ON)
endif()
option(LIBVPX_ENABLE_SIMD
  "Build compiler-supported architecture-specific C implementations" ON)
if(LIBVPX_ASM_MODE STREQUAL "ON" AND NOT LIBVPX_ENABLE_SIMD)
  message(FATAL_ERROR
    "LIBVPX_ASM_MODE=ON requires LIBVPX_ENABLE_SIMD=ON.")
endif()
option(LIBVPX_ENABLE_PIC "Build position-independent code" ON)
option(LIBVPX_ENABLE_VP9_HIGHBITDEPTH
  "Enable VP9 10/12-bit decoder support" OFF)
if(NOT DEFINED LIBVPX_ENABLE_POSTPROC)
  if(_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64)
    set(_libvpx_default_postproc ON)
  else()
    set(_libvpx_default_postproc OFF)
  endif()
  set(LIBVPX_ENABLE_POSTPROC "${_libvpx_default_postproc}" CACHE BOOL
    "Enable VP8 postprocessing")
endif()
option(LIBVPX_ENABLE_VP9_POSTPROC "Enable VP9 postprocessing" OFF)
option(LIBVPX_ENABLE_MULTITHREAD "Enable multithreaded codec support" ON)
option(LIBVPX_ENABLE_SPATIAL_RESAMPLING "Enable spatial resampling" ON)
option(LIBVPX_ENABLE_INTERNAL_STATS "Enable encoder internal statistics" OFF)
option(LIBVPX_ENABLE_REALTIME_ONLY "Build the reduced realtime-only encoder" OFF)
option(LIBVPX_ENABLE_ONTHEFLY_BITPACKING
  "Enable on-the-fly realtime bitpacking" OFF)
option(LIBVPX_ENABLE_ERROR_CONCEALMENT "Enable decoder error concealment" OFF)
option(LIBVPX_ENABLE_MULTI_RES_ENCODING "Enable multiresolution encoding" OFF)
option(LIBVPX_ENABLE_TEMPORAL_DENOISING "Enable temporal denoising" ON)
option(LIBVPX_ENABLE_VP9_TEMPORAL_DENOISING
  "Enable VP9 temporal denoising" OFF)
option(LIBVPX_ENABLE_COEFFICIENT_RANGE_CHECKING
  "Enable decoder coefficient range checking" OFF)
option(LIBVPX_ENABLE_BETTER_HW_COMPATIBILITY
  "Enable better encoder hardware compatibility" OFF)
option(LIBVPX_ENABLE_VSX "Enable Power VSX optimizations" OFF)
option(LIBVPX_ENABLE_NON_GREEDY_MV "Enable the experimental non-greedy MV code" OFF)
option(LIBVPX_ENABLE_EXPERIMENTAL "Enable experimental codec features" OFF)
option(LIBVPX_ENABLE_BITSTREAM_DEBUG "Enable bitstream debugging" OFF)
option(LIBVPX_ENABLE_MISMATCH_DEBUG "Enable mismatch debugging" OFF)
option(LIBVPX_ENABLE_SMALL "Favor smaller code over speed" OFF)
set(LIBVPX_SIZE_LIMIT "" CACHE STRING
  "Limit decoded frame size (WIDTHxHEIGHT), or leave empty")
option(LIBVPX_ENABLE_WEBM_IO "Enable WebM I/O support in libvpx tools" OFF)
option(LIBVPX_ENABLE_LIBYUV "Use libyuv in libvpx tools" OFF)
option(LIBVPX_ENABLE_POSTPROC_VISUALIZER
  "Enable the postproc visualizer in libvpx tools" OFF)
option(LIBVPX_ENABLE_DEBUG
  "Enable libvpx internal debug code (compiler debug flags come from the CMake configuration)" OFF)
option(LIBVPX_ENABLE_PROFILE "Build libvpx with profile instrumentation" OFF)
option(LIBVPX_ENABLE_GPROF "Build libvpx with gprof instrumentation" OFF)
option(LIBVPX_ENABLE_GCOV "Build libvpx with gcov instrumentation" OFF)
option(LIBVPX_ENABLE_FP_MB_STATS "Enable first-pass macroblock statistics" OFF)
option(LIBVPX_ENABLE_EMULATE_HARDWARE "Enable hardware emulation" OFF)
option(LIBVPX_ENABLE_ALWAYS_ADJUST_BPM "Always adjust the bitstream probability model" OFF)
option(LIBVPX_ENABLE_COLLECT_COMPONENT_TIMING "Collect component timing" OFF)
option(LIBVPX_ENABLE_STATIC_MSVCRT "Use the static MSVC runtime" OFF)
option(LIBVPX_INSTALL "Install libvpx and its public headers" OFF)

# This source distribution intentionally contains the codec libraries only.
# Do not expose configure switches which cannot have their upstream meaning.
if(LIBVPX_ENABLE_WEBM_IO OR LIBVPX_ENABLE_LIBYUV)
  message(FATAL_ERROR
    "LIBVPX_ENABLE_WEBM_IO and LIBVPX_ENABLE_LIBYUV require the pruned "
    "libvpx tools/examples sources and are not supported by this port.")
endif()
if(MSVC AND (LIBVPX_ENABLE_PROFILE OR LIBVPX_ENABLE_GPROF OR LIBVPX_ENABLE_GCOV))
  message(FATAL_ERROR
    "The selected libvpx instrumentation mode is not supported by MSVC.")
endif()

if(LIBVPX_ENABLE_INTERNAL_STATS)
  set(LIBVPX_ENABLE_VP9_POSTPROC ON)
endif()
if(LIBVPX_ENABLE_NON_GREEDY_MV)
  set(LIBVPX_ENABLE_EXPERIMENTAL ON)
endif()
if(LIBVPX_ENABLE_COEFFICIENT_RANGE_CHECKING)
  set(LIBVPX_BUILD_VP8_ENCODER OFF)
  set(LIBVPX_BUILD_VP9_ENCODER OFF)
endif()

set(_libvpx_build_vp8_encoder OFF)
set(_libvpx_build_vp8_decoder OFF)
set(_libvpx_build_vp9_encoder OFF)
set(_libvpx_build_vp9_decoder OFF)
if(LIBVPX_BUILD_VP8 AND LIBVPX_BUILD_VP8_ENCODER)
  set(_libvpx_build_vp8_encoder ON)
endif()
if(LIBVPX_BUILD_VP8 AND LIBVPX_BUILD_VP8_DECODER)
  set(_libvpx_build_vp8_decoder ON)
endif()
if(LIBVPX_BUILD_VP9 AND LIBVPX_BUILD_VP9_ENCODER)
  set(_libvpx_build_vp9_encoder ON)
endif()
if(LIBVPX_BUILD_VP9 AND LIBVPX_BUILD_VP9_DECODER)
  set(_libvpx_build_vp9_decoder ON)
endif()
if(_libvpx_build_vp8_encoder OR _libvpx_build_vp8_decoder)
  set(LIBVPX_BUILD_VP8 ON)
else()
  set(LIBVPX_BUILD_VP8 OFF)
endif()
if(_libvpx_build_vp9_encoder OR _libvpx_build_vp9_decoder)
  set(LIBVPX_BUILD_VP9 ON)
else()
  set(LIBVPX_BUILD_VP9 OFF)
endif()
if(_libvpx_build_vp8_encoder OR _libvpx_build_vp9_encoder)
  set(LIBVPX_BUILD_ENCODER ON)
else()
  set(LIBVPX_BUILD_ENCODER OFF)
endif()
if(_libvpx_build_vp8_decoder OR _libvpx_build_vp9_decoder)
  set(LIBVPX_BUILD_DECODER ON)
else()
  set(LIBVPX_BUILD_DECODER OFF)
endif()
if(NOT LIBVPX_BUILD_ENCODER AND NOT LIBVPX_BUILD_DECODER)
  message(FATAL_ERROR "At least one VP8/VP9 encoder or decoder must be enabled.")
endif()
if(LIBVPX_BUILD_ENCODER)
  enable_language(CXX)
endif()

if(LIBVPX_ENABLE_POSTPROC_VISUALIZER AND NOT LIBVPX_ENABLE_POSTPROC)
  message(FATAL_ERROR
    "LIBVPX_ENABLE_POSTPROC_VISUALIZER requires LIBVPX_ENABLE_POSTPROC=ON.")
endif()
if(LIBVPX_SIZE_LIMIT)
  if(NOT LIBVPX_SIZE_LIMIT MATCHES "^([1-9][0-9]*)x([1-9][0-9]*)$")
    message(FATAL_ERROR "LIBVPX_SIZE_LIMIT must have the form WIDTHxHEIGHT.")
  endif()
  set(_libvpx_decode_width "${CMAKE_MATCH_1}")
  set(_libvpx_decode_height "${CMAKE_MATCH_2}")
  if(_libvpx_decode_width GREATER_EQUAL 65536 OR _libvpx_decode_height GREATER_EQUAL 65536)
    message(FATAL_ERROR "LIBVPX_SIZE_LIMIT dimensions must be less than 65536.")
  endif()
  set(LIBVPX_CONFIG_SIZE_LIMIT 1)
else()
  set(_libvpx_decode_width 0)
  set(_libvpx_decode_height 0)
  set(LIBVPX_CONFIG_SIZE_LIMIT 0)
endif()
set(LIBVPX_DECODE_WIDTH_LIMIT "${_libvpx_decode_width}")
set(LIBVPX_DECODE_HEIGHT_LIMIT "${_libvpx_decode_height}")

set(LIBVPX_RTCD_ARCH generic)
set(VPX_ARCH_ARM 0)
set(VPX_ARCH_AARCH64 0)
set(VPX_ARCH_MIPS 0)
set(VPX_ARCH_X86 0)
set(VPX_ARCH_X86_64 0)
set(VPX_ARCH_PPC 0)
set(VPX_ARCH_LOONGARCH 0)
if(_libvpx_target_arch STREQUAL x86)
  set(LIBVPX_RTCD_ARCH x86)
  set(VPX_ARCH_X86 1)
elseif(_libvpx_target_arch STREQUAL x64)
  set(LIBVPX_RTCD_ARCH x86_64)
  set(VPX_ARCH_X86_64 1)
elseif(_libvpx_target_arch STREQUAL arm32)
  set(LIBVPX_RTCD_ARCH armv7)
  set(VPX_ARCH_ARM 1)
elseif(_libvpx_target_arch STREQUAL arm64)
  set(LIBVPX_RTCD_ARCH arm64)
  set(VPX_ARCH_ARM 1)
  set(VPX_ARCH_AARCH64 1)
elseif(_libvpx_target_arch STREQUAL mips32)
  set(LIBVPX_RTCD_ARCH mips32)
  set(VPX_ARCH_MIPS 1)
elseif(_libvpx_target_arch STREQUAL mips64)
  set(LIBVPX_RTCD_ARCH mips64)
  set(VPX_ARCH_MIPS 1)
elseif(_libvpx_target_arch STREQUAL ppc64le)
  set(LIBVPX_RTCD_ARCH ppc64le)
  set(VPX_ARCH_PPC 1)
elseif(_libvpx_target_arch MATCHES "^loongarch")
  set(LIBVPX_RTCD_ARCH "${_libvpx_target_arch}")
  set(VPX_ARCH_LOONGARCH 1)
endif()

# Runtime CPU detection
if(NOT DEFINED LIBVPX_ENABLE_RUNTIME_CPU_DETECT)
  # Upstream disables generic MIPS runtime detection. It is only enabled for
  # tuned Loongson configurations after MMI/MSA detection, which this port
  # does not currently model.
  if(_libvpx_target_arch STREQUAL mips32 OR _libvpx_target_arch STREQUAL mips64)
    set(LIBVPX_ENABLE_RUNTIME_CPU_DETECT OFF CACHE BOOL
      "Dispatch between architecture-specific implementations at runtime")
  elseif(NOT _libvpx_target_arch STREQUAL generic)
    set(LIBVPX_ENABLE_RUNTIME_CPU_DETECT ON CACHE BOOL
      "Dispatch between architecture-specific implementations at runtime")
  else()
    set(LIBVPX_ENABLE_RUNTIME_CPU_DETECT OFF CACHE BOOL
      "Dispatch between architecture-specific implementations at runtime")
  endif()
endif()

# CPU features
set(LIBVPX_CONFIG_HAVE_MMX 0)
set(LIBVPX_CONFIG_HAVE_SSE 0)
set(LIBVPX_CONFIG_HAVE_SSE2 0)
set(LIBVPX_CONFIG_HAVE_SSE3 0)
set(LIBVPX_CONFIG_HAVE_SSSE3 0)
set(LIBVPX_CONFIG_HAVE_SSE4_1 0)
set(LIBVPX_CONFIG_HAVE_AVX 0)
set(LIBVPX_CONFIG_HAVE_AVX2 0)
set(LIBVPX_CONFIG_HAVE_AVX512 0)
set(LIBVPX_CONFIG_HAVE_NEON 0)
set(LIBVPX_CONFIG_HAVE_NEON_ASM 0)
set(LIBVPX_CONFIG_HAVE_NEON_DOTPROD 0)
set(LIBVPX_CONFIG_HAVE_NEON_I8MM 0)
set(LIBVPX_CONFIG_HAVE_SVE 0)
set(LIBVPX_CONFIG_HAVE_SVE2 0)
set(LIBVPX_CONFIG_HAVE_MIPS32 0)
set(LIBVPX_CONFIG_HAVE_MIPS64 0)
set(LIBVPX_CONFIG_HAVE_DSPR2 0)
set(LIBVPX_CONFIG_HAVE_MSA 0)
set(LIBVPX_CONFIG_HAVE_MMI 0)
set(LIBVPX_CONFIG_HAVE_VSX 0)
set(LIBVPX_CONFIG_HAVE_LSX 0)
set(LIBVPX_CONFIG_HAVE_LASX 0)
if(APPLE)
  set(LIBVPX_CONFIG_HAVE_PRIVATE_EXTERN 1)
else()
  set(LIBVPX_CONFIG_HAVE_PRIVATE_EXTERN 0)
endif()

if(LIBVPX_ENABLE_SIMD AND NOT LIBVPX_ASM_MODE STREQUAL "OFF"
  AND (_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64))
  if(MSVC)
    if(_libvpx_target_arch STREQUAL x64)
      set(_libvpx_has_sse2 ON)
    else()
      check_c_compiler_flag("/arch:SSE2" _libvpx_has_sse2)
    endif()
    check_c_source_compiles([=[
#include <pmmintrin.h>
int main(void) {
  __m128 value = _mm_setzero_ps();
  value = _mm_hadd_ps(value, value);
  return (int)_mm_cvtss_f32(value);
}
    ]=] _libvpx_has_sse3)
    check_c_source_compiles([=[
#include <tmmintrin.h>
int main(void) {
  __m128i value = _mm_setzero_si128();
  value = _mm_abs_epi8(value);
  return _mm_cvtsi128_si32(value);
}
    ]=] _libvpx_has_ssse3)
    check_c_source_compiles([=[
#include <smmintrin.h>
int main(void) {
  __m128i value = _mm_setzero_si128();
  value = _mm_min_epi32(value, value);
  return _mm_cvtsi128_si32(value);
}
    ]=] _libvpx_has_sse4_1)
    check_c_compiler_flag("/arch:AVX" _libvpx_has_avx)
    check_c_compiler_flag("/arch:AVX2" _libvpx_has_avx2)
    check_c_compiler_flag("/arch:AVX512" _libvpx_has_avx512)
  else()
    check_c_compiler_flag("-msse2" _libvpx_has_sse2)
    check_c_compiler_flag("-msse3" _libvpx_has_sse3)
    check_c_compiler_flag("-mssse3" _libvpx_has_ssse3)
    check_c_compiler_flag("-msse4.1" _libvpx_has_sse4_1)
    check_c_compiler_flag("-mavx" _libvpx_has_avx)
    check_c_compiler_flag("-mavx2" _libvpx_has_avx2)
    check_c_compiler_flag("-mavx512f -mavx512cd -mavx512bw -mavx512dq -mavx512vl"
      _libvpx_has_avx512)
  endif()
  set(LIBVPX_CONFIG_HAVE_MMX 1)
  set(LIBVPX_CONFIG_HAVE_SSE 1)
  if(_libvpx_has_sse2)
    set(LIBVPX_CONFIG_HAVE_SSE2 1)
  endif()
  if(_libvpx_has_sse3)
    set(LIBVPX_CONFIG_HAVE_SSE3 1)
  endif()
  if(_libvpx_has_ssse3)
    set(LIBVPX_CONFIG_HAVE_SSSE3 1)
  endif()
  if(_libvpx_has_sse4_1)
    set(LIBVPX_CONFIG_HAVE_SSE4_1 1)
  endif()
  if(_libvpx_has_avx)
    set(LIBVPX_CONFIG_HAVE_AVX 1)
  endif()
  if(_libvpx_has_avx2)
    set(LIBVPX_CONFIG_HAVE_AVX2 1)
  endif()
  if(_libvpx_has_avx512)
    set(LIBVPX_CONFIG_HAVE_AVX512 1)
  endif()
  # Match configure.sh: x86 extensions form an ordered dependency chain.
  # Once one level is unavailable all higher RTCD implementations are off.
  if(NOT LIBVPX_CONFIG_HAVE_SSE2)
    set(LIBVPX_CONFIG_HAVE_SSE3 0)
    set(LIBVPX_CONFIG_HAVE_SSSE3 0)
    set(LIBVPX_CONFIG_HAVE_SSE4_1 0)
    set(LIBVPX_CONFIG_HAVE_AVX 0)
    set(LIBVPX_CONFIG_HAVE_AVX2 0)
    set(LIBVPX_CONFIG_HAVE_AVX512 0)
  elseif(NOT LIBVPX_CONFIG_HAVE_SSE3)
    set(LIBVPX_CONFIG_HAVE_SSSE3 0)
    set(LIBVPX_CONFIG_HAVE_SSE4_1 0)
    set(LIBVPX_CONFIG_HAVE_AVX 0)
    set(LIBVPX_CONFIG_HAVE_AVX2 0)
    set(LIBVPX_CONFIG_HAVE_AVX512 0)
  elseif(NOT LIBVPX_CONFIG_HAVE_SSSE3)
    set(LIBVPX_CONFIG_HAVE_SSE4_1 0)
    set(LIBVPX_CONFIG_HAVE_AVX 0)
    set(LIBVPX_CONFIG_HAVE_AVX2 0)
    set(LIBVPX_CONFIG_HAVE_AVX512 0)
  elseif(NOT LIBVPX_CONFIG_HAVE_SSE4_1)
    set(LIBVPX_CONFIG_HAVE_AVX 0)
    set(LIBVPX_CONFIG_HAVE_AVX2 0)
    set(LIBVPX_CONFIG_HAVE_AVX512 0)
  elseif(NOT LIBVPX_CONFIG_HAVE_AVX)
    set(LIBVPX_CONFIG_HAVE_AVX2 0)
    set(LIBVPX_CONFIG_HAVE_AVX512 0)
  elseif(NOT LIBVPX_CONFIG_HAVE_AVX2)
    set(LIBVPX_CONFIG_HAVE_AVX512 0)
  endif()
endif()

if(LIBVPX_ENABLE_SIMD AND _libvpx_target_arch STREQUAL arm32)
  if(MSVC)
    set(_libvpx_has_mfpu_neon OFF)
  else()
    check_c_compiler_flag("-mfpu=neon" _libvpx_has_mfpu_neon)
  endif()
  if(_libvpx_has_mfpu_neon)
    set(LIBVPX_CONFIG_HAVE_NEON 1)
  endif()
endif()
if(LIBVPX_ENABLE_SIMD AND _libvpx_target_arch STREQUAL arm64)
  if(MSVC)
    check_c_source_compiles([=[
#include <arm_neon.h>
int main(void) { return 0; }
    ]=] _libvpx_has_arm_neon_h)
    if(_libvpx_has_arm_neon_h)
      set(_libvpx_has_neon ON)
    else()
      check_c_source_compiles([=[
#include <arm64_neon.h>
int main(void) { return 0; }
      ]=] _libvpx_has_arm64_neon_h)
      if(NOT _libvpx_has_arm64_neon_h)
        message(FATAL_ERROR
          "Neither a usable arm_neon.h nor arm64_neon.h was found for MSVC ARM64.")
      endif()
      configure_file("${LIBVPX_ROOT}/cmake/templates/arm_neon.h.in"
        "${LIBVPX_BINARY_DIR}/arm_neon.h" COPYONLY)
      set(_libvpx_has_neon ON)
    endif()
  else()
    check_c_compiler_flag("-march=armv8-a" _libvpx_has_neon)
  endif()
  if(_libvpx_has_neon)
    set(LIBVPX_CONFIG_HAVE_NEON 1)
  endif()
  if(MSVC)
    set(_libvpx_has_dotprod OFF)
    set(_libvpx_has_i8mm OFF)
    set(_libvpx_has_sve OFF)
    set(_libvpx_has_sve2 OFF)
  else()
    check_c_compiler_flag("-march=armv8.2-a+dotprod" _libvpx_has_dotprod)
    check_c_compiler_flag("-march=armv8.2-a+dotprod+i8mm" _libvpx_has_i8mm)
    check_c_compiler_flag("-march=armv8.2-a+dotprod+i8mm+sve" _libvpx_has_sve)
    check_c_compiler_flag("-march=armv9-a+sve2" _libvpx_has_sve2)
  endif()
  if(_libvpx_has_dotprod)
    set(LIBVPX_CONFIG_HAVE_NEON_DOTPROD 1)
  endif()
  if(_libvpx_has_i8mm)
    set(LIBVPX_CONFIG_HAVE_NEON_I8MM 1)
  endif()
  if(_libvpx_has_sve)
    set(LIBVPX_CONFIG_HAVE_SVE 1)
  endif()
  if(_libvpx_has_sve2)
    set(LIBVPX_CONFIG_HAVE_SVE2 1)
  endif()
  if(NOT LIBVPX_CONFIG_HAVE_NEON_DOTPROD)
    set(LIBVPX_CONFIG_HAVE_NEON_I8MM 0)
    set(LIBVPX_CONFIG_HAVE_SVE 0)
    set(LIBVPX_CONFIG_HAVE_SVE2 0)
  elseif(NOT LIBVPX_CONFIG_HAVE_NEON_I8MM)
    set(LIBVPX_CONFIG_HAVE_SVE 0)
    set(LIBVPX_CONFIG_HAVE_SVE2 0)
  elseif(NOT LIBVPX_CONFIG_HAVE_SVE)
    set(LIBVPX_CONFIG_HAVE_SVE2 0)
  endif()
  if(LIBVPX_CONFIG_HAVE_SVE)
    set(_libvpx_required_flags "${CMAKE_REQUIRED_FLAGS}")
    string(APPEND CMAKE_REQUIRED_FLAGS
      " -march=armv8.2-a+dotprod+i8mm+sve")
    check_c_source_compiles([=[
#ifndef __ARM_NEON_SVE_BRIDGE
#error __ARM_NEON_SVE_BRIDGE is required
#endif
#include <arm_sve.h>
#include <arm_neon_sve_bridge.h>
    ]=] _libvpx_has_neon_sve_bridge)
    if(_libvpx_has_neon_sve_bridge)
      check_c_source_compiles([=[
#include <arm_sve.h>
void other(void);
svfloat32_t libvpx_sve_function(svfloat32_t value) {
  other();
  return value;
}
      ]=] _libvpx_has_sve_aapcs)
    endif()
    set(CMAKE_REQUIRED_FLAGS "${_libvpx_required_flags}")
    if(NOT _libvpx_has_neon_sve_bridge OR NOT _libvpx_has_sve_aapcs)
      set(LIBVPX_CONFIG_HAVE_SVE 0)
      set(LIBVPX_CONFIG_HAVE_SVE2 0)
    endif()
  endif()
endif()

if(LIBVPX_ENABLE_SIMD
  AND (_libvpx_target_arch STREQUAL mips32 OR _libvpx_target_arch STREQUAL mips64))
  if(_libvpx_target_arch STREQUAL mips32)
    check_c_compiler_flag("-mips32r2 -mdspr2" _libvpx_has_dspr2)
  else()
    set(_libvpx_has_dspr2 OFF)
  endif()
  check_c_compiler_flag("-mmsa" _libvpx_has_msa)
  check_c_compiler_flag("-march=loongson3a" _libvpx_has_mmi)
  if(_libvpx_has_dspr2)
    set(LIBVPX_CONFIG_HAVE_DSPR2 1)
  endif()
  if(_libvpx_has_msa)
    set(LIBVPX_CONFIG_HAVE_MSA 1)
  endif()
  if(_libvpx_has_mmi)
    set(LIBVPX_CONFIG_HAVE_MMI 1)
  endif()
endif()
if(_libvpx_target_arch STREQUAL ppc64le AND LIBVPX_ENABLE_VSX)
  check_c_compiler_flag("-maltivec -mvsx" _libvpx_has_vsx)
  if(_libvpx_has_vsx)
    set(LIBVPX_CONFIG_HAVE_VSX 1)
  endif()
endif()
if(_libvpx_target_arch MATCHES "^loongarch" AND LIBVPX_ENABLE_SIMD)
  check_c_compiler_flag("-mlsx" _libvpx_has_lsx)
  check_c_compiler_flag("-mlasx" _libvpx_has_lasx)
  if(_libvpx_has_lsx)
    set(LIBVPX_CONFIG_HAVE_LSX 1)
  endif()
  if(_libvpx_has_lasx)
    set(LIBVPX_CONFIG_HAVE_LASX 1)
  endif()
endif()

if(_libvpx_target_arch STREQUAL mips32)
  set(LIBVPX_CONFIG_HAVE_MIPS32 1)
elseif(_libvpx_target_arch STREQUAL mips64)
  set(LIBVPX_CONFIG_HAVE_MIPS64 1)
endif()

find_program(LIBVPX_ASSEMBLER NAMES yasm nasm)
set(_libvpx_use_x86_asm OFF)
if(LIBVPX_ENABLE_ASM AND LIBVPX_ENABLE_SIMD AND LIBVPX_ASSEMBLER
  AND (_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64))
  set(_libvpx_use_x86_asm ON)
endif()
if(LIBVPX_ASM_MODE STREQUAL "ON"
  AND (_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64)
  AND LIBVPX_ENABLE_SIMD AND NOT _libvpx_use_x86_asm)
  message(FATAL_ERROR
    "LIBVPX_ASM_MODE=ON requires NASM or YASM for an x86/x64 build.")
elseif(LIBVPX_ASM_MODE STREQUAL "AUTO"
  AND (_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64)
  AND LIBVPX_ENABLE_SIMD AND NOT _libvpx_use_x86_asm)
  message(STATUS
    "libvpx: no NASM/YASM found; disabling x86 SIMD implementations")
endif()
if(_libvpx_use_x86_asm)
  set(LIBVPX_CONFIG_HAVE_X86_ASM 1)
  set(CMAKE_ASM_NASM_COMPILER "${LIBVPX_ASSEMBLER}" CACHE FILEPATH
    "Assembler used for libvpx x86 sources" FORCE)
  enable_language(ASM_NASM)
else()
  set(LIBVPX_CONFIG_HAVE_X86_ASM 0)
  # libvpx's x86 RTCD feature switches cover both intrinsic and assembly
  # implementations. Enabling them without the corresponding ASM objects can
  # produce an archive successfully but leaves unresolved symbols for users.
  if(_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64)
    foreach(_libvpx_x86_feature IN ITEMS
      MMX SSE SSE2 SSE3 SSSE3 SSE4_1 AVX AVX2 AVX512)
      set(LIBVPX_CONFIG_HAVE_${_libvpx_x86_feature} 0)
    endforeach()
  endif()
endif()

# ARMv7 assembly uses ADS syntax and is converted to GAS syntax.
set(_libvpx_use_arm_asm OFF)
if(LIBVPX_ENABLE_ASM AND LIBVPX_CONFIG_HAVE_NEON
  AND _libvpx_target_arch STREQUAL arm32 AND NOT MSVC)
  set(_libvpx_use_arm_asm ON)
  set(LIBVPX_CONFIG_HAVE_NEON_ASM 1)
  enable_language(ASM)
endif()

if(_libvpx_target_arch STREQUAL arm32 AND LIBVPX_CONFIG_HAVE_NEON AND NOT MSVC)
  set(_libvpx_arm_neon_cflags -mfpu=neon)
endif()
if(_libvpx_target_arch STREQUAL arm64 AND LIBVPX_CONFIG_HAVE_NEON AND NOT MSVC)
  set(_libvpx_arm64_cflags -march=armv8-a)
endif()
if(LIBVPX_CONFIG_HAVE_VSX)
  set(_libvpx_vsx_cflags -maltivec -mvsx)
endif()
if(LIBVPX_CONFIG_HAVE_MSA)
  set(_libvpx_msa_cflags -mmsa)
endif()
if(LIBVPX_CONFIG_HAVE_MMI)
  set(_libvpx_mmi_cflags -march=loongson3a)
endif()
if(LIBVPX_CONFIG_HAVE_DSPR2)
  set(_libvpx_dspr2_cflags -mips32r2 -mdspr2)
endif()
if(LIBVPX_CONFIG_HAVE_LSX)
  set(_libvpx_lsx_cflags -mlsx)
endif()
if(LIBVPX_CONFIG_HAVE_LASX)
  set(_libvpx_lasx_cflags -mlasx)
endif()

if(WIN32)
  set(LIBVPX_HAVE_PTHREAD_H 0)
  set(LIBVPX_HAVE_PTHREAD_SETNAME_NP 0)
  set(LIBVPX_HAVE_UNISTD_H 0)
else()
  check_include_file("pthread.h" LIBVPX_HAVE_PTHREAD_H)
  check_include_file("unistd.h" LIBVPX_HAVE_UNISTD_H)
  if(LIBVPX_HAVE_PTHREAD_H AND LIBVPX_ENABLE_MULTITHREAD)
    find_package(Threads REQUIRED)
    set(_libvpx_had_required_definitions OFF)
    set(_libvpx_had_required_libraries OFF)
    if(DEFINED CMAKE_REQUIRED_DEFINITIONS)
      set(_libvpx_had_required_definitions ON)
      set(_libvpx_saved_required_definitions
        "${CMAKE_REQUIRED_DEFINITIONS}")
    endif()
    if(DEFINED CMAKE_REQUIRED_LIBRARIES)
      set(_libvpx_had_required_libraries ON)
      set(_libvpx_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
    endif()
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    list(APPEND CMAKE_REQUIRED_LIBRARIES Threads::Threads)
    check_symbol_exists(pthread_setname_np "pthread.h"
      LIBVPX_HAVE_PTHREAD_SETNAME_NP)
    if(_libvpx_had_required_definitions)
      set(CMAKE_REQUIRED_DEFINITIONS
        "${_libvpx_saved_required_definitions}")
    else()
      unset(CMAKE_REQUIRED_DEFINITIONS)
    endif()
    if(_libvpx_had_required_libraries)
      set(CMAKE_REQUIRED_LIBRARIES
        "${_libvpx_saved_required_libraries}")
    else()
      unset(CMAKE_REQUIRED_LIBRARIES)
    endif()
  endif()
endif()
foreach(_libvpx_check_var IN ITEMS
  LIBVPX_HAVE_PTHREAD_H LIBVPX_HAVE_PTHREAD_SETNAME_NP LIBVPX_HAVE_UNISTD_H)
  if(NOT ${_libvpx_check_var})
    set(${_libvpx_check_var} 0)
  endif()
endforeach()

if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  set(LIBVPX_CONFIG_BIG_ENDIAN 1)
else()
  set(LIBVPX_CONFIG_BIG_ENDIAN 0)
endif()
if(MSVC)
  set(LIBVPX_CONFIG_GCC 0)
  set(LIBVPX_CONFIG_MSVS 1)
else()
  set(LIBVPX_CONFIG_GCC 1)
  set(LIBVPX_CONFIG_MSVS 0)
endif()

# RTCD configuration
set(LIBVPX_CONFIG_VP8 0)
set(LIBVPX_CONFIG_VP9 0)
set(LIBVPX_CONFIG_VP8_ENCODER 0)
set(LIBVPX_CONFIG_VP9_ENCODER 0)
set(LIBVPX_CONFIG_VP8_DECODER 0)
set(LIBVPX_CONFIG_VP9_DECODER 0)
if(LIBVPX_BUILD_VP8)
  set(LIBVPX_CONFIG_VP8 1)
  if(_libvpx_build_vp8_encoder)
    set(LIBVPX_CONFIG_VP8_ENCODER 1)
  endif()
  if(_libvpx_build_vp8_decoder)
    set(LIBVPX_CONFIG_VP8_DECODER 1)
  endif()
endif()
if(LIBVPX_BUILD_VP9)
  set(LIBVPX_CONFIG_VP9 1)
  if(_libvpx_build_vp9_encoder)
    set(LIBVPX_CONFIG_VP9_ENCODER 1)
  endif()
  if(_libvpx_build_vp9_decoder)
    set(LIBVPX_CONFIG_VP9_DECODER 1)
  endif()
endif()
if(LIBVPX_BUILD_ENCODER)
  set(LIBVPX_CONFIG_ENCODERS 1)
else()
  set(LIBVPX_CONFIG_ENCODERS 0)
endif()
if(LIBVPX_BUILD_DECODER)
  set(LIBVPX_CONFIG_DECODERS 1)
else()
  set(LIBVPX_CONFIG_DECODERS 0)
endif()

set(LIBVPX_CONFIG_DEPENDENCY_TRACKING 1)
set(LIBVPX_CONFIG_EXTERNAL_BUILD 0)
set(LIBVPX_CONFIG_INSTALL_DOCS 0)
set(LIBVPX_CONFIG_INSTALL_BINS 0)
set(LIBVPX_CONFIG_INSTALL_LIBS 0)
set(LIBVPX_CONFIG_INSTALL_SRCS 0)
set(LIBVPX_CONFIG_DEBUG 0)
set(LIBVPX_CONFIG_GPROF 0)
set(LIBVPX_CONFIG_GCOV 0)
set(LIBVPX_CONFIG_RVCT 0)
if(LIBVPX_ENABLE_PIC OR LIBVPX_BUILD_SHARED)
  set(LIBVPX_CONFIG_PIC 1)
else()
  set(LIBVPX_CONFIG_PIC 0)
endif()
set(LIBVPX_CONFIG_CODEC_SRCS 0)
set(LIBVPX_CONFIG_DEBUG_LIBS 0)
if(VPX_ARCH_MIPS AND LIBVPX_ENABLE_SIMD)
  set(LIBVPX_CONFIG_DEQUANT_TOKENS 1)
  set(LIBVPX_CONFIG_DC_RECON 1)
else()
  set(LIBVPX_CONFIG_DEQUANT_TOKENS 0)
  set(LIBVPX_CONFIG_DC_RECON 0)
endif()
set(LIBVPX_CONFIG_POSTPROC 0)
set(LIBVPX_CONFIG_VP9_POSTPROC 0)
set(LIBVPX_CONFIG_MULTITHREAD 0)
set(LIBVPX_CONFIG_RUNTIME_CPU_DETECT 0)
set(LIBVPX_CONFIG_STATIC_MSVCRT 0)
set(LIBVPX_CONFIG_SPATIAL_RESAMPLING 0)
set(LIBVPX_CONFIG_REALTIME_ONLY 0)
set(LIBVPX_CONFIG_ONTHEFLY_BITPACKING 0)
set(LIBVPX_CONFIG_ERROR_CONCEALMENT 0)
set(LIBVPX_CONFIG_SHARED 0)
set(LIBVPX_CONFIG_STATIC 0)
set(LIBVPX_CONFIG_SMALL 0)
set(LIBVPX_CONFIG_POSTPROC_VISUALIZER 0)
set(LIBVPX_CONFIG_WEBM_IO 0)
set(LIBVPX_CONFIG_LIBYUV 0)
set(LIBVPX_CONFIG_EXPERIMENTAL 0)
if(LIBVPX_ENABLE_DEBUG)
  set(LIBVPX_CONFIG_DEBUG 1)
endif()
if(LIBVPX_ENABLE_GPROF)
  set(LIBVPX_CONFIG_GPROF 1)
endif()
if(LIBVPX_ENABLE_GCOV)
  set(LIBVPX_CONFIG_GCOV 1)
endif()
if(LIBVPX_ENABLE_STATIC_MSVCRT)
  set(LIBVPX_CONFIG_STATIC_MSVCRT 1)
endif()
if(LIBVPX_ENABLE_POSTPROC_VISUALIZER)
  set(LIBVPX_CONFIG_POSTPROC_VISUALIZER 1)
endif()
set(LIBVPX_CONFIG_OS_SUPPORT 1)
set(LIBVPX_CONFIG_UNIT_TESTS 0)
if(LIBVPX_ENABLE_WEBM_IO)
  set(LIBVPX_CONFIG_WEBM_IO 1)
endif()
if(LIBVPX_ENABLE_LIBYUV)
  set(LIBVPX_CONFIG_LIBYUV 1)
endif()
set(LIBVPX_CONFIG_DECODE_PERF_TESTS 0)
set(LIBVPX_CONFIG_ENCODE_PERF_TESTS 0)
set(LIBVPX_CONFIG_MULTI_RES_ENCODING 0)
set(LIBVPX_CONFIG_TEMPORAL_DENOISING 0)
set(LIBVPX_CONFIG_VP9_TEMPORAL_DENOISING 0)
set(LIBVPX_CONFIG_COEFFICIENT_RANGE_CHECKING 0)
set(LIBVPX_CONFIG_VP9_HIGHBITDEPTH 0)
set(LIBVPX_CONFIG_BETTER_HW_COMPATIBILITY 0)
if(LIBVPX_ENABLE_EXPERIMENTAL)
  set(LIBVPX_CONFIG_EXPERIMENTAL 1)
endif()
set(LIBVPX_CONFIG_ALWAYS_ADJUST_BPM 0)
if(LIBVPX_ENABLE_ALWAYS_ADJUST_BPM)
  set(LIBVPX_CONFIG_ALWAYS_ADJUST_BPM 1)
endif()
set(LIBVPX_CONFIG_BITSTREAM_DEBUG 0)
set(LIBVPX_CONFIG_MISMATCH_DEBUG 0)
set(LIBVPX_CONFIG_FP_MB_STATS 0)
if(LIBVPX_ENABLE_FP_MB_STATS)
  set(LIBVPX_CONFIG_FP_MB_STATS 1)
endif()
set(LIBVPX_CONFIG_EMULATE_HARDWARE 0)
if(LIBVPX_ENABLE_EMULATE_HARDWARE)
  set(LIBVPX_CONFIG_EMULATE_HARDWARE 1)
endif()
set(LIBVPX_CONFIG_NON_GREEDY_MV 0)
set(LIBVPX_CONFIG_COLLECT_COMPONENT_TIMING 0)
if(LIBVPX_ENABLE_COLLECT_COMPONENT_TIMING)
  set(LIBVPX_CONFIG_COLLECT_COMPONENT_TIMING 1)
endif()
if(LIBVPX_ENABLE_POSTPROC)
  set(LIBVPX_CONFIG_POSTPROC 1)
endif()
if(LIBVPX_ENABLE_VP9_POSTPROC)
  set(LIBVPX_CONFIG_VP9_POSTPROC 1)
endif()
if(LIBVPX_ENABLE_MULTITHREAD)
  set(LIBVPX_CONFIG_MULTITHREAD 1)
endif()
if(LIBVPX_ENABLE_RUNTIME_CPU_DETECT)
  set(LIBVPX_CONFIG_RUNTIME_CPU_DETECT 1)
endif()
if(LIBVPX_ENABLE_SPATIAL_RESAMPLING)
  set(LIBVPX_CONFIG_SPATIAL_RESAMPLING 1)
endif()
if(LIBVPX_ENABLE_REALTIME_ONLY)
  set(LIBVPX_CONFIG_REALTIME_ONLY 1)
endif()
if(LIBVPX_ENABLE_ONTHEFLY_BITPACKING)
  set(LIBVPX_CONFIG_ONTHEFLY_BITPACKING 1)
endif()
if(LIBVPX_ENABLE_ERROR_CONCEALMENT)
  set(LIBVPX_CONFIG_ERROR_CONCEALMENT 1)
endif()
if(LIBVPX_BUILD_SHARED)
  set(LIBVPX_CONFIG_SHARED 1)
endif()
if(LIBVPX_BUILD_STATIC)
  set(LIBVPX_CONFIG_STATIC 1)
endif()
if(LIBVPX_ENABLE_SMALL)
  set(LIBVPX_CONFIG_SMALL 1)
endif()
if(LIBVPX_ENABLE_INTERNAL_STATS)
  set(LIBVPX_CONFIG_INTERNAL_STATS 1)
else()
  set(LIBVPX_CONFIG_INTERNAL_STATS 0)
endif()
if(LIBVPX_ENABLE_MULTI_RES_ENCODING)
  set(LIBVPX_CONFIG_MULTI_RES_ENCODING 1)
endif()
if(LIBVPX_ENABLE_TEMPORAL_DENOISING)
  set(LIBVPX_CONFIG_TEMPORAL_DENOISING 1)
endif()
if(LIBVPX_ENABLE_VP9_TEMPORAL_DENOISING)
  set(LIBVPX_CONFIG_VP9_TEMPORAL_DENOISING 1)
endif()
if(LIBVPX_ENABLE_COEFFICIENT_RANGE_CHECKING)
  set(LIBVPX_CONFIG_COEFFICIENT_RANGE_CHECKING 1)
endif()
if(LIBVPX_ENABLE_BETTER_HW_COMPATIBILITY)
  set(LIBVPX_CONFIG_BETTER_HW_COMPATIBILITY 1)
endif()
if(LIBVPX_ENABLE_NON_GREEDY_MV)
  set(LIBVPX_CONFIG_NON_GREEDY_MV 1)
endif()
if(LIBVPX_ENABLE_BITSTREAM_DEBUG)
  set(LIBVPX_CONFIG_BITSTREAM_DEBUG 1)
endif()
if(LIBVPX_ENABLE_MISMATCH_DEBUG)
  set(LIBVPX_CONFIG_MISMATCH_DEBUG 1)
endif()
if(LIBVPX_ENABLE_VP9_HIGHBITDEPTH)
  set(LIBVPX_CONFIG_VP9_HIGHBITDEPTH 1)
endif()

# Source selection
#
# The rules below are a CMake translation of libs.mk, vpx_dsp/vpx_dsp.mk,
# vpx_scale/vpx_scale.mk, vpx_ports/vpx_ports.mk, vp8/vp8_common.mk,
# vp8/vp8cx.mk, vp8/vp8dx.mk, vp9/vp9_common.mk, vp9/vp9cx.mk and
# vp9/vp9dx.mk. The checked-in inventory is authoritative; globs only detect
# newly vendored files and never add sources to a target.
include("${LIBVPX_ROOT}/cmake/LibvpxSources.cmake")
file(GLOB_RECURSE _libvpx_current_c CONFIGURE_DEPENDS RELATIVE "${LIBVPX_ROOT}"
  "${LIBVPX_ROOT}/vpx/*.c"
  "${LIBVPX_ROOT}/vpx_mem/*.c"
  "${LIBVPX_ROOT}/vpx_scale/*.c"
  "${LIBVPX_ROOT}/vpx_ports/*.c"
  "${LIBVPX_ROOT}/vpx_dsp/*.c"
  "${LIBVPX_ROOT}/vpx_util/*.c"
  "${LIBVPX_ROOT}/vp8/*.c"
  "${LIBVPX_ROOT}/vp9/*.c")
foreach(_source IN LISTS _libvpx_current_c)
  list(FIND LIBVPX_C_SOURCES "${_source}" _source_index)
  if(_source_index EQUAL -1)
    message(FATAL_ERROR
      "New libvpx C source is not listed in cmake/LibvpxSources.cmake: ${_source}")
  endif()
endforeach()
file(GLOB_RECURSE _libvpx_current_asm CONFIGURE_DEPENDS RELATIVE "${LIBVPX_ROOT}"
  "${LIBVPX_ROOT}/vpx_ports/*.asm"
  "${LIBVPX_ROOT}/vpx_dsp/*.asm"
  "${LIBVPX_ROOT}/vp8/*.asm"
  "${LIBVPX_ROOT}/vp9/*.asm")
foreach(_source IN LISTS _libvpx_current_asm)
  list(FIND LIBVPX_ASM_SOURCES "${_source}" _source_index)
  if(_source_index EQUAL -1)
    message(FATAL_ERROR
      "New libvpx assembly source is not listed in cmake/LibvpxSources.cmake: ${_source}")
  endif()
endforeach()
file(GLOB_RECURSE _libvpx_current_cxx CONFIGURE_DEPENDS RELATIVE "${LIBVPX_ROOT}"
  "${LIBVPX_ROOT}/vp8/*.cc" "${LIBVPX_ROOT}/vp9/*.cc")
foreach(_source IN LISTS _libvpx_current_cxx)
  list(FIND LIBVPX_CXX_SOURCES "${_source}" _source_index)
  if(_source_index EQUAL -1)
    message(FATAL_ERROR
      "New libvpx C++ source is not listed in cmake/LibvpxSources.cmake: ${_source}")
  endif()
endforeach()

set(_libvpx_sources)
foreach(_source IN LISTS LIBVPX_C_SOURCES)
  list(APPEND _libvpx_sources "${LIBVPX_ROOT}/${_source}")
endforeach()
list(FILTER _libvpx_sources EXCLUDE REGEX "\\.cc$")
list(FILTER _libvpx_sources EXCLUDE REGEX "/(x86|arm|mips|ppc|loongarch)/")
list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_ports/emms_mmx\\.c$")
list(FILTER _libvpx_sources EXCLUDE REGEX "/(aarch32|aarch64|loongarch|mips|ppc)_cpudetect\\.c$")
list(FILTER _libvpx_sources EXCLUDE REGEX
  "/(vp8/common/(context|debugmodes)|vp9/common/vp9_debugmodes)\\.c$")

if(NOT LIBVPX_BUILD_VP8)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/")
endif()
if(NOT LIBVPX_BUILD_VP9)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/")
endif()
if(NOT _libvpx_build_vp8_encoder)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/encoder/")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/vp8_cx_iface\\.c$")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/common/vp8_skin_detection\\.c$")
endif()
if(NOT _libvpx_build_vp9_encoder)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/encoder/")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/vp9_cx_iface\\.c$")
endif()
if(NOT LIBVPX_BUILD_ENCODER)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/(vp8|vp9)/encoder/")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/(vp8|vp9)/vp[89]_cx_iface\\.c$")
  list(FILTER _libvpx_sources EXCLUDE REGEX
    "/vpx_dsp/(bitwriter|bitwriter_buffer|psnr|sse|subtract|sum_squares)\\.c$")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/common/vp8_skin_detection\\.c$")
endif()
if(NOT _libvpx_build_vp8_decoder)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/decoder/")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/vp8_dx_iface\\.c$")
endif()
if(NOT _libvpx_build_vp9_decoder)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/decoder/")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/vp9_dx_iface\\.c$")
endif()
if(NOT LIBVPX_BUILD_DECODER)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/(vp8|vp9)/decoder/")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/(vp8|vp9)/vp[89]_dx_iface\\.c$")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_dsp/bitreader(_buffer)?\\.c$")
endif()
if(NOT LIBVPX_BUILD_VP9)
  list(FILTER _libvpx_sources EXCLUDE REGEX
    "/vpx_dsp/(avg|fwd_txfm|inv_txfm|quantize)\\.c$")
elseif(NOT _libvpx_build_vp9_encoder)
  list(FILTER _libvpx_sources EXCLUDE REGEX
    "/vpx_dsp/(avg|fwd_txfm|quantize)\\.c$")
endif()
if(NOT LIBVPX_BUILD_VP9)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_dsp/vpx_convolve\\.c$")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_dsp/loopfilter\\.c$")
endif()
if(NOT LIBVPX_BUILD_ENCODER AND NOT LIBVPX_ENABLE_POSTPROC
  AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_dsp/(sad|variance)\\.c$")
endif()
if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_dsp/(add_noise|deblock)\\.c$")
endif()
if(NOT LIBVPX_ENABLE_POSTPROC)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/common/mfqe\\.c$")
endif()
if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_INTERNAL_STATS)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/common/postproc\\.c$")
endif()
if(NOT LIBVPX_ENABLE_VP9_POSTPROC)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/common/vp9_(mfqe|postproc)\\.c$")
endif()
if(NOT LIBVPX_ENABLE_ERROR_CONCEALMENT)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/decoder/error_concealment\\.c$")
endif()
if(NOT LIBVPX_ENABLE_INTERNAL_STATS)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_dsp/(fastssim|psnrhvs|ssim)\\.c$")
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/encoder/vp9_blockiness\\.c$")
endif()
if(NOT LIBVPX_ENABLE_MULTI_RES_ENCODING)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/encoder/mr_dissim\\.c$")
endif()
if(NOT LIBVPX_ENABLE_NON_GREEDY_MV)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/encoder/vp9_non_greedy_mv\\.c$")
endif()
if(NOT LIBVPX_ENABLE_VP9_TEMPORAL_DENOISING)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp9/encoder/vp9_denoiser\\.c$")
endif()
if(NOT LIBVPX_ENABLE_TEMPORAL_DENOISING)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/encoder/denoising\\.c$")
endif()
if(NOT LIBVPX_ENABLE_SPATIAL_RESAMPLING)
  list(FILTER _libvpx_sources EXCLUDE REGEX
    "/vpx_scale/generic/(gen_scalers|vpx_scale)\\.c$")
endif()
if(NOT LIBVPX_ENABLE_BITSTREAM_DEBUG AND NOT LIBVPX_ENABLE_MISMATCH_DEBUG)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vpx_util/vpx_debug_util\\.c$")
endif()
if(LIBVPX_ENABLE_REALTIME_ONLY)
  list(FILTER _libvpx_sources EXCLUDE REGEX "/vp8/encoder/(firstpass|temporal_filter)\\.c$")
  list(FILTER _libvpx_sources EXCLUDE REGEX
    "/vp9/encoder/(vp9_firstpass|vp9_mbgraph|vp9_temporal_filter|vp9_aq_variance|vp9_aq_360|vp9_aq_complexity|vp9_alt_ref_aq)\\.c$")
endif()

function(libvpx_filter_feature_sources output)
  set(_result "${ARGN}")
  foreach(_source IN LISTS _result)
    get_filename_component(_name "${_source}" NAME)
    set(_drop OFF)
    if(_name MATCHES "avx512" AND NOT LIBVPX_CONFIG_HAVE_AVX512)
      set(_drop ON)
    elseif(_name MATCHES "avx2" AND NOT LIBVPX_CONFIG_HAVE_AVX2)
      set(_drop ON)
    elseif(_name MATCHES "avx" AND NOT LIBVPX_CONFIG_HAVE_AVX)
      set(_drop ON)
    elseif(_name MATCHES "sse4" AND NOT LIBVPX_CONFIG_HAVE_SSE4_1)
      set(_drop ON)
    elseif(_name MATCHES "ssse3" AND NOT LIBVPX_CONFIG_HAVE_SSSE3)
      set(_drop ON)
    elseif(_name MATCHES "sse3" AND NOT _name STREQUAL "copy_sse3.asm"
      AND NOT LIBVPX_CONFIG_HAVE_SSE3)
      set(_drop ON)
    elseif(_name MATCHES "sse2" AND NOT LIBVPX_CONFIG_HAVE_SSE2)
      set(_drop ON)
    elseif(_name MATCHES "mmx" AND NOT LIBVPX_CONFIG_HAVE_MMX)
      set(_drop ON)
    elseif(_name MATCHES "dotprod" AND NOT LIBVPX_CONFIG_HAVE_NEON_DOTPROD)
      set(_drop ON)
    elseif(_name MATCHES "i8mm" AND NOT LIBVPX_CONFIG_HAVE_NEON_I8MM)
      set(_drop ON)
    elseif(_name MATCHES "sve2" AND NOT LIBVPX_CONFIG_HAVE_SVE2)
      set(_drop ON)
    elseif(_name MATCHES "sve" AND NOT LIBVPX_CONFIG_HAVE_SVE)
      set(_drop ON)
    endif()
    if(_drop)
      list(REMOVE_ITEM _result "${_source}")
    endif()
  endforeach()
  set(${output} "${_result}" PARENT_SCOPE)
endfunction()

if(LIBVPX_ENABLE_SIMD AND (_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64))
  set(_libvpx_x86_sources)
  foreach(_source IN LISTS LIBVPX_C_SOURCES)
    if(_source MATCHES "(^|/)(vp8|vp9|vpx_dsp)/.*x86/.*\\.c$")
      list(APPEND _libvpx_x86_sources "${LIBVPX_ROOT}/${_source}")
    endif()
  endforeach()
  # These C files are glue for symbols provided by x86 assembly objects.
  if(NOT _libvpx_use_x86_asm)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/(vp8_asm_stubs|loopfilter_x86)\\.c$")
  endif()
  if(NOT LIBVPX_BUILD_VP8)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/vp8/")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/vp9/")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/(vp8|vp9)/encoder/")
  endif()
  if(NOT _libvpx_build_vp8_encoder)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/vp8/encoder/")
  endif()
  if(NOT _libvpx_build_vp9_encoder)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/vp9/encoder/")
  endif()
  if(NOT LIBVPX_ENABLE_VP9_HIGHBITDEPTH)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/highbd|high_bd|highbd")
  else()
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX
      "/vpx_dsp/x86/fwd_txfm_avx2\.c$")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX
      "/vpx_dsp/x86/(avg_intrin|avg_ssse3|fwd_|highbd_(quantize|sad)|quantize_|sad4d_|sse_|subtract_|sum_squares)")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX
      "/vpx_dsp/x86/(avg_intrin|avg_ssse3|convolve|fwd_|highbd_|inv_|loopfilter_|quantize_|txfm|vpx_)")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER AND NOT LIBVPX_ENABLE_POSTPROC
    AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX
      "/vpx_dsp/x86/(avg_pred|highbd_avg_pred|sad|variance|ssim_opt)")
  endif()
  if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/vpx_dsp/x86/deblock_sse2\\.c$")
  endif()
  if(NOT LIBVPX_ENABLE_TEMPORAL_DENOISING)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/vp8/encoder/x86/denoising_sse2\\.c$")
  endif()
  if(NOT LIBVPX_ENABLE_VP9_TEMPORAL_DENOISING)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX "/vp9/encoder/x86/vp9_denoiser_sse2\\.c$")
  endif()
  if(LIBVPX_ENABLE_REALTIME_ONLY)
    list(FILTER _libvpx_x86_sources EXCLUDE REGEX
      "/vp9/encoder/x86/(temporal_filter|highbd_temporal_filter)_")
  endif()
  libvpx_filter_feature_sources(_libvpx_x86_sources ${_libvpx_x86_sources})
  list(APPEND _libvpx_sources ${_libvpx_x86_sources})
  if(_libvpx_target_arch STREQUAL x86 AND LIBVPX_CONFIG_HAVE_MMX)
    list(APPEND _libvpx_sources "${LIBVPX_ROOT}/vpx_ports/emms_mmx.c")
  endif()
endif()

if(LIBVPX_ENABLE_SIMD AND LIBVPX_CONFIG_HAVE_NEON
  AND (_libvpx_target_arch STREQUAL arm32 OR _libvpx_target_arch STREQUAL arm64))
  set(_libvpx_arm_sources)
  foreach(_source IN LISTS LIBVPX_C_SOURCES)
    if(_source MATCHES "(^|/)(vp8|vp9|vpx_dsp)/.*arm/.*\\.c$")
      list(APPEND _libvpx_arm_sources "${LIBVPX_ROOT}/${_source}")
    endif()
  endforeach()
  if(NOT LIBVPX_BUILD_VP8)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/vp8/")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/vp9/")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/(vp8|vp9)/encoder/")
  endif()
  if(NOT _libvpx_build_vp8_encoder)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/vp8/encoder/")
  endif()
  if(NOT _libvpx_build_vp9_encoder)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/vp9/encoder/")
  endif()
  if(NOT LIBVPX_ENABLE_VP9_HIGHBITDEPTH)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "(highbd|high_bd)")
  endif()
  if(NOT LIBVPX_ENABLE_VP9_TEMPORAL_DENOISING)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/vp9_denoiser")
  endif()
  if(LIBVPX_ENABLE_REALTIME_ONLY)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX
      "/vp9/encoder/.*temporal_filter")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX
      "/vpx_dsp/arm/(avg_neon|hadamard_neon|fdct|quantize_neon|sad4d|subtract_neon|sum_squares|highbd_sad|sse_neon|highbd_sse)")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX
      "/vpx_dsp/arm/(avg_neon|hadamard_neon|fdct|idct|loopfilter_|quantize_neon|vpx_convolve|vpx_scaled)")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER AND NOT LIBVPX_ENABLE_POSTPROC
    AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX
      "/vpx_dsp/arm/(avg_pred|highbd_avg_pred|sad|subpel_variance|variance|ssim)")
  endif()
  if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/vpx_dsp/arm/deblock_neon\\.c$")
  endif()
  libvpx_filter_feature_sources(_libvpx_arm_sources ${_libvpx_arm_sources})
  list(FILTER _libvpx_arm_sources EXCLUDE REGEX "/vpx_convolve8_neon_asm\\.c$")
  if(_libvpx_use_arm_asm AND LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_arm_sources EXCLUDE REGEX
      "/vpx_dsp/arm/(idct4x4_1_add_neon|idct4x4_add_neon|loopfilter_neon|vpx_convolve_copy_neon|vpx_convolve8_neon|vpx_convolve_avg_neon)\\.c$")
    list(APPEND _libvpx_arm_sources
      "${LIBVPX_ROOT}/vpx_dsp/arm/vpx_convolve8_neon_asm.c")
  endif()
  list(APPEND _libvpx_sources ${_libvpx_arm_sources})
endif()

set(_libvpx_arm_asm_sources)
if(_libvpx_use_arm_asm)
  foreach(_source IN LISTS LIBVPX_ASM_SOURCES)
    if(_source MATCHES "(^|/)(vp8|vp9|vpx_dsp)/.*arm/.*\\.asm$")
      list(APPEND _libvpx_arm_asm_sources "${LIBVPX_ROOT}/${_source}")
    endif()
  endforeach()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_arm_asm_sources EXCLUDE REGEX "/vpx_dsp/arm/(idct|loopfilter|save_reg|vpx_convolve)")
  endif()
  libvpx_filter_feature_sources(_libvpx_arm_asm_sources ${_libvpx_arm_asm_sources})
endif()

if(VPX_ARCH_MIPS)
  set(_libvpx_mips_sources)
  foreach(_source IN LISTS LIBVPX_C_SOURCES)
    if(_source MATCHES "(^|/)(vp8|vp9|vpx_dsp|vpx_scale)/.*mips/.*\\.c$")
      list(APPEND _libvpx_mips_sources "${LIBVPX_ROOT}/${_source}")
    endif()
  endforeach()
  if(NOT LIBVPX_BUILD_VP8)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/vp8/")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/vp9/")
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX
      "/vpx_dsp/mips/(avg_|convolve|fwd_|idct|itrans|loopfilter|vpx_convolve)")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX
      "/vpx_dsp/mips/(avg_|fwd_|subtract_|sum_squares_)")
  endif()
  if(NOT _libvpx_build_vp8_encoder)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/vp8/encoder/")
  endif()
  if(NOT _libvpx_build_vp9_encoder)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/vp9/encoder/")
  endif()
  if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/(add_noise|deblock|mfqe)")
  endif()
  if(NOT LIBVPX_CONFIG_HAVE_MSA)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/msa/|_msa\\.c$")
  endif()
  if(NOT LIBVPX_CONFIG_HAVE_MMI)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/mmi/|_mmi\\.c$")
  endif()
  if(NOT LIBVPX_CONFIG_HAVE_DSPR2)
    list(FILTER _libvpx_mips_sources EXCLUDE REGEX "/dspr2/|_dspr2\\.c$")
  endif()
  list(APPEND _libvpx_sources ${_libvpx_mips_sources})
endif()

if(VPX_ARCH_PPC AND LIBVPX_CONFIG_HAVE_VSX)
  set(_libvpx_ppc_sources)
  foreach(_source IN LISTS LIBVPX_C_SOURCES)
    if(_source MATCHES "(^|/)(vp8|vp9|vpx_dsp)/.*ppc/.*\\.c$")
      list(APPEND _libvpx_ppc_sources "${LIBVPX_ROOT}/${_source}")
    endif()
  endforeach()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_ppc_sources EXCLUDE REGEX "/vp9/")
    list(FILTER _libvpx_ppc_sources EXCLUDE REGEX
      "/vpx_dsp/ppc/(fdct|hadamard|inv_txfm|loopfilter|quantize|vpx_convolve)")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_ppc_sources EXCLUDE REGEX "/vp9/encoder/")
    list(FILTER _libvpx_ppc_sources EXCLUDE REGEX
      "/vpx_dsp/ppc/(fdct|hadamard|quantize|subtract)")
  endif()
  if(NOT _libvpx_build_vp9_encoder)
    list(FILTER _libvpx_ppc_sources EXCLUDE REGEX "/vp9/encoder/")
  endif()
  if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_ppc_sources EXCLUDE REGEX "/vpx_dsp/ppc/deblock_vsx\\.c$")
  endif()
  list(APPEND _libvpx_sources ${_libvpx_ppc_sources})
endif()
if(VPX_ARCH_LOONGARCH AND (LIBVPX_CONFIG_HAVE_LSX OR LIBVPX_CONFIG_HAVE_LASX))
  set(_libvpx_loongarch_sources)
  foreach(_source IN LISTS LIBVPX_C_SOURCES)
    if(_source MATCHES "(^|/)(vp8|vp9|vpx_dsp)/.*loongarch/.*\\.c$")
      list(APPEND _libvpx_loongarch_sources "${LIBVPX_ROOT}/${_source}")
    endif()
  endforeach()
  if(NOT LIBVPX_BUILD_VP8)
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX "/vp8/")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX "/vp9/")
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX
      "/vpx_dsp/loongarch/(avg_|fwd_|idct|loopfilter|quantize|vpx_convolve)")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX "/(vp8|vp9)/encoder/")
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX
      "/vpx_dsp/loongarch/(avg_|fwd_|quantize|subtract)")
  endif()
  if(NOT _libvpx_build_vp8_encoder)
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX "/vp8/encoder/")
  endif()
  if(NOT _libvpx_build_vp9_encoder)
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX "/vp9/encoder/")
  endif()
  if(NOT LIBVPX_CONFIG_HAVE_LSX)
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX "_lsx\\.c$")
  endif()
  if(NOT LIBVPX_CONFIG_HAVE_LASX)
    list(FILTER _libvpx_loongarch_sources EXCLUDE REGEX "_lasx\\.c$")
  endif()
  list(APPEND _libvpx_sources ${_libvpx_loongarch_sources})
endif()

if(LIBVPX_ENABLE_RUNTIME_CPU_DETECT)
  if(_libvpx_target_arch STREQUAL arm64)
    list(APPEND _libvpx_sources "${LIBVPX_ROOT}/vpx_ports/aarch64_cpudetect.c")
  elseif(_libvpx_target_arch STREQUAL arm32)
    list(APPEND _libvpx_sources "${LIBVPX_ROOT}/vpx_ports/aarch32_cpudetect.c")
  elseif(_libvpx_target_arch STREQUAL mips32 OR _libvpx_target_arch STREQUAL mips64)
    list(APPEND _libvpx_sources "${LIBVPX_ROOT}/vpx_ports/mips_cpudetect.c")
  elseif(_libvpx_target_arch STREQUAL ppc64le)
    list(APPEND _libvpx_sources "${LIBVPX_ROOT}/vpx_ports/ppc_cpudetect.c")
  elseif(_libvpx_target_arch MATCHES "^loongarch")
    list(APPEND _libvpx_sources "${LIBVPX_ROOT}/vpx_ports/loongarch_cpudetect.c")
  endif()
endif()

list(REMOVE_DUPLICATES _libvpx_sources)
list(SORT _libvpx_sources)

find_package(Perl COMPONENTS Interpreter REQUIRED)

# Generated configuration and RTCD headers
string(TOLOWER "${LIBVPX_ASM_MODE}" _libvpx_asm_mode_config)
set(LIBVPX_BUILD_CONFIG
  "--cmake --target=${LIBVPX_RTCD_ARCH} --asm-mode=${_libvpx_asm_mode_config}")
set(_libvpx_build_config_options
  LIBVPX_BUILD_STATIC:static
  LIBVPX_BUILD_SHARED:shared
  LIBVPX_CONFIG_VP8:vp8
  LIBVPX_CONFIG_VP9:vp9
  LIBVPX_CONFIG_VP8_ENCODER:vp8-encoder
  LIBVPX_CONFIG_VP8_DECODER:vp8-decoder
  LIBVPX_CONFIG_VP9_ENCODER:vp9-encoder
  LIBVPX_CONFIG_VP9_DECODER:vp9-decoder
  LIBVPX_CONFIG_MULTITHREAD:multithread
  LIBVPX_CONFIG_RUNTIME_CPU_DETECT:runtime-cpu-detect
  LIBVPX_CONFIG_POSTPROC:postproc
  LIBVPX_CONFIG_VP9_POSTPROC:vp9-postproc
  LIBVPX_CONFIG_VP9_HIGHBITDEPTH:vp9-highbitdepth
  LIBVPX_CONFIG_SPATIAL_RESAMPLING:spatial-resampling
  LIBVPX_CONFIG_INTERNAL_STATS:internal-stats
  LIBVPX_CONFIG_REALTIME_ONLY:realtime-only
  LIBVPX_CONFIG_ONTHEFLY_BITPACKING:onthefly-bitpacking
  LIBVPX_CONFIG_ERROR_CONCEALMENT:error-concealment
  LIBVPX_CONFIG_TEMPORAL_DENOISING:temporal-denoising
  LIBVPX_CONFIG_VP9_TEMPORAL_DENOISING:vp9-temporal-denoising
  LIBVPX_CONFIG_COEFFICIENT_RANGE_CHECKING:coefficient-range-checking
  LIBVPX_CONFIG_BETTER_HW_COMPATIBILITY:better-hw-compatibility
  LIBVPX_CONFIG_MULTI_RES_ENCODING:multi-res-encoding
  LIBVPX_CONFIG_NON_GREEDY_MV:non-greedy-mv
  LIBVPX_CONFIG_EXPERIMENTAL:experimental
  LIBVPX_CONFIG_BITSTREAM_DEBUG:bitstream-debug
  LIBVPX_CONFIG_MISMATCH_DEBUG:mismatch-debug
  LIBVPX_CONFIG_POSTPROC_VISUALIZER:postproc-visualizer
  LIBVPX_CONFIG_FP_MB_STATS:fp-mb-stats
  LIBVPX_CONFIG_EMULATE_HARDWARE:emulate-hardware
  LIBVPX_CONFIG_ALWAYS_ADJUST_BPM:always-adjust-bpm
  LIBVPX_CONFIG_COLLECT_COMPONENT_TIMING:collect-component-timing
  LIBVPX_CONFIG_SMALL:small
  LIBVPX_CONFIG_DEBUG:debug
  LIBVPX_CONFIG_PIC:pic)
foreach(_libvpx_build_config_option IN LISTS _libvpx_build_config_options)
  string(REPLACE ":" ";" _libvpx_build_config_pair
    "${_libvpx_build_config_option}")
  list(GET _libvpx_build_config_pair 0 _libvpx_build_config_variable)
  list(GET _libvpx_build_config_pair 1 _libvpx_build_config_name)
  if(${_libvpx_build_config_variable})
    string(APPEND LIBVPX_BUILD_CONFIG
      " --enable-${_libvpx_build_config_name}")
  else()
    string(APPEND LIBVPX_BUILD_CONFIG
      " --disable-${_libvpx_build_config_name}")
  endif()
endforeach()

configure_file("${LIBVPX_ROOT}/cmake/templates/vpx_config.h.in"
  "${LIBVPX_BINARY_DIR}/vpx_config.h" @ONLY)
configure_file("${LIBVPX_ROOT}/cmake/templates/vpx_config.c.in"
  "${LIBVPX_BINARY_DIR}/vpx_config.c" @ONLY)
configure_file("${LIBVPX_ROOT}/cmake/templates/vpx_config.asm.in"
  "${LIBVPX_BINARY_DIR}/vpx_config.asm" @ONLY)
configure_file("${LIBVPX_ROOT}/cmake/templates/vpx_config_gas.asm.in"
  "${LIBVPX_BINARY_DIR}/vpx_config_gas.asm" @ONLY)

set(_libvpx_config_mk_vars
  CONFIG_RUNTIME_CPU_DETECT CONFIG_SPATIAL_RESAMPLING
  CONFIG_VP8_DECODER CONFIG_VP9_DECODER CONFIG_VP8_ENCODER CONFIG_VP9_ENCODER
  CONFIG_VP8 CONFIG_VP9 CONFIG_ENCODERS CONFIG_DECODERS
  CONFIG_POSTPROC CONFIG_VP9_POSTPROC CONFIG_VP9_HIGHBITDEPTH
  CONFIG_MULTITHREAD CONFIG_INTERNAL_STATS CONFIG_REALTIME_ONLY
  CONFIG_ONTHEFLY_BITPACKING CONFIG_ERROR_CONCEALMENT CONFIG_TEMPORAL_DENOISING
  CONFIG_VP9_TEMPORAL_DENOISING CONFIG_COEFFICIENT_RANGE_CHECKING
  CONFIG_MULTI_RES_ENCODING CONFIG_NON_GREEDY_MV CONFIG_BIG_ENDIAN CONFIG_PIC
  HAVE_X86_ASM HAVE_MMX HAVE_SSE HAVE_SSE2 HAVE_SSE3 HAVE_SSSE3 HAVE_SSE4_1
  HAVE_AVX HAVE_AVX2 HAVE_AVX512 HAVE_NEON_ASM HAVE_NEON HAVE_NEON_DOTPROD
  HAVE_NEON_I8MM HAVE_SVE HAVE_SVE2 HAVE_MIPS32 HAVE_MIPS64 HAVE_DSPR2 HAVE_MSA
  HAVE_MMI HAVE_VSX HAVE_LSX HAVE_LASX)
set(_libvpx_config_mk "# Generated file.\n")
foreach(_libvpx_mk_var IN LISTS _libvpx_config_mk_vars)
  if(_libvpx_mk_var MATCHES "^CONFIG_")
    string(REGEX REPLACE "^CONFIG_" "LIBVPX_CONFIG_"
      _libvpx_cmake_var "${_libvpx_mk_var}")
  else()
    string(REGEX REPLACE "^HAVE_" "LIBVPX_CONFIG_HAVE_"
      _libvpx_cmake_var "${_libvpx_mk_var}")
  endif()
  if(${_libvpx_cmake_var})
    string(APPEND _libvpx_config_mk
      "${_libvpx_mk_var}=yes\n")
  else()
    string(APPEND _libvpx_config_mk
      "${_libvpx_mk_var}=no\n")
  endif()
endforeach()

file(CONFIGURE OUTPUT "${LIBVPX_BINARY_DIR}/libvpx_config.mk"
  CONTENT "@_libvpx_config_mk@" @ONLY)

function(libvpx_generate_rtcd output_name symbol defs_file)
  add_custom_command(
    OUTPUT "${LIBVPX_BINARY_DIR}/${output_name}"
    COMMAND "${CMAKE_COMMAND}"
      "-DPERL=${PERL_EXECUTABLE}"
      "-DRTCD=${LIBVPX_ROOT}/build/make/rtcd.pl"
      "-DARCH=${LIBVPX_RTCD_ARCH}"
      "-DSYMBOL=${symbol}"
      "-DCONFIG=${LIBVPX_BINARY_DIR}/libvpx_config.mk"
      "-DDEFS=${LIBVPX_ROOT}/${defs_file}"
      "-DOUTPUT=${LIBVPX_BINARY_DIR}/${output_name}"
      -P "${LIBVPX_ROOT}/cmake/LibvpxRtcd.cmake"
    DEPENDS
      "${LIBVPX_ROOT}/build/make/rtcd.pl"
      "${LIBVPX_ROOT}/${defs_file}"
      "${LIBVPX_BINARY_DIR}/libvpx_config.mk"
      "${LIBVPX_ROOT}/cmake/LibvpxRtcd.cmake"
    VERBATIM)
endfunction()
libvpx_generate_rtcd(vpx_scale_rtcd.h vpx_scale_rtcd vpx_scale/vpx_scale_rtcd.pl)
libvpx_generate_rtcd(vpx_dsp_rtcd.h vpx_dsp_rtcd vpx_dsp/vpx_dsp_rtcd_defs.pl)
libvpx_generate_rtcd(vp8_rtcd.h vp8_rtcd vp8/common/rtcd_defs.pl)
libvpx_generate_rtcd(vp9_rtcd.h vp9_rtcd vp9/common/vp9_rtcd_defs.pl)
set(_libvpx_rtcd_generated_sources
  "${LIBVPX_BINARY_DIR}/vpx_scale_rtcd.h"
  "${LIBVPX_BINARY_DIR}/vpx_dsp_rtcd.h"
  "${LIBVPX_BINARY_DIR}/vp8_rtcd.h"
  "${LIBVPX_BINARY_DIR}/vp9_rtcd.h")

# This reduced vendor tree intentionally omits CHANGELOG and has no independent
# Git metadata, so the upstream release must be recorded explicitly by project().
set(LIBVPX_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(LIBVPX_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(LIBVPX_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(LIBVPX_VERSION_EXTRA "")
set(LIBVPX_VERSION_STRING "v${PROJECT_VERSION}")
configure_file("${LIBVPX_ROOT}/cmake/templates/vpx_version.h.in"
  "${LIBVPX_BINARY_DIR}/vpx_version.h" @ONLY)

# Shared library version
set(_libvpx_so_version_major 0)
set(_libvpx_so_version_minor 0)
set(_libvpx_so_version_patch 0)
file(STRINGS "${LIBVPX_ROOT}/libs.mk" _libvpx_so_version_lines
  REGEX "^SO_VERSION_(MAJOR|MINOR|PATCH)")
foreach(_libvpx_so_version_line IN LISTS _libvpx_so_version_lines)
  if(_libvpx_so_version_line MATCHES
    "^SO_VERSION_MAJOR[ \\t]*:?=[ \\t]*([0-9]+)")
    set(_libvpx_so_version_major "${CMAKE_MATCH_1}")
  elseif(_libvpx_so_version_line MATCHES
    "^SO_VERSION_MINOR[ \\t]*:?=[ \\t]*([0-9]+)")
    set(_libvpx_so_version_minor "${CMAKE_MATCH_1}")
  elseif(_libvpx_so_version_line MATCHES
    "^SO_VERSION_PATCH[ \\t]*:?=[ \\t]*([0-9]+)")
    set(_libvpx_so_version_patch "${CMAKE_MATCH_1}")
  endif()
endforeach()

list(APPEND _libvpx_sources "${LIBVPX_BINARY_DIR}/vpx_config.c")

# Library targets and public symbol exports
if(NOT LIBVPX_BUILD_STATIC AND NOT LIBVPX_BUILD_SHARED)
  message(FATAL_ERROR "At least one of LIBVPX_BUILD_STATIC/LIBVPX_BUILD_SHARED must be enabled.")
endif()

if(LIBVPX_BUILD_SHARED)
  set(_libvpx_export_files "${LIBVPX_ROOT}/vpx/exports_com")
  if(LIBVPX_BUILD_ENCODER)
    list(APPEND _libvpx_export_files "${LIBVPX_ROOT}/vpx/exports_enc")
  endif()
  if(LIBVPX_BUILD_DECODER)
    list(APPEND _libvpx_export_files "${LIBVPX_ROOT}/vpx/exports_dec")
  endif()
  if(_libvpx_build_vp8_encoder)
    list(APPEND _libvpx_export_files "${LIBVPX_ROOT}/vp8/exports_enc")
  endif()
  if(_libvpx_build_vp8_decoder)
    list(APPEND _libvpx_export_files "${LIBVPX_ROOT}/vp8/exports_dec")
  endif()
  if(_libvpx_build_vp9_encoder)
    list(APPEND _libvpx_export_files "${LIBVPX_ROOT}/vp9/exports_enc")
  endif()
  if(_libvpx_build_vp9_decoder)
    list(APPEND _libvpx_export_files "${LIBVPX_ROOT}/vp9/exports_dec")
  endif()
  set(_libvpx_export_symbols)
  foreach(_libvpx_export_file IN LISTS _libvpx_export_files)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
      "${_libvpx_export_file}")
    file(STRINGS "${_libvpx_export_file}" _libvpx_export_lines)
    foreach(_libvpx_export_line IN LISTS _libvpx_export_lines)
      if(_libvpx_export_line MATCHES "^[ ]*text[ ]+([^ ]+)")
        list(APPEND _libvpx_export_symbols "${CMAKE_MATCH_1}")
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES _libvpx_export_symbols)
  if(WIN32)
    set(_libvpx_export_file "${LIBVPX_BINARY_DIR}/libvpx.def")
    set(_libvpx_export_content "LIBRARY vpx\nEXPORTS\n")
    foreach(_libvpx_export_symbol IN LISTS _libvpx_export_symbols)
      string(APPEND _libvpx_export_content
        "${_libvpx_export_symbol}\n")
    endforeach()
  elseif(APPLE)
    set(_libvpx_export_file "${LIBVPX_BINARY_DIR}/libvpx.syms")
    set(_libvpx_export_content "")
    foreach(_libvpx_export_symbol IN LISTS _libvpx_export_symbols)
      string(APPEND _libvpx_export_content "_${_libvpx_export_symbol}\n")
    endforeach()
  else()
    set(_libvpx_export_file "${LIBVPX_BINARY_DIR}/libvpx.ver")
    set(_libvpx_export_content "{ global:\n")
    foreach(_libvpx_export_symbol IN LISTS _libvpx_export_symbols)
      string(APPEND _libvpx_export_content "${_libvpx_export_symbol};\n")
    endforeach()
    string(APPEND _libvpx_export_content "local: *; };\n")
  endif()
  file(CONFIGURE OUTPUT "${_libvpx_export_file}"
    CONTENT "@_libvpx_export_content@" @ONLY)
endif()

set(_libvpx_arm_asm_generated_sources)
foreach(_libvpx_arm_asm_source IN LISTS _libvpx_arm_asm_sources)
  file(RELATIVE_PATH _libvpx_arm_asm_relative
    "${LIBVPX_ROOT}" "${_libvpx_arm_asm_source}")
  set(_libvpx_arm_asm_output
    "${LIBVPX_BINARY_DIR}/arm_asm/${_libvpx_arm_asm_relative}.S")
  get_filename_component(_libvpx_arm_asm_output_dir
    "${_libvpx_arm_asm_output}" DIRECTORY)
  if(APPLE)
    set(_libvpx_ads2gas "${LIBVPX_ROOT}/build/make/ads2gas_apple.pl")
  else()
    set(_libvpx_ads2gas "${LIBVPX_ROOT}/build/make/ads2gas.pl")
  endif()
  add_custom_command(
    OUTPUT "${_libvpx_arm_asm_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${_libvpx_arm_asm_output_dir}"
    COMMAND "${CMAKE_COMMAND}"
      "-DPERL=${PERL_EXECUTABLE}"
      "-DADS2GAS=${_libvpx_ads2gas}"
      "-DINPUT=${_libvpx_arm_asm_source}"
      "-DOUTPUT=${_libvpx_arm_asm_output}"
      -P "${LIBVPX_ROOT}/cmake/LibvpxAds2Gas.cmake"
    DEPENDS
      "${_libvpx_arm_asm_source}"
      "${_libvpx_ads2gas}"
      "${LIBVPX_ROOT}/cmake/LibvpxAds2Gas.cmake"
    VERBATIM)
  list(APPEND _libvpx_arm_asm_generated_sources "${_libvpx_arm_asm_output}")
endforeach()
add_library(libvpx_objects OBJECT ${_libvpx_sources} ${_libvpx_rtcd_generated_sources}
  ${_libvpx_arm_asm_generated_sources})
set_target_properties(libvpx_objects PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
  POSITION_INDEPENDENT_CODE "${LIBVPX_CONFIG_PIC}")

if(LIBVPX_BUILD_STATIC)
  add_library(libvpx STATIC $<TARGET_OBJECTS:libvpx_objects>)
  set(_libvpx_link_targets libvpx)
endif()
if(LIBVPX_BUILD_SHARED)
  if(TARGET libvpx)
    add_library(libvpx_shared SHARED $<TARGET_OBJECTS:libvpx_objects>)
    set(_libvpx_shared_target libvpx_shared)
  else()
    add_library(libvpx SHARED $<TARGET_OBJECTS:libvpx_objects>)
    set(_libvpx_link_targets libvpx)
    set(_libvpx_shared_target libvpx)
  endif()
  if(NOT _libvpx_shared_target STREQUAL libvpx)
    list(APPEND _libvpx_link_targets ${_libvpx_shared_target})
  endif()
endif()
add_library(libvpx::vpx ALIAS libvpx)
if(TARGET libvpx_shared)
  add_library(libvpx::vpx_shared ALIAS libvpx_shared)
endif()

set_target_properties(libvpx_objects PROPERTIES OUTPUT_NAME vpx)
foreach(_libvpx_target IN LISTS _libvpx_link_targets)
  if(WIN32 AND _libvpx_target STREQUAL libvpx AND TARGET libvpx_shared)
    set(_libvpx_output_name vpx_static)
  else()
    set(_libvpx_output_name vpx)
  endif()
  set_target_properties(${_libvpx_target} PROPERTIES
    OUTPUT_NAME "${_libvpx_output_name}"
    EXPORT_NAME vpx
    VERSION "${_libvpx_so_version_major}.${_libvpx_so_version_minor}.${_libvpx_so_version_patch}"
    SOVERSION "${_libvpx_so_version_major}")
  target_include_directories(${_libvpx_target}
    PUBLIC
      $<BUILD_INTERFACE:${LIBVPX_ROOT}>
      $<BUILD_INTERFACE:${LIBVPX_BINARY_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endforeach()
if(TARGET libvpx_shared)
  set_target_properties(libvpx_shared PROPERTIES EXPORT_NAME vpx_shared)
endif()
target_include_directories(libvpx_objects PRIVATE
  "${LIBVPX_ROOT}" "${LIBVPX_BINARY_DIR}")
target_compile_definitions(libvpx_objects PRIVATE VPX_BUILD_CMAKE=1)

if(_libvpx_arm_asm_generated_sources)
  set_source_files_properties(${_libvpx_arm_asm_generated_sources}
    PROPERTIES LANGUAGE ASM)
  target_compile_options(libvpx_objects PRIVATE
    $<$<COMPILE_LANGUAGE:ASM>:-I${LIBVPX_BINARY_DIR}/>
    $<$<COMPILE_LANGUAGE:ASM>:-I${LIBVPX_ROOT}/>)
endif()

if(LIBVPX_CONFIG_HAVE_X86_ASM)
  set(_libvpx_x86_asm_sources)
  foreach(_source IN LISTS LIBVPX_ASM_SOURCES)
    if(_source MATCHES "(^|/)(vp8|vp9|vpx_dsp)/.*x86/.*\\.asm$")
      list(APPEND _libvpx_x86_asm_sources "${LIBVPX_ROOT}/${_source}")
    endif()
  endforeach()
  if(NOT LIBVPX_BUILD_VP8)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX "/vp8/")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX "/vp9/")
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX
      "/vpx_dsp/x86/(avg_ssse3|fwd_|highbd_|inv_|quantize_|vpx_)")
  endif()
  if(_libvpx_target_arch STREQUAL x86)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX "_x86_64\\.asm$")
  endif()
  list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX
    "/(bitdepth_conversion_sse2|x86_abi_support|float_control_word)\\.asm$")
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX "/(vp8|vp9)/encoder/")
  endif()
  if(NOT _libvpx_build_vp8_encoder)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX "/vp8/encoder/")
  endif()
  if(NOT _libvpx_build_vp9_encoder)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX "/vp9/encoder/")
  endif()
  if(NOT LIBVPX_ENABLE_TEMPORAL_DENOISING)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX
      "/vp8/encoder/x86/temporal_filter_apply_sse2\\.asm$")
  endif()
  if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX "(mfqe|add_noise)")
  endif()
  if(NOT LIBVPX_ENABLE_VP9_POSTPROC)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX
      "/vp9/common/x86/vp9_mfqe_sse2\\.asm$")
  endif()
  if(NOT LIBVPX_ENABLE_VP9_HIGHBITDEPTH)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX
      "(highbd|high_bd|vpx_high_)")
  endif()
  if(NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX
      "/vpx_dsp/x86/(avg_ssse3|fwd_|highbd_(quantize|sad)|quantize_|sad4d_|sse_|subtract_|sum_squares)")
  endif()
  if(NOT LIBVPX_ENABLE_POSTPROC AND NOT LIBVPX_ENABLE_VP9_POSTPROC
    AND NOT LIBVPX_BUILD_ENCODER)
    list(FILTER _libvpx_x86_asm_sources EXCLUDE REGEX
      "/vpx_dsp/x86/(highbd_(subpel_)?variance|ssim_opt|subpel_variance)")
  endif()
  libvpx_filter_feature_sources(_libvpx_x86_asm_sources ${_libvpx_x86_asm_sources})
  set_source_files_properties(${_libvpx_x86_asm_sources}
    PROPERTIES LANGUAGE ASM_NASM)
  target_sources(libvpx_objects PRIVATE ${_libvpx_x86_asm_sources})
  if(_libvpx_target_arch STREQUAL x64)
    if(LIBVPX_CONFIG_HAVE_MMX)
      set_source_files_properties("${LIBVPX_ROOT}/vpx_ports/emms_mmx.asm"
        PROPERTIES LANGUAGE ASM_NASM)
      target_sources(libvpx_objects PRIVATE
        "${LIBVPX_ROOT}/vpx_ports/emms_mmx.asm")
    endif()
    if(MSVC)
      set_source_files_properties("${LIBVPX_ROOT}/vpx_ports/float_control_word.asm"
        PROPERTIES LANGUAGE ASM_NASM)
      target_sources(libvpx_objects PRIVATE
        "${LIBVPX_ROOT}/vpx_ports/float_control_word.asm")
    endif()
  endif()
  target_compile_options(libvpx_objects PRIVATE
    $<$<COMPILE_LANGUAGE:ASM_NASM>:-I${LIBVPX_BINARY_DIR}/>
    $<$<COMPILE_LANGUAGE:ASM_NASM>:-I${LIBVPX_ROOT}/>)
endif()

if(LIBVPX_BUILD_ENCODER)
  set(_libvpx_rc_sources)
  foreach(_source IN LISTS _libvpx_sources)
    if(_source MATCHES "/(vp8|vp9)/"
      AND NOT _source MATCHES "/decoder/"
      AND NOT _source MATCHES "/vp[89]_dx_iface\\.c$")
      list(APPEND _libvpx_rc_sources "${_source}")
    endif()
  endforeach()
  foreach(_source IN LISTS _libvpx_x86_asm_sources)
    if(_source MATCHES "/(vp8|vp9)/"
      AND NOT _source MATCHES "/decoder/")
      list(APPEND _libvpx_rc_sources "${_source}")
    endif()
  endforeach()
  foreach(_source IN LISTS LIBVPX_CXX_SOURCES)
    list(APPEND _libvpx_rc_sources "${LIBVPX_ROOT}/${_source}")
  endforeach()
  if(NOT _libvpx_build_vp8_encoder)
    list(FILTER _libvpx_rc_sources EXCLUDE REGEX "/vp8/")
  endif()
  if(NOT _libvpx_build_vp9_encoder)
    list(FILTER _libvpx_rc_sources EXCLUDE REGEX "/vp9/")
  endif()
  if(NOT LIBVPX_BUILD_VP8)
    list(FILTER _libvpx_rc_sources EXCLUDE REGEX "/vp8/")
  endif()
  if(NOT LIBVPX_BUILD_VP9)
    list(FILTER _libvpx_rc_sources EXCLUDE REGEX "/vp9/")
  endif()
  list(REMOVE_DUPLICATES _libvpx_rc_sources)
  add_library(libvpxrc STATIC ${_libvpx_rc_sources} ${_libvpx_rtcd_generated_sources})
  set_target_properties(libvpxrc PROPERTIES
    OUTPUT_NAME vpxrc
    EXPORT_NAME vpxrc
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE "${LIBVPX_CONFIG_PIC}")
  target_include_directories(libvpxrc PRIVATE
    "${LIBVPX_ROOT}" "${LIBVPX_BINARY_DIR}")
  target_compile_definitions(libvpxrc PRIVATE VPX_BUILD_CMAKE=1)
  if(WIN32)
    target_compile_definitions(libvpxrc PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  if(LIBVPX_CONFIG_HAVE_X86_ASM)
    target_compile_options(libvpxrc PRIVATE
      $<$<COMPILE_LANGUAGE:ASM_NASM>:-I${LIBVPX_BINARY_DIR}/>
      $<$<COMPILE_LANGUAGE:ASM_NASM>:-I${LIBVPX_ROOT}/>)
  endif()
  if(MSVC AND LIBVPX_ENABLE_STATIC_MSVCRT)
    set_property(TARGET libvpxrc PROPERTY MSVC_RUNTIME_LIBRARY
      "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

if(_libvpx_target_arch STREQUAL arm32 AND LIBVPX_CONFIG_HAVE_NEON)
  target_compile_options(libvpx_objects PRIVATE ${_libvpx_arm_neon_cflags})
  if(TARGET libvpxrc)
    target_compile_options(libvpxrc PRIVATE ${_libvpx_arm_neon_cflags})
  endif()
endif()
if(_libvpx_target_arch STREQUAL arm64 AND LIBVPX_CONFIG_HAVE_NEON)
  target_compile_options(libvpx_objects PRIVATE ${_libvpx_arm64_cflags})
  if(TARGET libvpxrc)
    target_compile_options(libvpxrc PRIVATE ${_libvpx_arm64_cflags})
  endif()
endif()

if(_libvpx_target_arch STREQUAL x86 OR _libvpx_target_arch STREQUAL x64)
  foreach(_source IN LISTS _libvpx_sources)
    get_filename_component(_source_name "${_source}" NAME)
    if(MSVC)
      if(_source_name MATCHES "avx512")
        set(_libvpx_isa_flag "/arch:AVX512")
      elseif(_source_name MATCHES "avx2")
        set(_libvpx_isa_flag "/arch:AVX2")
      elseif(_source_name MATCHES "avx")
        set(_libvpx_isa_flag "/arch:AVX")
      else()
        set(_libvpx_isa_flag "")
      endif()
    else()
      if(_source_name MATCHES "avx512")
        set(_libvpx_isa_flag
          "-mavx512f;-mavx512cd;-mavx512bw;-mavx512dq;-mavx512vl")
      elseif(_source_name MATCHES "avx2")
        set(_libvpx_isa_flag "-mavx2")
      elseif(_source_name MATCHES "avx")
        set(_libvpx_isa_flag "-mavx")
      elseif(_source_name MATCHES "sse4")
        set(_libvpx_isa_flag "-msse4.1")
      elseif(_source_name MATCHES "ssse3")
        set(_libvpx_isa_flag "-mssse3")
      elseif(_source_name MATCHES "sse3")
        set(_libvpx_isa_flag "-msse3")
      elseif(_source_name MATCHES "sse2")
        set(_libvpx_isa_flag "-msse2")
      elseif(_source_name MATCHES "mmx")
        set(_libvpx_isa_flag "-mmmx")
      else()
        set(_libvpx_isa_flag "")
      endif()
    endif()
    if(_libvpx_isa_flag)
      set_source_files_properties("${_source}" PROPERTIES COMPILE_OPTIONS
        "${_libvpx_isa_flag}")
    endif()
  endforeach()
endif()

if(_libvpx_target_arch STREQUAL arm32 OR _libvpx_target_arch STREQUAL arm64)
  foreach(_source IN LISTS _libvpx_sources)
    if(_source MATCHES "/arm/" AND _source MATCHES "(dotprod|i8mm|sve2|sve)")
      if(_source MATCHES "sve2")
        set(_libvpx_isa_flags "-march=armv9-a+sve2")
      elseif(_source MATCHES "sve")
        set(_libvpx_isa_flags "-march=armv8.2-a+dotprod+i8mm+sve")
      elseif(_source MATCHES "i8mm")
        set(_libvpx_isa_flags "-march=armv8.2-a+dotprod+i8mm")
      else()
        set(_libvpx_isa_flags "-march=armv8.2-a+dotprod")
      endif()
      set_source_files_properties("${_source}" PROPERTIES
        COMPILE_OPTIONS "${_libvpx_isa_flags}")
    endif()
  endforeach()
endif()
if(_libvpx_target_arch STREQUAL mips32 OR _libvpx_target_arch STREQUAL mips64)
  foreach(_source IN LISTS _libvpx_sources)
    if(_source MATCHES "/mips/" OR _source MATCHES "_dspr2\\.c$")
      if(_source MATCHES "/(msa|.*_msa)\\.c$" OR _source MATCHES "/msa/")
        set(_libvpx_isa_flags "${_libvpx_msa_cflags}")
      elseif(_source MATCHES "/(mmi|.*_mmi)\\.c$" OR _source MATCHES "/mmi/")
        set(_libvpx_isa_flags "${_libvpx_mmi_cflags}")
      elseif(_source MATCHES "/dspr2/|_dspr2\\.c$")
        set(_libvpx_isa_flags "${_libvpx_dspr2_cflags}")
      else()
        set(_libvpx_isa_flags "")
      endif()
      if(_libvpx_isa_flags)
        set_source_files_properties("${_source}" PROPERTIES
          COMPILE_OPTIONS "${_libvpx_isa_flags}")
      endif()
    endif()
  endforeach()
endif()
if(_libvpx_target_arch STREQUAL ppc64le AND LIBVPX_CONFIG_HAVE_VSX)
  foreach(_source IN LISTS _libvpx_sources)
    if(_source MATCHES "/ppc/")
      set_source_files_properties("${_source}" PROPERTIES
        COMPILE_OPTIONS "${_libvpx_vsx_cflags}")
    endif()
  endforeach()
endif()
if(_libvpx_target_arch MATCHES "^loongarch")
  foreach(_source IN LISTS _libvpx_sources)
    if(_source MATCHES "_lasx\\.c$")
      set_source_files_properties("${_source}" PROPERTIES
        COMPILE_OPTIONS "${_libvpx_lasx_cflags}")
    elseif(_source MATCHES "_lsx\\.c$")
      set_source_files_properties("${_source}" PROPERTIES
        COMPILE_OPTIONS "${_libvpx_lsx_cflags}")
    endif()
  endforeach()
endif()

# Platform link dependencies
set(_libvpx_package_needs_threads 0)
if(NOT WIN32 AND LIBVPX_HAVE_PTHREAD_H AND LIBVPX_ENABLE_MULTITHREAD)
  if(LIBVPX_BUILD_STATIC)
    set(_libvpx_package_needs_threads 1)
  endif()
  foreach(_libvpx_target IN LISTS _libvpx_link_targets)
    target_link_libraries(${_libvpx_target} PRIVATE Threads::Threads)
  endforeach()
  if(TARGET libvpxrc)
    target_link_libraries(libvpxrc PRIVATE Threads::Threads)
  endif()
endif()
if(UNIX)
  check_library_exists(m floor "" LIBVPX_HAVE_LIBM)
  if(LIBVPX_HAVE_LIBM)
    foreach(_libvpx_target IN LISTS _libvpx_link_targets)
      target_link_libraries(${_libvpx_target} PRIVATE m)
    endforeach()
    if(TARGET libvpxrc)
      target_link_libraries(libvpxrc PRIVATE m)
    endif()
  endif()
endif()
if(WIN32)
  target_compile_definitions(libvpx_objects PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Optimisation, debug information and NDEBUG are owned by the selected CMake
# configuration. LIBVPX_ENABLE_DEBUG controls only libvpx's CONFIG_DEBUG code.
if(LIBVPX_ENABLE_PROFILE AND NOT MSVC)
  target_compile_options(libvpx_objects PRIVATE -fprofile-generate)
  if(TARGET libvpxrc)
    target_compile_options(libvpxrc PRIVATE -fprofile-generate)
  endif()
  foreach(_libvpx_target IN LISTS _libvpx_link_targets)
    target_link_options(${_libvpx_target} PRIVATE -fprofile-generate)
  endforeach()
endif()
if(LIBVPX_ENABLE_GPROF AND NOT MSVC)
  target_compile_options(libvpx_objects PRIVATE -pg)
  if(TARGET libvpxrc)
    target_compile_options(libvpxrc PRIVATE -pg)
  endif()
  foreach(_libvpx_target IN LISTS _libvpx_link_targets)
    target_link_options(${_libvpx_target} PRIVATE -pg)
  endforeach()
endif()
if(LIBVPX_ENABLE_GCOV AND NOT MSVC)
  target_compile_options(libvpx_objects PRIVATE -fprofile-arcs -ftest-coverage)
  if(TARGET libvpxrc)
    target_compile_options(libvpxrc PRIVATE -fprofile-arcs -ftest-coverage)
  endif()
  foreach(_libvpx_target IN LISTS _libvpx_link_targets)
    target_link_options(${_libvpx_target} PRIVATE -fprofile-arcs -ftest-coverage)
  endforeach()
endif()
if(MSVC AND LIBVPX_ENABLE_STATIC_MSVCRT)
  set_property(TARGET libvpx_objects ${_libvpx_link_targets} PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(LIBVPX_BUILD_SHARED)
  if(WIN32)
    if(MSVC)
      target_link_options(${_libvpx_shared_target} PRIVATE
        "/DEF:${_libvpx_export_file}")
    else()
      target_link_options(${_libvpx_shared_target} PRIVATE
        "${_libvpx_export_file}")
    endif()
  elseif(APPLE)
    target_link_options(${_libvpx_shared_target} PRIVATE
      "-Wl,-exported_symbols_list,${_libvpx_export_file}")
  else()
    target_link_options(${_libvpx_shared_target} PRIVATE
      "-Wl,--version-script=${_libvpx_export_file}")
  endif()
  set_property(TARGET ${_libvpx_shared_target} APPEND PROPERTY LINK_DEPENDS
    "${_libvpx_export_file}")
endif()

# Installation and package metadata
if(LIBVPX_INSTALL)
  set(_libvpx_public_headers
    "${LIBVPX_ROOT}/vpx/vpx_codec.h"
    "${LIBVPX_ROOT}/vpx/vpx_frame_buffer.h"
    "${LIBVPX_ROOT}/vpx/vpx_image.h"
    "${LIBVPX_ROOT}/vpx/vpx_integer.h")
  if(LIBVPX_BUILD_ENCODER)
    list(APPEND _libvpx_public_headers
      "${LIBVPX_ROOT}/vpx/vpx_encoder.h"
      "${LIBVPX_ROOT}/vpx/vpx_tpl.h"
      "${LIBVPX_ROOT}/vpx/vp8.h"
      "${LIBVPX_ROOT}/vpx/vp8cx.h"
      "${LIBVPX_ROOT}/vpx/vpx_ext_ratectrl.h")
  endif()
  if(LIBVPX_BUILD_DECODER)
    list(APPEND _libvpx_public_headers
      "${LIBVPX_ROOT}/vpx/vpx_decoder.h"
      "${LIBVPX_ROOT}/vpx/vp8.h"
      "${LIBVPX_ROOT}/vpx/vp8dx.h")
  endif()
  list(REMOVE_DUPLICATES _libvpx_public_headers)

  install(TARGETS ${_libvpx_link_targets} EXPORT LibvpxTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(FILES ${_libvpx_public_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/vpx")
  install(FILES "${LIBVPX_BINARY_DIR}/vpx_version.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/vpx")
  set(LIBVPX_PC_LIBS_PRIVATE "")
  if(LIBVPX_HAVE_LIBM)
    string(APPEND LIBVPX_PC_LIBS_PRIVATE " -lm")
  endif()
  if(_libvpx_package_needs_threads AND CMAKE_THREAD_LIBS_INIT)
    string(APPEND LIBVPX_PC_LIBS_PRIVATE " ${CMAKE_THREAD_LIBS_INIT}")
  endif()
  string(STRIP "${LIBVPX_PC_LIBS_PRIVATE}" LIBVPX_PC_LIBS_PRIVATE)
  configure_file("${LIBVPX_ROOT}/cmake/templates/vpx.pc.in"
    "${LIBVPX_BINARY_DIR}/vpx.pc" @ONLY)
  install(FILES "${LIBVPX_BINARY_DIR}/vpx.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
  configure_package_config_file(
    "${LIBVPX_ROOT}/cmake/LibvpxConfig.cmake.in"
    "${LIBVPX_BINARY_DIR}/LibvpxConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Libvpx")
  write_basic_package_version_file(
    "${LIBVPX_BINARY_DIR}/LibvpxConfigVersion.cmake"
    VERSION "${LIBVPX_VERSION_MAJOR}.${LIBVPX_VERSION_MINOR}.${LIBVPX_VERSION_PATCH}"
    COMPATIBILITY SameMajorVersion)
  install(EXPORT LibvpxTargets NAMESPACE libvpx::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Libvpx")
  install(FILES
    "${LIBVPX_BINARY_DIR}/LibvpxConfig.cmake"
    "${LIBVPX_BINARY_DIR}/LibvpxConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Libvpx")
endif()
