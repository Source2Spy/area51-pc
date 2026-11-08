if( WIN32 )
    set( X_FILES_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_atomic_windows.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_threads_windows.cpp"
    )
    set( X_FILES_PLATFORM_DEPENDENCIES )
elseif( CMAKE_SYSTEM_NAME MATCHES "^(Linux|Android)$" )
    find_package( Threads REQUIRED )
    set( X_FILES_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_atomic_posix.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_threads_posix.cpp"
    )
    set( X_FILES_PLATFORM_DEPENDENCIES Threads::Threads )

    if( CMAKE_SYSTEM_NAME STREQUAL "Android" )
        list( APPEND X_FILES_PLATFORM_DEPENDENCIES log )
    endif()

    # The libatomic library is required for 64-bit atomics on 32-bit Linux.
    if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
        find_library( A51_ATOMIC_LIBRARY NAMES atomic )
        if( A51_ATOMIC_LIBRARY )
            list( APPEND X_FILES_PLATFORM_DEPENDENCIES "${A51_ATOMIC_LIBRARY}" )
        endif()
    endif()
else()
    message( FATAL_ERROR "x_files has no threading backend for ${CMAKE_SYSTEM_NAME}." )
endif()

a51_add_project( x_files STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    DEPENDENCIES ${X_FILES_PLATFORM_DEPENDENCIES}
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_atomic.cpp"
        ${X_FILES_PLATFORM_SOURCES}
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitmap.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitmap_cmap.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitmap_convert.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitmap_dxtc.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitmap_io.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitmap_quant.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitstream.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bytestream.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_debug.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_files.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_flare.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_hash.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_hash.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_locale.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_log.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_basic.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_misc.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_sphere.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_memfile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_memory.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_mqueue.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_mutex.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_plus.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_profile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_profile_tracy_client.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_stdio.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_string.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_threads.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_workers.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_time.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_time_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_atomic.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_array.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_bitmap.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_bitstream.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_bytestream.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_color.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_command_queue.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_debug.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_files.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_flare.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_locale.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_log.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_math.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_math_misc.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_memfile.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_memory.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_plus.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_profile.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_profile_tracy.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_stdio.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_string.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_target.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_threads.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_workers.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_time.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/x_types.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_bitmap_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_color_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_bb_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_m4_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_p_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_q_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_r3_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_rect_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_sph_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_v2_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_v3_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_math_v4_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_string_inline.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_array_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_files_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_log_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_memory_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_mqueue_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_mutex_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_semaphore_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_stdio_private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Implementation/x_threads_private.hpp"
)

if( CMAKE_SYSTEM_NAME STREQUAL "Android" AND
    CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    target_compile_options( x_files PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-Wno-register>" )
endif()
