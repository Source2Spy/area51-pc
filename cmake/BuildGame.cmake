#==========================================================================
#  Prepare host shaders, then configure and build the game.
#==========================================================================

cmake_minimum_required( VERSION 3.23 )

if( NOT A51_PRESET )
    message( FATAL_ERROR "Specify the game configure preset with -DA51_PRESET=<preset>." )
endif()
if( NOT A51_CONFIG )
    set( A51_CONFIG Debug )
endif()
if( NOT A51_JOBS )
    set( A51_JOBS 4 )
endif()
get_filename_component( _source_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE )

execute_process(
    COMMAND "${CMAKE_COMMAND}" --preset shader-tools
    WORKING_DIRECTORY "${_source_dir}"
    COMMAND_ERROR_IS_FATAL ANY )
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build --preset prepare-shaders --parallel "${A51_JOBS}"
    WORKING_DIRECTORY "${_source_dir}"
    COMMAND_ERROR_IS_FATAL ANY )
execute_process(
    COMMAND "${CMAKE_COMMAND}" --preset "${A51_PRESET}"
        "-DA51_SHADER_KIT=${_source_dir}/build/shaders/GameApp/release"
    WORKING_DIRECTORY "${_source_dir}"
    COMMAND_ERROR_IS_FATAL ANY )
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_source_dir}/build/${A51_PRESET}"
        --config "${A51_CONFIG}" --target A51 --parallel "${A51_JOBS}"
    WORKING_DIRECTORY "${_source_dir}"
    COMMAND_ERROR_IS_FATAL ANY )
