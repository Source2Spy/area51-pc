add_subdirectory( AudioMgr )
add_subdirectory( ConversationMgr )
add_subdirectory( Dialogs )
add_subdirectory( EventMgr )
add_subdirectory( Font )
add_subdirectory( FX )
add_subdirectory( GameLib )
add_subdirectory( Music_mgr )
add_subdirectory( NetworkMgr )
add_subdirectory( Render )
add_subdirectory( ScriptMgr )
add_subdirectory( StringMgr )

set( _a51_support_targets
    AudioMgr
    ConversationMgr
    UserInterface
    EventMgr
    Font
    FX
    Gamelib
    Music_mgr
    NetworkMgr
    Render
    ScriptMgr
    StringMgr
)

add_custom_target( a51_support )
add_dependencies( a51_support ${_a51_support_targets} )

if( WIN32 AND A51_BUILD_WINDOWS_TARGETS )
    add_subdirectory( Compilers/DecalCompiler )
    add_subdirectory( Compilers/ResourceTool )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    add_subdirectory( Compilers/DecalCompiler )
    add_subdirectory( Compilers/ResourceTool )
else()
    message( STATUS "Skipping compiler tools: host platform is not supported." )
endif()
