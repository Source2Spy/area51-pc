a51_add_project( AudioMgr STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/AudioMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/AudioMgr.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/Support"
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/Support/AudioMgr"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary"
)

find_package( Python3 QUIET COMPONENTS Interpreter )
if( Python3_Interpreter_FOUND )
    add_custom_target( a51_check_audio_calls
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/Tools/check_audio_calls.py"
        VERBATIM
    )
    add_dependencies( AudioMgr a51_check_audio_calls )
endif()
