#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <io.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "x_files.hpp"
#include "CommandLine.hpp"
#include "Auxiliary/Bitmap/aux_Bitmap.hpp"
#include "../Support/Decals/DecalPackage.hpp"
#include "../Support/Decals/DecalPackageFile.hpp"
#include "Compilers/Shared/BMPUtil.hpp"
#include <windows.h>

//=========================================================================
// Types
//=========================================================================

enum export_platform
{
    EXPORT_UNKNOWN = 0,
    EXPORT_DESKTOP,
    EXPORT_PS2,
    EXPORT_XBOX
};

enum format_flags
{
    BITMAP_FORMAT_8BIT      = 0x1,
    BITMAP_FORMAT_INTENSITY = 0x2,
};

struct platform_output
{
    export_platform Platform;
    xstring         FileName;
};

//=========================================================================
// Statics
//=========================================================================

static          xbool       s_Verbose         = FALSE;
static          s32         s_Platform        = EXPORT_UNKNOWN;
static struct   _finddata_t s_ExeData;
static struct   _finddata_t s_SrcBitmapData;
static struct   _finddata_t s_DstBitmapData;

//=========================================================================
// Implementation
//=========================================================================

xbool FloatBitsEqual( f32 A, f32 B )
{
    u32 ABits;
    u32 BBits;
    x_memcpy( &ABits, &A, sizeof(ABits) );
    x_memcpy( &BBits, &B, sizeof(BBits) );
    return( ABits == BBits );
}

//=========================================================================

xbool EqualPackages( const decal_package& A, const decal_package& B )
{
    if( (A.GetNGroups() != B.GetNGroups()) ||
        (A.GetNDecalDefs() != B.GetNDecalDefs()) )
    {
        return( FALSE );
    }

    for( s32 i = 0; i < A.GetNGroups(); i++ )
    {
        if( x_strcmp( A.GetGroupName( i ), B.GetGroupName( i ) ) ||
            ((u32)A.GetGroupColor( i ) != (u32)B.GetGroupColor( i )) ||
            (A.GetGroupDecalDefStart( i ) != B.GetGroupDecalDefStart( i )) ||
            (A.GetNDecalDefs( i ) != B.GetNDecalDefs( i )) )
        {
            return( FALSE );
        }
    }

    for( s32 i = 0; i < A.GetNDecalDefs(); i++ )
    {
        const decal_definition& DA = A.GetDecalDef( i );
        const decal_definition& DB = B.GetDecalDef( i );
        if( x_strcmp( DA.m_Name, DB.m_Name ) ||
            !FloatBitsEqual( DA.m_MinSize.X, DB.m_MinSize.X ) ||
            !FloatBitsEqual( DA.m_MinSize.Y, DB.m_MinSize.Y ) ||
            !FloatBitsEqual( DA.m_MaxSize.X, DB.m_MaxSize.X ) ||
            !FloatBitsEqual( DA.m_MaxSize.Y, DB.m_MaxSize.Y ) ||
            !FloatBitsEqual( DA.m_MinRoll, DB.m_MinRoll ) ||
            !FloatBitsEqual( DA.m_MaxRoll, DB.m_MaxRoll ) ||
            ((u32)DA.m_Color != (u32)DB.m_Color) ||
            x_strcmp( DA.m_BitmapName, DB.m_BitmapName ) ||
            (DA.m_MaxVisible != DB.m_MaxVisible) ||
            !FloatBitsEqual( DA.m_FadeTime, DB.m_FadeTime ) ||
            (DA.m_Flags != DB.m_Flags) ||
            (DA.m_BlendMode != DB.m_BlendMode) )
        {
            return( FALSE );
        }
    }

    return( TRUE );
}

//=========================================================================

xbool LoadDecalPackage( const char* pFileName,
                        decal_package*& pPackage,
                        xstring& Error )
{
    pPackage = NULL;

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        Error = "Unable to open the decal package for verification.";
        return( FALSE );
    }

    const xbool Result = decal_package_file::Load( pFile, pPackage, Error );
    x_fclose( pFile );
    return( Result );
}

//=========================================================================

