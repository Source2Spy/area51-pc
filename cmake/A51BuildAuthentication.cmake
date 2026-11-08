#==========================================================================
#
#  A51BuildAuthentication.cmake
#
#==========================================================================

option( A51_REQUIRE_BUILD_KEY "Require an official multiplayer build key" OFF )

set( _a51_build_key "$ENV{A51_BUILD_KEY}" )
set( A51_HAS_BUILD_KEY 0 )
set( A51_BUILD_KEY_BYTES "" )

if( NOT _a51_build_key STREQUAL "" )
    string( LENGTH "${_a51_build_key}" _a51_build_key_length )

    if( NOT _a51_build_key_length EQUAL 64 OR
        NOT _a51_build_key MATCHES "^[0-9a-fA-F]+$" OR
        _a51_build_key MATCHES "^0+$" )
        message( FATAL_ERROR "A51_BUILD_KEY must contain 32 random bytes encoded as 64 hexadecimal characters." )
    endif()

    set( A51_HAS_BUILD_KEY 1 )
    string( REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1, "
        A51_BUILD_KEY_BYTES "${_a51_build_key}" )
elseif( A51_REQUIRE_BUILD_KEY )
    message( FATAL_ERROR "This build requires the A51_BUILD_KEY environment variable." )
endif()

if( NOT DEFINED A51_BUILD_AUTHENTICATION_INCLUDE_DIR )
    set( A51_BUILD_AUTHENTICATION_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/authentication" )
endif()
file( MAKE_DIRECTORY "${A51_BUILD_AUTHENTICATION_INCLUDE_DIR}" )
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/../Support/NetworkMgr/Connection/BuildAuthenticationKey.hpp.in"
    "${A51_BUILD_AUTHENTICATION_INCLUDE_DIR}/BuildAuthenticationKey.hpp"
    @ONLY )

unset( _a51_build_key )
unset( _a51_build_key_length )
unset( A51_BUILD_KEY_BYTES )
