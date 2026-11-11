# Copyright (c) 2026 The WebM project authors. All Rights Reserved.
# Use of this source code is governed by the BSD-style license in LICENSE.

# Run upstream's ADS-to-GAS converter without relying on shell redirection.

if(NOT DEFINED PERL OR NOT DEFINED ADS2GAS OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "PERL, ADS2GAS, INPUT and OUTPUT are required")
endif()

execute_process(
  COMMAND "${PERL}" "${ADS2GAS}"
  INPUT_FILE "${INPUT}"
  OUTPUT_VARIABLE _libvpx_asm
  RESULT_VARIABLE _libvpx_result
  ERROR_VARIABLE _libvpx_error)
if(_libvpx_result)
  file(REMOVE "${OUTPUT}")
  message(FATAL_ERROR "ads2gas failed: ${_libvpx_error}")
endif()

string(REPLACE ".include \"./vpx_config.asm\""
  ".include \"vpx_config_gas.asm\"" _libvpx_asm "${_libvpx_asm}")
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "@_libvpx_asm@" @ONLY)