void SaveDecalPackage( const char* pFileName, const decal_package& Package )
{
    if( !pFileName || !pFileName[0] )
    {
        x_throw( "Decal package output filename is empty." );
        return;
    }

    xstring TempFileName( pFileName );
    TempFileName += ".bitsery.tmp";
    if( GetFileAttributesA( TempFileName ) != INVALID_FILE_ATTRIBUTES )
    {
        x_throw( xfs( "Temporary decal package already exists: %s",
                      (const char*)TempFileName ) );
        return;
    }

    xstring Error;
    if( !decal_package_file::Save( TempFileName, Package, Error ) )
    {
        DeleteFileA( TempFileName );
        x_throw( (const char*)Error );
        return;
    }

    decal_package* pVerification = NULL;
    if( !LoadDecalPackage( TempFileName, pVerification, Error ) )
    {
        DeleteFileA( TempFileName );
        x_throw( (const char*)Error );
        return;
    }

    const xbool Matches = EqualPackages( Package, *pVerification );
    delete pVerification;
    if( !Matches )
    {
        DeleteFileA( TempFileName );
        x_throw( "Bitsery decal package verification failed." );
        return;
    }

    if( !MoveFileExA( TempFileName,
                      pFileName,
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) )
    {
        const DWORD ErrorCode = GetLastError();
        DeleteFileA( TempFileName );
        x_throw( xfs( "Unable to install verified decal package (Win32 error %lu).",
                      (unsigned long)ErrorCode ) );
    }
}

//=========================================================================

xbool IsTextEnd( const char* pText )
{
    while( pText && *pText && isspace( (unsigned char)*pText ) )
    {
        pText++;
    }
    return( pText && !*pText );
}

//=========================================================================

s32 ParseS32( const xstring& Text,
              s32            Minimum,
              s32            Maximum,
              const char*    pDescription )
{
    const char* pStart = Text;
    char*       pEnd   = NULL;
    errno = 0;
    const long Value = strtol( pStart, &pEnd, 10 );
    if( (errno == ERANGE) || (pEnd == pStart) || !IsTextEnd( pEnd ) ||
        (Value < Minimum) || (Value > Maximum) )
    {
        x_throw( xfs( "Invalid %s: %s", pDescription, pStart ) );
        return( Minimum );
    }
    return( (s32)Value );
}

//=========================================================================

u32 ParseU32( const xstring& Text,
              s32            Radix,
              const char*    pDescription )
{
    const char* pStart = Text;
    while( *pStart && isspace( (unsigned char)*pStart ) )
    {
        pStart++;
    }
    if( *pStart == '-' )
    {
        x_throw( xfs( "Invalid %s: %s", pDescription, (const char*)Text ) );
        return( 0 );
    }

    char* pEnd = NULL;
    errno = 0;
    const unsigned long Value = strtoul( pStart, &pEnd, Radix );
    if( (errno == ERANGE) || (pEnd == pStart) || !IsTextEnd( pEnd ) ||
        (Value > UINT_MAX) )
    {
        x_throw( xfs( "Invalid %s: %s", pDescription, (const char*)Text ) );
        return( 0 );
    }
    return( (u32)Value );
}

//=========================================================================

f32 ParseF32( const xstring& Text, const char* pDescription )
{
    const char* pStart = Text;
    char*       pEnd   = NULL;
    errno = 0;
    const double Value = strtod( pStart, &pEnd );
    const f32    Result = (f32)Value;
    if( (errno == ERANGE) || (pEnd == pStart) || !IsTextEnd( pEnd ) ||
        (Value < -FLT_MAX) || (Value > FLT_MAX) || !x_isvalid( Result ) )
    {
        x_throw( xfs( "Invalid %s: %s", pDescription, pStart ) );
        return( 0.0f );
    }
    return( Result );
}

//=========================================================================

void RequireGroup( s32 CurrGroup, const char* pOption )
{
    if( CurrGroup < 0 )
    {
        x_throw( xfs( "-%s requires a preceding -GROUP option.", pOption ) );
    }
}

//=========================================================================

void RequireDecal( s32 CurrDecal, const char* pOption )
{
    if( CurrDecal < 0 )
    {
        x_throw( xfs( "-%s requires a preceding -DECAL option.", pOption ) );
    }
}

//=========================================================================

void RequireTextLength( const xstring& Text,
                        s32            MaximumLength,
                        const char*    pDescription )
{
    if( Text.GetLength() > MaximumLength )
    {
        x_throw( xfs( "%s is longer than %d characters.",
                      pDescription,
                      MaximumLength ) );
    }
}

//=========================================================================

