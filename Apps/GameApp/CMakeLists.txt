set( A51_GAME_APP_PLATFORM_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/GameAppPlatform.hpp"
)

install( DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/media/FONTS"
    DESTINATION .
    COMPONENT Runtime
)

install( DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/media/strings/"
    DESTINATION STRINGS
    COMPONENT Runtime
    FILES_MATCHING PATTERN "*.string"
)

set( A51_GAME_APP_WINDOWS_RESOURCES )

if( WIN32 )
    list( APPEND A51_GAME_APP_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/GameAppPlatform_Windows.cpp"
    )
    list( APPEND A51_GAME_APP_WINDOWS_RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/A51.rc"
    )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    list( APPEND A51_GAME_APP_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/GameAppPlatform_Linux.cpp"
    )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Android" )
    list( APPEND A51_GAME_APP_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/GameAppPlatform_Android.cpp"
    )
else()
    message( FATAL_ERROR "GameApp has no platform backend for ${CMAKE_SYSTEM_NAME}" )
endif()

set( A51_GAME_APP_DEPENDENCIES
    AudioMgr
    ConversationMgr
    UserInterface
    EventMgr
    Gamelib
    Music_mgr
    ScriptMgr
    NetworkMgr
    Render
    StringMgr
    aux_Bitmap
    FX
    Entropy
    Parsing
    x_files
)

if( CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
    # Group circular static-library dependencies for GNU ld.
    list( INSERT A51_GAME_APP_DEPENDENCIES 0 "-Wl,--start-group" )
    list( APPEND A51_GAME_APP_DEPENDENCIES "-Wl,--end-group" )
endif()

if( CMAKE_SYSTEM_NAME STREQUAL "Android" )
    set( A51_GAME_APP_TARGET main )
    set( A51_GAME_APP_TYPE SHARED )
    set( A51_GAME_APP_OPTIONS )
else()
    set( A51_GAME_APP_TARGET A51 )
    set( A51_GAME_APP_TYPE EXECUTABLE )
    set( A51_GAME_APP_OPTIONS WIN32 )
endif()

a51_add_project( ${A51_GAME_APP_TARGET} ${A51_GAME_APP_TYPE} ${A51_GAME_APP_OPTIONS}
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Config.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FramePacer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FrameTiming.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/IniFile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Version.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Config.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FramePacer.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FrameTiming.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/IniFile.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/main.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/main_desktop.inl"
        "${CMAKE_CURRENT_LIST_DIR}/main_mobile.inl"
        "${CMAKE_CURRENT_LIST_DIR}/A51Resources.hpp"
        ${A51_GAME_APP_PLATFORM_SOURCES}
        ${A51_GAME_APP_WINDOWS_RESOURCES}
    DEPENDENCIES
        ${A51_GAME_APP_DEPENDENCIES}
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/3rdParty/SDL3/include"
)
