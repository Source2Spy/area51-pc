add_subdirectory( x_files )
add_subdirectory( Parsing )
add_subdirectory( Auxiliary/CommandLine )
add_subdirectory( Entropy/Shader )

if( A51_SHADER_TOOLS_ONLY )
    return()
endif()

add_subdirectory( Entropy )
add_subdirectory( MeshUtil )
add_subdirectory( Auxiliary/Bitmap )
add_subdirectory( Auxiliary/fx_Core )
