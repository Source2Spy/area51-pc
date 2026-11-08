a51_add_project( Font STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Font.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontCollection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontFace.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/TextLayout.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/GlyphAtlas.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Font.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontTypes.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontCollection.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontFace.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FontFace_Private.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/TextLayout.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/GlyphAtlas.hpp"
    DEPENDENCIES Parsing Entropy a51::freetype a51::harfbuzz a51::icu
)
