add_subdirectory( libmp3lame )
a51_add_project( SoundPackager EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/aiff_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/audio_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Compress_PC.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/endian.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ExportPackage.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/imaadpcm.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ParseScript.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ProcessSample.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SoundPackager.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/wav_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/aiff_file.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/audio_file.hpp"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy/Audio/audio_package_format.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/Compress.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/endian.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ExportPackage.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/imaadpcm.h"
        "${CMAKE_CURRENT_LIST_DIR}/PackageTypes.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ParseScript.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/SoundPackager.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/wav_file.hpp"
    DEPENDENCIES
        CommandLine
        Parsing
        x_files
        libmp3lame_vc6
)

target_include_directories( SoundPackager PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/libmp3lame"
    "${CMAKE_CURRENT_LIST_DIR}/mpglib"
    "${CMAKE_SOURCE_DIR}/xCore/Entropy/Audio"
)
