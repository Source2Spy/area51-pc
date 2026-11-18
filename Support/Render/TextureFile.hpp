//=========================================================================
//
//  TextureFile.hpp
//
//=========================================================================

#ifndef TEXTURE_FILE_HPP
#define TEXTURE_FILE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_stdio.hpp"

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class xbitmap;

//=========================================================================
//  TYPES
//=========================================================================

class texture_file
{
public:

    // Reads a compiled texture from an already-open stream without closing it.
    static xbool   LoadXBMP         ( xbitmap& Bitmap, X_FILE*    pFile );

    // Tries the requested .xbmp path, then its .png and .tga siblings.
    static X_FILE* LoadWithFallback ( xbitmap& Bitmap, char const* pFileName );
};

//=========================================================================
#endif // TEXTURE_FILE_HPP
//=========================================================================
