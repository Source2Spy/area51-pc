set( A51_NETWORK_PLATFORM_SOURCES )
set( A51_NETWORK_PLATFORM_DEPENDENCIES )
set( A51_SPEEX_ROOT "${CMAKE_SOURCE_DIR}/xCore/3rdParty/Speex" )
set( A51_SPEEX_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/speex" )
file( MAKE_DIRECTORY "${A51_SPEEX_GENERATED_INCLUDE_DIR}" )
set( INCLUDE_STDINT "#include <stdint.h>" )
set( SIZE16 "int16_t" )
set( USIZE16 "uint16_t" )
set( SIZE32 "int32_t" )
set( USIZE32 "uint32_t" )
configure_file(
    "${A51_SPEEX_ROOT}/include/speex/speex_config_types.h.in"
    "${A51_SPEEX_GENERATED_INCLUDE_DIR}/speex_config_types.h"
    @ONLY
)
set( A51_SPEEX_SOURCES
    "${A51_SPEEX_ROOT}/libspeex/bits.c"
    "${A51_SPEEX_ROOT}/libspeex/cb_search.c"
    "${A51_SPEEX_ROOT}/libspeex/exc_10_16_table.c"
    "${A51_SPEEX_ROOT}/libspeex/exc_10_32_table.c"
    "${A51_SPEEX_ROOT}/libspeex/exc_20_32_table.c"
    "${A51_SPEEX_ROOT}/libspeex/exc_5_256_table.c"
    "${A51_SPEEX_ROOT}/libspeex/exc_5_64_table.c"
    "${A51_SPEEX_ROOT}/libspeex/exc_8_128_table.c"
    "${A51_SPEEX_ROOT}/libspeex/fftwrap.c"
    "${A51_SPEEX_ROOT}/libspeex/filters.c"
    "${A51_SPEEX_ROOT}/libspeex/gain_table.c"
    "${A51_SPEEX_ROOT}/libspeex/gain_table_lbr.c"
    "${A51_SPEEX_ROOT}/libspeex/hexc_10_32_table.c"
    "${A51_SPEEX_ROOT}/libspeex/hexc_table.c"
    "${A51_SPEEX_ROOT}/libspeex/high_lsp_tables.c"
    "${A51_SPEEX_ROOT}/libspeex/kiss_fft.c"
    "${A51_SPEEX_ROOT}/libspeex/kiss_fftr.c"
    "${A51_SPEEX_ROOT}/libspeex/lpc.c"
    "${A51_SPEEX_ROOT}/libspeex/lsp.c"
    "${A51_SPEEX_ROOT}/libspeex/lsp_tables_nb.c"
    "${A51_SPEEX_ROOT}/libspeex/ltp.c"
    "${A51_SPEEX_ROOT}/libspeex/modes.c"
    "${A51_SPEEX_ROOT}/libspeex/modes_wb.c"
    "${A51_SPEEX_ROOT}/libspeex/nb_celp.c"
    "${A51_SPEEX_ROOT}/libspeex/quant_lsp.c"
    "${A51_SPEEX_ROOT}/libspeex/sb_celp.c"
    "${A51_SPEEX_ROOT}/libspeex/smallft.c"
    "${A51_SPEEX_ROOT}/libspeex/speex.c"
    "${A51_SPEEX_ROOT}/libspeex/speex_callbacks.c"
    "${A51_SPEEX_ROOT}/libspeex/speex_header.c"
    "${A51_SPEEX_ROOT}/libspeex/stereo.c"
    "${A51_SPEEX_ROOT}/libspeex/vbr.c"
    "${A51_SPEEX_ROOT}/libspeex/vorbis_psy.c"
    "${A51_SPEEX_ROOT}/libspeex/vq.c"
    "${A51_SPEEX_ROOT}/libspeex/window.c"
)
set( A51_SPEEX_HEADERS
    "${A51_SPEEX_ROOT}/include/speex/speex.h"
    "${A51_SPEEX_ROOT}/include/speex/speex_bits.h"
    "${A51_SPEEX_ROOT}/include/speex/speex_callbacks.h"
    "${A51_SPEEX_ROOT}/include/speex/speex_config_types.h.in"
    "${A51_SPEEX_ROOT}/include/speex/speex_header.h"
    "${A51_SPEEX_ROOT}/include/speex/speex_stereo.h"
    "${A51_SPEEX_ROOT}/include/speex/speex_types.h"
    "${A51_SPEEX_ROOT}/libspeex/arch.h"
    "${A51_SPEEX_ROOT}/libspeex/cb_search.h"
    "${A51_SPEEX_ROOT}/libspeex/cb_search_sse.h"
    "${A51_SPEEX_ROOT}/libspeex/filters.h"
    "${A51_SPEEX_ROOT}/libspeex/filters_sse.h"
    "${A51_SPEEX_ROOT}/libspeex/fixed_arm4.h"
    "${A51_SPEEX_ROOT}/libspeex/fixed_arm5e.h"
    "${A51_SPEEX_ROOT}/libspeex/fixed_bfin.h"
    "${A51_SPEEX_ROOT}/libspeex/fixed_debug.h"
    "${A51_SPEEX_ROOT}/libspeex/fixed_generic.h"
    "${A51_SPEEX_ROOT}/libspeex/fftwrap.h"
    "${A51_SPEEX_ROOT}/libspeex/kiss_fft.h"
    "${A51_SPEEX_ROOT}/libspeex/kiss_fftr.h"
    "${A51_SPEEX_ROOT}/libspeex/lpc.h"
    "${A51_SPEEX_ROOT}/libspeex/lsp.h"
    "${A51_SPEEX_ROOT}/libspeex/ltp.h"
    "${A51_SPEEX_ROOT}/libspeex/ltp_sse.h"
    "${A51_SPEEX_ROOT}/libspeex/math_approx.h"
    "${A51_SPEEX_ROOT}/libspeex/modes.h"
    "${A51_SPEEX_ROOT}/libspeex/nb_celp.h"
    "${A51_SPEEX_ROOT}/libspeex/quant_lsp.h"
    "${A51_SPEEX_ROOT}/libspeex/sb_celp.h"
    "${A51_SPEEX_ROOT}/libspeex/smallft.h"
    "${A51_SPEEX_ROOT}/libspeex/speex.def"
    "${A51_SPEEX_ROOT}/libspeex/stack_alloc.h"
    "${A51_SPEEX_ROOT}/libspeex/vbr.h"
    "${A51_SPEEX_ROOT}/libspeex/vq.h"
    "${A51_SPEEX_ROOT}/libspeex/vq_sse.h"
    "${A51_SPEEX_ROOT}/libspeex/vorbis_psy.h"
)

if( WIN32 )
    list( APPEND A51_NETWORK_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Voice/Headset_Windows.cpp"
    )
    list( APPEND A51_NETWORK_PLATFORM_DEPENDENCIES winmm )
elseif( CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$" )
    list( APPEND A51_NETWORK_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Voice/Headset_POSIX.cpp"
    )
else()
    message( FATAL_ERROR "NetworkMgr has no platform backend for ${CMAKE_SYSTEM_NAME}" )
endif()

include( A51BuildAuthentication )

a51_add_project( NetworkMgr STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../Objects/Actor/ActorNet.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../Objects/NetGhost.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../Objects/Player/PlayerNet.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/ClientProxy.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/BuildAuthentication.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/ClientProxyStates.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/ConnMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameClient.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameClientStates.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameServer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameServerStates.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameState.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/GameMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Base.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_CNH.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_CTF.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Campaign.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_DM.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Infect.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_TDM.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Tag.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/DirectoryClient.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/LanDiscovery.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/MatchMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/MatchProvider.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/EndpointResolver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/Messages.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/Msg.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/MsgClient.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/MsgMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/MsgQueue.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/NetworkMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/GMMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/NetObj.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/NetUpdate.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/NetObjMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/PainQueue.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/UpdateMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/Headset_Common.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/Speex.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/VoiceMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/VoiceProxy.cpp"
        ${A51_NETWORK_PLATFORM_SOURCES}
        ${A51_SPEEX_SOURCES}
        "${CMAKE_CURRENT_LIST_DIR}/../Objects/NetGhost.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/ClientProxy.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/BuildAuthentication.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/BuildAuthenticationKey.hpp.in"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/ConnMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/ConnectionProtocol.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameClient.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameServer.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Connection/GameState.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/GameMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/PlayerStats.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/GameTypes.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Base.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_CNH.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_CTF.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Campaign.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_DM.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Infect.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_TDM.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Game/logic_Tag.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/DirectoryClient.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/LanDiscovery.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/MatchPacket.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/MatchMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/MatchBrowser.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/MatchProvider.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/EndpointResolver.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Match/MatchServer.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/Messages.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/Msg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/MsgClient.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/MsgMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Messages/MsgQueue.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/NetLimits.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/NetworkMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/Blender.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/GMMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/NetObj.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/NetUpdate.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/NetObjMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/PainQueue.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Replication/UpdateMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ServerVersion.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/Headset.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/Speex.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/VoiceMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Voice/VoiceProxy.hpp"
        ${A51_SPEEX_HEADERS}
    DEPENDENCIES
        a51::monocypher
        ${A51_NETWORK_PLATFORM_DEPENDENCIES}
    PRIVATE_INCLUDE_DIRECTORIES
        "${A51_BUILD_AUTHENTICATION_INCLUDE_DIR}"
        "${A51_SPEEX_GENERATED_INCLUDE_DIR}"
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/../Support"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary"
        "${A51_SPEEX_ROOT}/include"
        "${A51_SPEEX_ROOT}/libspeex"
    COMPILE_DEFINITIONS
        FLOATING_POINT
        USE_KISS_FFT
        "EXPORT="
    )
