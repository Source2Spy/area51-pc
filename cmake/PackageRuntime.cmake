# Package the executable and its matching shader packages in one directory.
cmake_minimum_required( VERSION 3.23 )

foreach( _a51_argument A51_BINARY A51_SHADER_KIT A51_PACKAGE_DIR )
    if( NOT DEFINED ${_a51_argument} OR "${${_a51_argument}}" STREQUAL "" )
        message( FATAL_ERROR "${_a51_argument} is required." )
    endif()
    get_filename_component( ${_a51_argument} "${${_a51_argument}}"
        ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )
endforeach()

if( NOT EXISTS "${A51_BINARY}" OR IS_DIRECTORY "${A51_BINARY}" )
    message( FATAL_ERROR "Runtime binary is missing: ${A51_BINARY}" )
endif()

file( GLOB_RECURSE _a51_shader_packages
    RELATIVE "${A51_SHADER_KIT}" "${A51_SHADER_KIT}/*.ecs" )
if( NOT _a51_shader_packages )
    message( FATAL_ERROR "No ECS shader packages found in ${A51_SHADER_KIT}." )
endif()

file( MAKE_DIRECTORY "${A51_PACKAGE_DIR}/SHADERS" )
file( COPY "${A51_BINARY}" DESTINATION "${A51_PACKAGE_DIR}" )

foreach( _a51_shader IN LISTS _a51_shader_packages )
    get_filename_component( _a51_shader_directory "${_a51_shader}" DIRECTORY )
    file( MAKE_DIRECTORY "${A51_PACKAGE_DIR}/SHADERS/${_a51_shader_directory}" )
    file( COPY "${A51_SHADER_KIT}/${_a51_shader}"
        DESTINATION "${A51_PACKAGE_DIR}/SHADERS/${_a51_shader_directory}" )
endforeach()
