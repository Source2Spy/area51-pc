# Host and runtime share the ECS wire codec without linking the renderer.
a51_add_project( a51_ecs_codec STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ecs_block.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ecs_package.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ecs_block_cache.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ecs_package_store.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ecs_block.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ecs_package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ecs_block_cache.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ecs_package_store.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}/.."
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
    DEPENDENCIES x_files libzstd_static )
set_target_properties( a51_ecs_codec PROPERTIES POSITION_INDEPENDENT_CODE ON )

