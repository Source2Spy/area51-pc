#==========================================================================
#  Prepared shader kit validation.
#==========================================================================

include_guard( GLOBAL )

function( a51_import_shader_kit KitDirectory )
    set( _manifest "${KitDirectory}/shaderkit.json" )
    if( NOT EXISTS "${_manifest}" )
        message( FATAL_ERROR
            "No prepared shader kit at ${KitDirectory}. "
            "Build a51_prepare_shaders with the shader-tools preset, "
            "or set A51_SHADER_KIT to a prepared kit directory." )
    endif()

    file( READ "${_manifest}" _json )
    string( JSON _format GET "${_json}" format )
    if( NOT _format EQUAL 1 )
        message( FATAL_ERROR "Unsupported shader kit format: ${_manifest}" )
    endif()

    string( JSON _count LENGTH "${_json}" files )
    if( _count EQUAL 0 )
        message( FATAL_ERROR "Empty shader kit: ${_manifest}" )
    endif()
    math( EXPR _last "${_count} - 1" )
    set( _files )
    foreach( _index RANGE 0 ${_last} )
        string( JSON _path GET "${_json}" files ${_index} path )
        string( JSON _expected GET "${_json}" files ${_index} sha256 )
        if( IS_ABSOLUTE "${_path}" OR _path MATCHES "(^|/)\\.\\.(/|$)" )
            message( FATAL_ERROR "Invalid shader kit path: ${_path}" )
        endif()
        if( NOT EXISTS "${KitDirectory}/${_path}" )
            message( FATAL_ERROR "Shader kit file is missing: ${KitDirectory}/${_path}" )
        endif()
        file( SHA256 "${KitDirectory}/${_path}" _actual )
        if( NOT _actual STREQUAL _expected )
            message( FATAL_ERROR "Shader kit file does not match its manifest: ${KitDirectory}/${_path}" )
        endif()
        list( APPEND _files "${_path}" )
        set_property( DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${KitDirectory}/${_path}" )
    endforeach()

    foreach( _required
        catalog.json
        generated/a51_shader_contract.hpp
        generated/a51_shader_contract.hlsl
        generated/a51_shader_programs.hpp
        generated/a51_shader_catalog.hpp
        generated/a51_shader_combos.hpp
        ${ARGN} )
        if( NOT _required IN_LIST _files )
            message( FATAL_ERROR "Shader kit manifest is missing ${_required}: ${_manifest}" )
        endif()
        if( _required MATCHES "^generated/a51_compute_(.+)\\.hpp$" )
            set( _compute_name "${CMAKE_MATCH_1}" )
            file( STRINGS "${KitDirectory}/${_required}" _descriptor
                REGEX "inline xbool A51Compute_${_compute_name}_GetDesc\\(" LIMIT_COUNT 1 )
            if( NOT _descriptor )
                message( FATAL_ERROR
                    "Outdated compute shader header: ${KitDirectory}/${_required}. "
                    "Regenerate the kit with the current shader-tools ShaderTool; "
                    "this header is missing A51Compute_${_compute_name}_GetDesc." )
            endif()
        endif()
    endforeach()
    list( FILTER _files INCLUDE REGEX "\\.ecs$" )
    if( NOT _files )
        message( FATAL_ERROR "Shader kit contains no ECS packages: ${_manifest}" )
    endif()

    set_property( DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_manifest}" )
    message( STATUS "Using prepared shader kit: ${KitDirectory}" )
endfunction()
