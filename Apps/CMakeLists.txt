set( _a51_portable_app_targets )
set( _a51_windows_app_targets )

if( A51_BUILD_SELECTED_APPS )
    if( NOT CMAKE_SYSTEM_NAME STREQUAL "Android" )
        add_subdirectory( XbmpTool )
        add_subdirectory( Art2Code )
        list( APPEND _a51_portable_app_targets xbmptool Art2Code )
    else()
        message( STATUS "Skipping desktop asset tools for Android." )
    endif()

    if( WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux" )
        add_subdirectory( DfsTool )
        list( APPEND _a51_portable_app_targets dfsTool )
    else()
        message( STATUS "Skipping DfsTool: unsupported host platform." )
    endif()

    if( A51_ENABLE_QT AND Qt6_FOUND )
        add_subdirectory( XbmpViewer )
        list( APPEND _a51_portable_app_targets XBMPViewer )
    else()
        message( STATUS "Skipping XBMPViewer: Qt6 is not available." )
    endif()

    if( WIN32 AND A51_BUILD_WINDOWS_TARGETS )
        add_subdirectory( GameApp )
        list( APPEND _a51_windows_app_targets A51 )
    elseif( CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$" )
        add_subdirectory( GameApp )
        if( CMAKE_SYSTEM_NAME STREQUAL "Android" )
            list( APPEND _a51_portable_app_targets main )
        else()
            list( APPEND _a51_portable_app_targets A51 )
        endif()
    endif()

    if( WIN32 AND A51_BUILD_WINDOWS_TARGETS )
        add_subdirectory( SoundPackager )
        list( APPEND _a51_windows_app_targets SoundPackager )
    endif()

    add_custom_target( a51_portable_apps )
    add_dependencies( a51_portable_apps ${_a51_portable_app_targets} )

    if( _a51_windows_app_targets )
        add_custom_target( a51_windows_apps )
        add_dependencies( a51_windows_apps ${_a51_windows_app_targets} )
    endif()

    add_custom_target( a51_selected_apps )
    add_dependencies( a51_selected_apps
        ${_a51_portable_app_targets}
        ${_a51_windows_app_targets}
    )
endif()

if( A51_BUILD_SHADER_TOOL )
    add_subdirectory( ShaderTool )
endif()