const char* CompileBitmap( const char* pOutputPath, const char* pSourceBitmap, u32 FormatFlags )
{
    if( !pOutputPath || !pSourceBitmap ||
        (x_strlen( pOutputPath ) >= X_MAX_PATH) ||
        (x_strlen( pSourceBitmap ) >= X_MAX_PATH) )
    {
        x_throw( "Decal bitmap input or output path is invalid or too long." );
    }

    // figure out what the name of the final bitmap should be
    char SrcDrive[X_MAX_DRIVE];
    char SrcDir[X_MAX_DIR];
    char SrcFName[X_MAX_FNAME];
    char SrcExt[X_MAX_EXT];
    char DstDrive[X_MAX_DRIVE];
    char DstDir[X_MAX_DIR];
    char DstFName[X_MAX_FNAME];
    char DstExt[X_MAX_EXT];
    static char FinalPath[X_MAX_PATH];

    FinalPath[0] = '\0';
    x_splitpath( pSourceBitmap, SrcDrive, SrcDir, SrcFName, SrcExt );
    x_splitpath( pOutputPath, DstDrive, DstDir, DstFName, DstExt );
    if( x_strlen( SrcFName ) > (X_MAX_FNAME - 4) )
    {
        x_throw( "Decal bitmap filename is too long for a format suffix." );
    }
    if ( FormatFlags & BITMAP_FORMAT_INTENSITY )
        x_strcat( SrcFName, "[I]" );
    else if ( FormatFlags & BITMAP_FORMAT_8BIT )
        x_strcat( SrcFName, "[8]" );
    else
        x_strcat( SrcFName, "[4]" );
    x_makepath( FinalPath, DstDrive, DstDir, SrcFName, "xbmp" );

    // see if the output file is already up-to-date by comparing it to
    // the source bitmap and to the timestamp of this exe
    xbool bOutOfDate = FALSE;
    sptr SrcFindHandle = -1;
    sptr DstFindHandle = -1;
    s_SrcBitmapData.time_write = 0;
    s_DstBitmapData.time_write = 0;
    SrcFindHandle = _findfirst( pSourceBitmap, &s_SrcBitmapData );
    if ( SrcFindHandle == -1 )
    {
        x_throw( "Unable to locate source bitmap." );
        return FinalPath;
    }
    
    DstFindHandle = _findfirst( FinalPath, &s_DstBitmapData );
    if ( DstFindHandle == -1 )
    {
        // the destination bitmap doesn't exist yet, so obviously we need
        // to compile it
        bOutOfDate = TRUE;
    }
    else
    {
        // compare timestamps to see if we should compile the xbmp
        if ( s_DstBitmapData.time_write <= s_SrcBitmapData.time_write )
            bOutOfDate = TRUE;

        if ( s_DstBitmapData.time_write <= s_ExeData.time_write )
            bOutOfDate = TRUE;
    }

    _findclose( SrcFindHandle );
    if ( DstFindHandle != -1 )
        _findclose( DstFindHandle );

    // handle the bitmap compilation
    if ( bOutOfDate == TRUE )
    {
        //load the bitmap
        xbitmap BMP;
        xbool   Result = auxbmp_Load( BMP, pSourceBitmap );
        if ( !Result )
        {
            x_throw( "Unable to load source bitmap." );
            return FinalPath;
        }

        // convert to the platform's native format
        switch ( s_Platform )
        {
        default:
        case EXPORT_UNKNOWN:
            x_throw( "Internal error: platform not specified" );
            break;
        case EXPORT_DESKTOP:
            if ( FormatFlags & BITMAP_FORMAT_INTENSITY )
                bmp_util::ProcessDetailMap( BMP, FALSE );
            auxbmp_ConvertToD3D( BMP );
            break;
        case EXPORT_PS2:
            if ( FormatFlags & BITMAP_FORMAT_INTENSITY )
            {
                bmp_util::ProcessDetailMap( BMP, TRUE );
                ASSERT( BMP.GetBPP() <= 8 );
                BMP.ConvertFormat( xbitmap::FMT_P4_ABGR_8888 );
                bmp_util::ConvertToPS2( BMP, FALSE );
            }
            else
            if ( FormatFlags & BITMAP_FORMAT_8BIT )
            {
                BMP.ConvertFormat( xbitmap::FMT_P8_ABGR_8888 );
                bmp_util::ConvertToPS2( BMP, TRUE );
            }
            else
            {
                BMP.ConvertFormat( xbitmap::FMT_P4_ABGR_8888 );
                bmp_util::ConvertToPS2( BMP, TRUE );
            }
            break;

            case EXPORT_XBOX:
            {
                s32 nMips = 4;
                if( FormatFlags & BITMAP_FORMAT_INTENSITY )
                {
                    bmp_util::ProcessDetailMap( BMP,FALSE );
                    auxbmp_ConvertToD3D( BMP );
                    BMP.BuildMips( nMips );
                    auxbmp_ConvertRGBToA8( BMP );
                }
                else
                {
                    auxbmp_Compress( BMP,pSourceBitmap,nMips );
                }
                break;
            }
        }

        // save out the bitmap
        Result = BMP.Save( FinalPath );
        if ( !Result )
            x_throw( xfs("Unable to save %s.", FinalPath) );
    }

    x_makepath( FinalPath, NULL, NULL, SrcFName, "xbmp" );
    return FinalPath;
}

