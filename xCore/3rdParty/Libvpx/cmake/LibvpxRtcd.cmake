# Copyright (c) 2026 The WebM project authors. All Rights Reserved.
# Use of this source code is governed by the BSD-style license in LICENSE.

# Generate one runtime-dispatch header from upstream's RTCD definitions.

if(NOT DEFINED PERL OR NOT DEFINED RTCD OR NOT DEFINED ARCH
  OR NOT DEFINED SYMBOL OR NOT DEFINED CONFIG OR NOT DEFINED DEFS
  OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "PERL, RTCD, ARCH, SYMBOL, CONFIG, DEFS and OUTPUT are required")
endif()

file(STRINGS "${CONFIG}" _libvpx_config_lines)
set(_libvpx_rtcd_options)
foreach(_libvpx_feature IN ITEMS
  MMX SSE SSE2 SSE3 SSSE3 SSE4_1 AVX AVX2 AVX512
  NEON_ASM NEON NEON_DOTPROD NEON_I8MM SVE SVE2
  DSPR2 MSA MMI VSX LSX LASX)
  list(FIND _libvpx_config_lines "HAVE_${_libvpx_feature}=yes"
    _libvpx_feature_enabled)
  if(_libvpx_feature_enabled EQUAL -1)
    string(TOLOWER "${_libvpx_feature}" _libvpx_rtcd_feature)
    list(APPEND _libvpx_rtcd_options "--disable-${_libvpx_rtcd_feature}")
  endif()
endforeach()

execute_process(
  COMMAND "${PERL}" "${RTCD}"
    "--arch=${ARCH}" "--sym=${SYMBOL}" "--config=${CONFIG}"
    ${_libvpx_rtcd_options} "${DEFS}"
  OUTPUT_VARIABLE _libvpx_rtcd
  RESULT_VARIABLE _libvpx_result
  ERROR_VARIABLE _libvpx_error)
if(_libvpx_result)
  file(REMOVE "${OUTPUT}")
  message(FATAL_ERROR "rtcd.pl failed: ${_libvpx_error}")
endif()
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "@_libvpx_rtcd@" @ONLY)
