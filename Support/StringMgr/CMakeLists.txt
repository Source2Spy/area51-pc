a51_add_project( StringMgr STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/StringMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/StringMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/StringTable.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/StringTable.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/StringResource.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/Support"
    DEPENDENCIES
        Parsing
)
