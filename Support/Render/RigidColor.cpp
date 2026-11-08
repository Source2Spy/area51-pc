//=========================================================================
//
//  RigidColor.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RigidColor.hpp"
#include "Auxiliary/MiscUtils/BitseryIO.hpp"

//=========================================================================
//  LOCAL CONSTANTS
//=========================================================================

enum
{
    RIGID_COLOR_FILE_VERSION = 1,
    MAX_RIGID_COLOR_COUNT = 16 * 1024 * 1024,
};

//-------------------------------------------------------------------------

bitsery_io::file_format const RIGID_COLOR_FILE_FORMAT = {
    { 'R', 'C', 'L', 'R' },
    RIGID_COLOR_FILE_VERSION,
};

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
xbool Validate( RigidColorData const& data, xstring& error )
{
    if ( data.Colors.GetCount() > MAX_RIGID_COLOR_COUNT )
    {
        return ( bitsery_io::Fail( error, "Rigid color table exceeds the supported color count." ) );
    }

    return ( TRUE );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void serialize( bitsery::Serializer<bitsery_io::output_adapter>& serializer, RigidColorData& data )
{
    u32 count = static_cast<u32>( data.Colors.GetCount() );
    serializer.value4b( count );

    for ( s32 i = 0; i < data.Colors.GetCount(); i++ )
    {
        serializer.value4b( data.Colors[i] );
    }
}

//=========================================================================

void serialize( bitsery::Deserializer<bitsery_io::input_adapter>& serializer, RigidColorData& data )
{
    u32 count = 0;
    serializer.value4b( count );

    if ( count > MAX_RIGID_COLOR_COUNT )
    {
        bitsery_io::SetInvalidData( serializer );
        data.Colors.Clear();
        return;
    }

    data.Colors.SetCount( static_cast<s32>( count ) );
    for ( s32 i = 0; i < data.Colors.GetCount(); i++ )
    {
        serializer.value4b( data.Colors[i] );
    }
}

//=========================================================================

xbool rigid_color_file::Load( X_FILE* pFile, RigidColorData& data, xstring& error )
{
    data.Colors.Clear();

    if ( !bitsery_io::Read( pFile, RIGID_COLOR_FILE_FORMAT, data, error ) )
    {
        data.Colors.Clear();
        return ( FALSE );
    }

    if ( !Validate( data, error ) )
    {
        data.Colors.Clear();
        return ( FALSE );
    }

    return ( TRUE );
}

//=========================================================================

xbool rigid_color_file::Save( X_FILE* pFile, RigidColorData const& data, xstring& error )
{
    if ( !Validate( data, error ) )
    {
        return ( FALSE );
    }

    return ( bitsery_io::Write( pFile, RIGID_COLOR_FILE_FORMAT, data, error ) );
}

//=========================================================================

xbool rigid_color_file::Save( char const* pFileName, RigidColorData const& data, xstring& error )
{
    if ( !pFileName || !pFileName[0] )
    {
        return ( bitsery_io::Fail( error, "Rigid color output filename is empty." ) );
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if ( !pFile )
    {
        return ( bitsery_io::Fail( error, "Failed to open the rigid color output file." ) );
    }

    xbool const result = Save( pFile, data, error );
    x_fclose( pFile );
    return ( result );
}