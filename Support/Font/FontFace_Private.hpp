//=========================================================================
//
//  FontFace_Private.hpp
//
//=========================================================================

#ifndef FONT_FACE_PRIVATE_HPP
#define FONT_FACE_PRIVATE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontFace.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_MULTIPLE_MASTERS_H

#include <hb.h>
#include <hb-ot.h>

//=========================================================================
//  STRUCTURES
//=========================================================================

struct FontFace::Data
{
    xarray<u8>  Bytes;
    FT_Face     Face      = NULL;
    hb_blob_t*  Blob      = NULL;
    hb_face_t*  ShapeFace = NULL;
    xarray<f32> ShapeCoordinates;

    hb_font_t*  CreateShapeFont( f32 Size ) const;
};

//=========================================================================
#endif // FONT_FACE_PRIVATE_HPP
//=========================================================================