//=========================================================================

void CompileBitmaps( const char*      pOutputPath,
                     decal_package&   DecalPkg,
                     xarray<xstring>& SourceBitmapNames,
                     xarray<u32>&     PreferredBitmapFormats )
{
    if ( DecalPkg.GetNDecalDefs() != SourceBitmapNames.GetCount() )
    {
        x_throw( "# of decals doesn't match number of bitmaps" );
        return;
    }

    // compile each of the bitmaps in the appropriate formats (this will
    // also check timestamps for us, and skip the compile if it doesn't
    // really need to do it)
    s32 i;
    for ( i = 0; i < DecalPkg.GetNDecalDefs(); i++ )
    {
        decal_definition& DecalDef = DecalPkg.GetDecalDef(i);

        const char* pBitmapName = CompileBitmap( pOutputPath, SourceBitmapNames[i], PreferredBitmapFormats[i] );
        if( x_strlen( pBitmapName ) >= 256 )
        {
            x_throw( "Compiled decal bitmap name is too long for the package." );
        }
        x_strsavecpy( DecalDef.m_BitmapName, pBitmapName, 256 );
    }
}

//=========================================================================

void ExecuteScript( command_line& CommandLine )
{
    decal_package           DecalPkg;
    xarray<xstring>         SourceBitmapNames;
    xarray<u32>             PreferredBitmapFormats;
    xarray<s32>             GroupStarts;
    xarray<s32>             GroupCounts;
    xarray<platform_output> Outputs;
    xbool                   SawNumGroups = FALSE;
    xbool                   SawNumDecals = FALSE;
    s32                     CurrGroup    = -1;
    s32                     CurrDecal    = -1;

    for( s32 i = 0; i < CommandLine.GetNumOptions(); i++ )
    {
        const xstring OptName = CommandLine.GetOptionName( i );

        if( OptName == xstring( "LOG" ) )
        {
            s_Verbose = TRUE;
        }
        else if( OptName == xstring( "NUMGROUPS" ) )
        {
            if( SawNumGroups )
            {
                x_throw( "-NUMGROUPS may only be specified once." );
            }

            const s32 Count = ParseS32(
                CommandLine.GetOptionString( i ),
                0,
                decal_package_file::MAX_GROUPS,
                "decal group count" );
            DecalPkg.AllocGroups( Count );
            GroupStarts.SetCount( Count );
            GroupCounts.SetCount( Count );
            for( s32 j = 0; j < Count; j++ )
            {
                GroupStarts[j] = 0;
                GroupCounts[j] = 0;
            }
            CurrGroup    = -1;
            SawNumGroups = TRUE;
        }
        else if( OptName == xstring( "NUMDECALS" ) )
        {
            if( SawNumDecals )
            {
                x_throw( "-NUMDECALS may only be specified once." );
            }

            const s32 Count = ParseS32(
                CommandLine.GetOptionString( i ),
                0,
                decal_package_file::MAX_DEFINITIONS,
                "decal definition count" );
            DecalPkg.AllocDecals( Count );
            SourceBitmapNames.SetCount( Count );
            PreferredBitmapFormats.SetCount( Count );
            for( s32 j = 0; j < Count; j++ )
            {
                decal_definition& DecalDef = DecalPkg.GetDecalDef( j );
                DecalDef.m_Flags = 0;
                SourceBitmapNames[j].Clear();
                PreferredBitmapFormats[j] = 0;
            }
            CurrDecal    = -1;
            SawNumDecals = TRUE;
        }
        else if( OptName == xstring( "GROUP" ) )
        {
            if( !SawNumGroups )
            {
                x_throw( "-GROUP requires -NUMGROUPS first." );
            }
            if( (CurrGroup + 1) >= DecalPkg.GetNGroups() )
            {
                x_throw( "More -GROUP options were supplied than declared." );
            }

            CurrGroup++;
            const xstring Name = CommandLine.GetOptionString( i );
            RequireTextLength( Name, 31, "Decal group name" );
            DecalPkg.SetGroupName( CurrGroup, Name );
            DecalPkg.SetGroupColor( CurrGroup, XCOLOR_WHITE );
        }
        else if( OptName == xstring( "GROUPCOLOR" ) )
        {
            RequireGroup( CurrGroup, "GROUPCOLOR" );
            DecalPkg.SetGroupColor(
                CurrGroup,
                xcolor( ParseU32( CommandLine.GetOptionString( i ),
                                  16,
                                  "group color" ) ) );
        }
        else if( OptName == xstring( "DECALSTART" ) )
        {
            RequireGroup( CurrGroup, "DECALSTART" );
            GroupStarts[CurrGroup] = ParseS32(
                CommandLine.GetOptionString( i ),
                0,
                decal_package_file::MAX_DEFINITIONS,
                "group decal start" );
        }
        else if( OptName == xstring( "DECALCOUNT" ) )
        {
            RequireGroup( CurrGroup, "DECALCOUNT" );
            GroupCounts[CurrGroup] = ParseS32(
                CommandLine.GetOptionString( i ),
                0,
                decal_package_file::MAX_DEFINITIONS,
                "group decal count" );
        }
        else if( OptName == xstring( "DECAL" ) )
        {
            if( !SawNumDecals )
            {
                x_throw( "-DECAL requires -NUMDECALS first." );
            }
            if( (CurrDecal + 1) >= DecalPkg.GetNDecalDefs() )
            {
                x_throw( "More -DECAL options were supplied than declared." );
            }

            CurrDecal++;
            decal_definition& DecalDef = DecalPkg.GetDecalDef( CurrDecal );
            const xstring Name = CommandLine.GetOptionString( i );
            RequireTextLength( Name, 31, "Decal definition name" );
            x_strsavecpy( DecalDef.m_Name, Name, 32 );
        }
        else if( OptName == xstring( "MINWIDTH" ) )
        {
            RequireDecal( CurrDecal, "MINWIDTH" );
            DecalPkg.GetDecalDef( CurrDecal ).m_MinSize.X = ParseF32(
                CommandLine.GetOptionString( i ), "minimum decal width" );
        }
        else if( OptName == xstring( "MINHEIGHT" ) )
        {
            RequireDecal( CurrDecal, "MINHEIGHT" );
            DecalPkg.GetDecalDef( CurrDecal ).m_MinSize.Y = ParseF32(
                CommandLine.GetOptionString( i ), "minimum decal height" );
        }
        else if( OptName == xstring( "MAXWIDTH" ) )
        {
            RequireDecal( CurrDecal, "MAXWIDTH" );
            DecalPkg.GetDecalDef( CurrDecal ).m_MaxSize.X = ParseF32(
                CommandLine.GetOptionString( i ), "maximum decal width" );
        }
        else if( OptName == xstring( "MAXHEIGHT" ) )
        {
            RequireDecal( CurrDecal, "MAXHEIGHT" );
            DecalPkg.GetDecalDef( CurrDecal ).m_MaxSize.Y = ParseF32(
                CommandLine.GetOptionString( i ), "maximum decal height" );
        }
        else if( OptName == xstring( "MINROLL" ) )
        {
            RequireDecal( CurrDecal, "MINROLL" );
            DecalPkg.GetDecalDef( CurrDecal ).m_MinRoll = ParseF32(
                CommandLine.GetOptionString( i ), "minimum decal roll" );
        }
        else if( OptName == xstring( "MAXROLL" ) )
        {
            RequireDecal( CurrDecal, "MAXROLL" );
            DecalPkg.GetDecalDef( CurrDecal ).m_MaxRoll = ParseF32(
                CommandLine.GetOptionString( i ), "maximum decal roll" );
        }
        else if( OptName == xstring( "COLOR" ) )
        {
            RequireDecal( CurrDecal, "COLOR" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Color = ParseU32(
                CommandLine.GetOptionString( i ), 16, "decal color" );
        }
        else if( OptName == xstring( "MAXVIS" ) )
        {
            RequireDecal( CurrDecal, "MAXVIS" );
            DecalPkg.GetDecalDef( CurrDecal ).m_MaxVisible = ParseU32(
                CommandLine.GetOptionString( i ), 10, "maximum visible decals" );
        }
        else if( OptName == xstring( "BITMAP" ) )
        {
            RequireDecal( CurrDecal, "BITMAP" );
            const xstring BitmapName = CommandLine.GetOptionString( i );
            RequireTextLength( BitmapName,
                               X_MAX_PATH - 1,
                               "Decal source bitmap path" );
            SourceBitmapNames[CurrDecal] = BitmapName;
        }
        else if( OptName == xstring( "P8" ) )
        {
            RequireDecal( CurrDecal, "P8" );
            PreferredBitmapFormats[CurrDecal] |= BITMAP_FORMAT_8BIT;
        }
        else if( OptName == xstring( "P4" ) )
        {
            RequireDecal( CurrDecal, "P4" );
            PreferredBitmapFormats[CurrDecal] &= ~BITMAP_FORMAT_8BIT;
        }
        else if( OptName == xstring( "USE_TRI" ) )
        {
            RequireDecal( CurrDecal, "USE_TRI" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Flags |=
                decal_definition::DECAL_FLAG_USE_TRI;
        }
        else if( OptName == xstring( "NO_CLIP" ) )
        {
            RequireDecal( CurrDecal, "NO_CLIP" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Flags |=
                decal_definition::DECAL_FLAG_NO_CLIP;
        }
        else if( OptName == xstring( "USE_PROJECTION" ) )
        {
            RequireDecal( CurrDecal, "USE_PROJECTION" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Flags |=
                decal_definition::DECAL_FLAG_USE_PROJECTION;
        }
        else if( OptName == xstring( "KEEP_SIZE_RATIO" ) )
        {
            RequireDecal( CurrDecal, "KEEP_SIZE_RATIO" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Flags |=
                decal_definition::DECAL_FLAG_KEEP_SIZE_RATIO;
        }
        else if( OptName == xstring( "PERMANENT" ) )
        {
            RequireDecal( CurrDecal, "PERMANENT" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Flags |=
                decal_definition::DECAL_FLAG_PERMANENT;
        }
        else if( OptName == xstring( "FADE_OUT" ) )
        {
            RequireDecal( CurrDecal, "FADE_OUT" );
            decal_definition& DecalDef = DecalPkg.GetDecalDef( CurrDecal );
            DecalDef.m_Flags |= decal_definition::DECAL_FLAG_FADE_OUT;
            DecalDef.m_FadeTime = ParseF32(
                CommandLine.GetOptionString( i ), "decal fade time" );
        }
        else if( OptName == xstring( "ADD_GLOW" ) )
        {
            RequireDecal( CurrDecal, "ADD_GLOW" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Flags |=
                decal_definition::DECAL_FLAG_ADD_GLOW;
        }
        else if( OptName == xstring( "ENV_MAPPED" ) )
        {
            RequireDecal( CurrDecal, "ENV_MAPPED" );
            DecalPkg.GetDecalDef( CurrDecal ).m_Flags |=
                decal_definition::DECAL_FLAG_ENV_MAPPED;
        }
        else if( OptName == xstring( "BLEND_NORMAL" ) )
        {
            RequireDecal( CurrDecal, "BLEND_NORMAL" );
            DecalPkg.GetDecalDef( CurrDecal ).m_BlendMode =
                decal_definition::DECAL_BLEND_NORMAL;
            PreferredBitmapFormats[CurrDecal] &= ~BITMAP_FORMAT_INTENSITY;
        }
        else if( OptName == xstring( "BLEND_ADD" ) )
        {
            RequireDecal( CurrDecal, "BLEND_ADD" );
            DecalPkg.GetDecalDef( CurrDecal ).m_BlendMode =
                decal_definition::DECAL_BLEND_ADD;
            PreferredBitmapFormats[CurrDecal] &= ~BITMAP_FORMAT_INTENSITY;
        }
        else if( OptName == xstring( "BLEND_SUBTRACT" ) )
        {
            RequireDecal( CurrDecal, "BLEND_SUBTRACT" );
            DecalPkg.GetDecalDef( CurrDecal ).m_BlendMode =
                decal_definition::DECAL_BLEND_SUBTRACT;
            PreferredBitmapFormats[CurrDecal] &= ~BITMAP_FORMAT_INTENSITY;
        }
        else if( OptName == xstring( "BLEND_INTENSITY" ) )
        {
            RequireDecal( CurrDecal, "BLEND_INTENSITY" );
            DecalPkg.GetDecalDef( CurrDecal ).m_BlendMode =
                decal_definition::DECAL_BLEND_INTENSITY;
            PreferredBitmapFormats[CurrDecal] |= BITMAP_FORMAT_INTENSITY;
        }
        else if( (OptName == xstring( "PC" )) ||
                 (OptName == xstring( "PS2" )) ||
                 (OptName == xstring( "XBOX" )) )
        {
            platform_output Output;
            Output.FileName = CommandLine.GetOptionString( i );
            if( Output.FileName.IsEmpty() )
            {
                x_throw( "Platform output filename is empty." );
            }
            RequireTextLength( Output.FileName,
                               X_MAX_PATH - 1,
                               "Decal package output path" );

            Output.Platform = EXPORT_DESKTOP;
            if( OptName == xstring( "PS2" ) )
            {
                Output.Platform = EXPORT_PS2;
            }
            else if( OptName == xstring( "XBOX" ) )
            {
                Output.Platform = EXPORT_XBOX;
            }

            for( s32 j = 0; j < Outputs.GetCount(); j++ )
            {
                if( !x_stricmp( Outputs[j].FileName, Output.FileName ) )
                {
                    x_throw( "A decal package output filename was specified more than once." );
                }
            }
            Outputs.Append( Output );
        }
    }

    if( !SawNumGroups )
    {
        x_throw( "-NUMGROUPS was not specified." );
    }
    if( !SawNumDecals )
    {
        x_throw( "-NUMDECALS was not specified." );
    }
    if( (CurrGroup + 1) != DecalPkg.GetNGroups() )
    {
        x_throw( "The number of -GROUP options does not match -NUMGROUPS." );
    }
    if( (CurrDecal + 1) != DecalPkg.GetNDecalDefs() )
    {
        x_throw( "The number of -DECAL options does not match -NUMDECALS." );
    }
    if( Outputs.GetCount() == 0 )
    {
        x_throw( "No platform output was specified." );
    }

    const s32 DefinitionCount = DecalPkg.GetNDecalDefs();
    for( s32 i = 0; i < DecalPkg.GetNGroups(); i++ )
    {
        if( (GroupStarts[i] > DefinitionCount) ||
            (GroupCounts[i] > (DefinitionCount - GroupStarts[i])) )
        {
            x_throw( xfs( "Decal group %d has an invalid definition range.", i ) );
        }
        DecalPkg.SetGroupDecalDefStart( i, GroupStarts[i] );
        DecalPkg.SetGroupDecalDefCount( i, GroupCounts[i] );
    }

    for( s32 i = 0; i < DefinitionCount; i++ )
    {
        if( SourceBitmapNames[i].IsEmpty() )
        {
            x_throw( xfs( "Decal definition %d has no -BITMAP option.", i ) );
        }
    }

    xstring Error;
    if( !decal_package_file::Validate( DecalPkg, Error ) )
    {
        x_throw( (const char*)Error );
    }

    for( s32 i = 0; i < Outputs.GetCount(); i++ )
    {
        const platform_output& Output = Outputs[i];
        s_Platform = Output.Platform;
        CompileBitmaps( Output.FileName,
                        DecalPkg,
                        SourceBitmapNames,
                        PreferredBitmapFormats );

        if( !decal_package_file::Validate( DecalPkg, Error ) )
        {
            x_throw( (const char*)Error );
        }
        SaveDecalPackage( Output.FileName, DecalPkg );

        if( s_Verbose )
        {
            x_printf( "Wrote verified Bitsery decal package: %s\n",
                      (const char*)Output.FileName );
        }
    }
}

//=========================================================================

void PrintHelp( void )
{
    x_printf( "Error: Compiling\n" );
    x_printf( "-LOG                     Verbose mode                        \n" );
    x_printf( "-XBOX <filename>         Output XBox decal                   \n" );
    x_printf( "-PS2 <filename>          Output PS2 decal                    \n" );
    x_printf( "-PC <filename>           Output PC decal                     \n" );
    x_printf( "-NUMGROUPS <int>         Number of groups to expect          \n" );
    x_printf( "-NUMDECALS <int>         Number of decals to expect          \n" );
    x_printf( "-GROUP <string>          Start of a group named <string>     \n" );
    x_printf( "-GROUPCOLOR <hex>        Color of current group              \n" );
    x_printf( "-DECALSTART <int>        Start index of this group's decals  \n" );
    x_printf( "-DECALCOUNT <int>        Number of decals in this group      \n" );
    x_printf( "-DECAL <string>          Start of a decal named <string>     \n" );
    x_printf( "-MINWIDTH <float>        Min Width of decal                  \n" );
    x_printf( "-MINHEIGHT <float>       Min Height of decal                 \n" );
    x_printf( "-MAXWIDTH <float>        Max Width of decal                  \n" );
    x_printf( "-MAXHEIGHT <float>       Max Height of decal                 \n" );
    x_printf( "-MINROLL <float>         Min Roll of decal (in radians)      \n" );
    x_printf( "-MAXROLL <float>         Max Roll of decal (in radians)      \n" );
    x_printf( "-COLOR <hex>             Color of decal                      \n" );
    x_printf( "-MAXVIS <decimal>        Max # of decals visible             \n" );
    x_printf( "-BITMAP <filename>       Bitmap to use (TGA)                 \n" );
    x_printf( "-P8                      Compile bitmap as 8-bit             \n" );
    x_printf( "-P4                      Compile bitmap as 4-bit             \n" );
    x_printf( "-USE_TRI                 Decal can use triangle primitive    \n" );
    x_printf( "-NO_CLIP                 Don't clip decal to polys           \n" );
    x_printf( "-USE_PROJECTION          Use projection mapping to stretch   \n" );
    x_printf( "-KEEP_SIZE_RATIO         Maintain width/height ratio         \n" );
    x_printf( "-PERMANENT               Decal never fades out or disappears \n" );
    x_printf( "-FADE_OUT <float>        Fade decal out over this duration   \n" );
    x_printf( "-ADD_GLOW                Add a bloom effect around decal     \n" );
    x_printf( "-ENV_MAPPED              Environment-map the decal           \n" );
    x_printf( "-BLEND_NORMAL            Use normal blending                 \n" );
    x_printf( "-BLEND_ADD               Use additive blending               \n" );
    x_printf( "-BLEND_SUBTRACT          Use subtractive blending            \n" );
    x_printf( "-BLEND_INTENSITY         Use intensity mode blending         \n" );
}

//=========================================================================

int main( int argc, char* argv[] )
{
    xbool Success = FALSE;

    x_Init( argc,argv );

    x_try;

    // save out the exe timestamp for doing dependancy checks
    xstring ExePath(argv[0]);
    sptr ExeFindHandle;
    s_ExeData.time_write = 0;
    ExeFindHandle = _findfirst( ExePath, &s_ExeData );
    if ( ExeFindHandle != -1 )
        _findclose( ExeFindHandle );

    command_line CommandLine;
    
    // Specify all the options
    CommandLine.AddOptionDef( "LOG" );
    CommandLine.AddOptionDef( "XBOX",       command_line::STRING );
    CommandLine.AddOptionDef( "PS2",        command_line::STRING );
    CommandLine.AddOptionDef( "PC",         command_line::STRING );
    CommandLine.AddOptionDef( "NUMGROUPS",  command_line::STRING );
    CommandLine.AddOptionDef( "NUMDECALS",  command_line::STRING );
    CommandLine.AddOptionDef( "GROUP",      command_line::STRING );
    CommandLine.AddOptionDef( "GROUPCOLOR", command_line::STRING );
    CommandLine.AddOptionDef( "DECALSTART", command_line::STRING );
    CommandLine.AddOptionDef( "DECALCOUNT", command_line::STRING );
    CommandLine.AddOptionDef( "DECAL",      command_line::STRING );
    CommandLine.AddOptionDef( "MINWIDTH",   command_line::STRING );
    CommandLine.AddOptionDef( "MINHEIGHT",  command_line::STRING );
    CommandLine.AddOptionDef( "MAXWIDTH",   command_line::STRING );
    CommandLine.AddOptionDef( "MAXHEIGHT",  command_line::STRING );
    CommandLine.AddOptionDef( "MINROLL",    command_line::STRING );
    CommandLine.AddOptionDef( "MAXROLL",    command_line::STRING );
    CommandLine.AddOptionDef( "COLOR",      command_line::STRING );
    CommandLine.AddOptionDef( "MAXVIS",     command_line::STRING );
    CommandLine.AddOptionDef( "BITMAP",     command_line::STRING );
    CommandLine.AddOptionDef( "P8" );
    CommandLine.AddOptionDef( "P4" );
    CommandLine.AddOptionDef( "USE_TRI" );
    CommandLine.AddOptionDef( "NO_CLIP" );
    CommandLine.AddOptionDef( "USE_PROJECTION" );
    CommandLine.AddOptionDef( "KEEP_SIZE_RATIO" );
    CommandLine.AddOptionDef( "PERMANENT" );
    CommandLine.AddOptionDef( "FADE_OUT",   command_line::STRING );
    CommandLine.AddOptionDef( "ADD_GLOW" );
    CommandLine.AddOptionDef( "ENV_MAPPED" );
    CommandLine.AddOptionDef( "BLEND_NORMAL" );
    CommandLine.AddOptionDef( "BLEND_ADD" );
    CommandLine.AddOptionDef( "BLEND_SUBTRACT" );
    CommandLine.AddOptionDef( "BLEND_INTENSITY" );

    // Parse the command line
    if( CommandLine.Parse( argc, argv ) )
    {
        PrintHelp();
    }
    else
    {
        // Do the script
        ExecuteScript( CommandLine );
        Success = TRUE;
    }
    
    x_catch_begin;
    #ifdef X_EXCEPTIONS
        x_printf( "Error: %s\n", xExceptionGetErrorString() );
    #endif
        Success = FALSE;
    x_catch_end;

    x_Kill();
    return( Success ? 0 : 1 );
}
