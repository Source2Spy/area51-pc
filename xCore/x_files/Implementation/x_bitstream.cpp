//=========================================================================
//
//  X_BITSTREAM.CPP
//
//=========================================================================

#include "../x_bitstream.hpp"
#include "../x_debug.hpp"
#include "../x_plus.hpp"
#include "../x_color.hpp"
#include "../x_string.hpp"
#include "../x_files.hpp"

//=========================================================================

#define TO_U32(x)       (*((u32*)(&(x))))
#define TO_F32(x)       (*((f32*)(&(x))))
#define LOWER_BITS(x)   ((u32)((1ULL << (x)) - 1ULL))

static s32 s_VarLenBitOptions[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,32};

//=========================================================================

inline s32 GetHighestBit( u32 N )
{
    s32 B=0;
    while( N ) { B++; N>>=1; }
    return B;
}

//=========================================================================

bitstream::bitstream( void )
{
    m_Data              = NULL;
    m_DataSize          = 0;
    m_DataSizeInBits    = m_DataSize<<3;
    m_HighestBitWritten = -1;
    m_Cursor            = 0;
    m_bOwnsData         = TRUE;
    m_MaxGrowSize       = 1024;
    m_SectionCursor     = -1;
    m_bOverwrite        = FALSE;
    m_bReadError        = FALSE;
    m_bReadFiniteFloats = FALSE;
    m_WriteLimit = m_DataSizeInBits;
    m_SectionWriteLimit = -1;
}

//=========================================================================

bitstream::~bitstream( void )
{
    Kill();
}

//=========================================================================

void bitstream::Kill( void )
{
    if( m_bOwnsData )
        x_free( m_Data );
    m_Data = NULL;
    m_DataSize = 0;
    m_DataSizeInBits = 0;
    m_HighestBitWritten = -1;
    m_Cursor = 0;
    m_bOwnsData = FALSE;
    m_bOverwrite = FALSE;
    m_bReadError = FALSE;
    m_bReadFiniteFloats = FALSE;
    m_WriteLimit = m_DataSizeInBits;
    m_SectionWriteLimit = -1;
}

//=========================================================================

void    bitstream::Init( s32 DataSize )
{
    s32 Size = MAX(DataSize, m_DataSize);
    if( Size > m_DataSize )
    {
        ASSERT( (m_Data == NULL) || m_bOwnsData );
        m_Data           = (byte*)x_realloc(m_Data, Size);
        ASSERT( m_Data );
        m_bOwnsData      = TRUE;
        m_DataSize       = Size;
        m_DataSizeInBits = m_DataSize<<3;
    }
    m_HighestBitWritten  = -1;
    m_Cursor             = 0;
    m_SectionCursor      = -1;
    m_bOverwrite         = FALSE;
    m_bReadError         = FALSE;
    m_bReadFiniteFloats  = FALSE;
    m_WriteLimit = m_DataSizeInBits;
    m_SectionWriteLimit = -1;
}

//=========================================================================

void    bitstream::Init( const byte* pData, s32 DataSize )
{
    m_bOwnsData         = FALSE;
    m_Data              = (byte*)pData;
    m_DataSize          = DataSize;
    m_DataSizeInBits    = m_DataSize<<3;
    m_HighestBitWritten = -1;
    m_Cursor            =  0;
    m_SectionCursor     = -1;
    m_bOverwrite        = FALSE;
    m_bReadError        = FALSE;
    m_bReadFiniteFloats = FALSE;
    m_WriteLimit = m_DataSizeInBits;
    m_SectionWriteLimit = -1;
}

//=========================================================================

void bitstream::SetOwnsData( xbool OwnsData )
{
    m_bOwnsData = OwnsData;
}

//=========================================================================

void bitstream::Grow ( void )
{
    ASSERT( m_bOwnsData );

    xbool FullLimit = (m_WriteLimit == m_DataSizeInBits);
    xbool FullSectionLimit = (m_SectionWriteLimit == m_DataSizeInBits);

    s32 GrowSize        = 16 + (m_DataSize/2);
    GrowSize            = MIN( GrowSize, m_MaxGrowSize );

    m_DataSize         += GrowSize;
    m_DataSizeInBits    = m_DataSize<<3;
    m_Data              = (byte *)x_realloc(m_Data, m_DataSize);
    if( FullLimit )
        m_WriteLimit = m_DataSizeInBits;
    if( FullSectionLimit )
        m_SectionWriteLimit = m_DataSizeInBits;

    ASSERT( m_Data );
}

//=========================================================================

void bitstream::SetMaxGrowSize( s32 MaxGrowSize )
{
    m_MaxGrowSize       = MaxGrowSize;
}

//=========================================================================

xbool     bitstream::Overwrite       ( void ) const
{
    return m_bOverwrite || m_bReadError;
}

//=========================================================================

void     bitstream::ClearOverwrite   ( void )
{
    m_bOverwrite        = FALSE;
}

//=========================================================================

s32     bitstream::GetNBytes       ( void ) const
{
    return m_DataSize;
}

//=========================================================================

s32     bitstream::GetNBits        ( void ) const
{
    return m_DataSizeInBits;
}

//=========================================================================

s32     bitstream::GetNBytesUsed   ( void ) const
{
    s32 NBitsUsed       = (m_HighestBitWritten+1);
    return (NBitsUsed + 7)>>3;
}

//=========================================================================

s32     bitstream::GetNBytesFree   ( void ) const
{
    return MAX(0, (m_WriteLimit - GetNBitsUsed()) / 8);
}

//=========================================================================

s32     bitstream::GetNBitsUsed    ( void ) const
{
    return (m_HighestBitWritten+1);
}

//=========================================================================

s32     bitstream::GetNBitsFree    ( void ) const
{
    return MAX(0, m_WriteLimit - GetNBitsUsed());
}

//=========================================================================

byte*   bitstream::GetDataPtr      ( void ) const
{
    return m_Data;
}

//=========================================================================

xbool   bitstream::IsFull          ( void ) const
{
    return (m_Cursor>=m_WriteLimit);
}

//=========================================================================

s32     bitstream::GetCursor       ( void ) const
{
    return m_Cursor;
}

//=========================================================================

s32     bitstream::GetCursorRemaining( void ) const
{
    return m_DataSizeInBits - m_Cursor;
}

//=========================================================================

void    bitstream::SetCursor       ( s32 BitIndex )
{
    ASSERT( BitIndex>=0 );
    m_Cursor = BitIndex;
    m_bOverwrite = (m_Cursor > m_WriteLimit);
}

//=========================================================================

void    bitstream::WriteU64        ( u64 Value, s32 NBits )
{
    u32 V0     = (u32)((Value>> 0)&0xFFFFFFFF);
    u32 V1     = (u32)((Value>>32)&0xFFFFFFFF);
    s32 NBits0 = MIN(NBits,32);
    s32 NBits1 = NBits - NBits0;

    WriteRaw32( V0, NBits0);

    if( NBits1 > 0 )
        WriteRaw32( V1, NBits1);
}

//=========================================================================

void    bitstream::ReadMarker  ( void ) const
{
#if defined(X_DEBUG)
    u32 R;
    ReadU32( R );
    if( R != 0xDEADBEEF )
        SetReadError();
#endif
}

//=========================================================================

xbool   bitstream::OpenSection     ( xbool Flag, s32 ReserveBits )
{
    ASSERT( m_SectionCursor == -1 );
    ASSERT( ReserveBits >= 0 );

    if( m_Cursor >= m_WriteLimit )
    {
        if( m_bOwnsData && (m_WriteLimit == m_DataSizeInBits) )
        {
            Grow();
        }
        else
        {
            m_bOverwrite = TRUE;
            return FALSE;
        }
    }

    if( Flag && (ReserveBits >= m_WriteLimit - m_Cursor) )
        Flag = FALSE;

    if( Flag )
    {
        m_SectionCursor = GetCursor();
        m_SectionWriteLimit = m_WriteLimit;
        m_WriteLimit -= ReserveBits;
    }

    WriteFlag( Flag );
    return Flag;
}

//=========================================================================

xbool   bitstream::CloseSection    ( void )
{
    ASSERT( m_SectionCursor >= 0 );

    m_WriteLimit = m_SectionWriteLimit;
    m_SectionWriteLimit = -1;

    if( Overwrite() )
    {
        SetCursor( m_SectionCursor );
        ClearOverwrite();
        // If we overwrite the end of the bitstream buffer, we rewind
        // to the start of the section and write a flag now stating
        // that the section does not exist.
        WriteFlag( FALSE );
        Truncate();
        m_SectionCursor = -1;
        return( FALSE );
    }

    m_SectionCursor = -1;
    return( TRUE );
}

//=========================================================================

void    bitstream::WriteRangedS32  ( s32 Value, s32 Min, s32 Max )
{
    ASSERT( ( *(u32*)&Value ) != 0xFEEDC0DE );
    ASSERT( Max > Min );
    ASSERT( Value >= Min );
    ASSERT( Value <= Max );
    
    u32 Range = (u32)Max - (u32)Min;
    s32 NBits = GetHighestBit(Range);
    WriteU32( (u32)Value - (u32)Min, NBits );
}

//=========================================================================

void    bitstream::WriteRangedU32  ( u32 Value, s32 Min, s32 Max )
{
    ASSERT( Value != 0xFEEDC0DE );
    ASSERT( Max > Min );
    ASSERT( Value >= (u32)Min );
    ASSERT( Value <= (u32)Max );
    
    u32 Range = (u32)Max - (u32)Min;
    s32 NBits = GetHighestBit(Range);
    WriteU32( (u32)Value - (u32)Min, NBits );
}

//=========================================================================

void    bitstream::WriteVariableLenS32 ( s32 Value )
{
    ASSERT( ( *(u32*)&Value ) != 0xFEEDC0DE );

    u32 Magnitude = (u32)Value;
    if( WriteFlag( Value < 0 ) )
        Magnitude = 0u - Magnitude;
    WriteVariableLenU32( Magnitude );
}

//=========================================================================

void    bitstream::WriteVariableLenU32 ( u32 Value )
{
    ASSERT( Value != 0xFEEDC0DE );

    s32 i;

    s32 nBits = GetHighestBit( Value );
    for( i=0; i<16; i++ )
        if( nBits <= s_VarLenBitOptions[i] ) break;

    WriteU32( i, 4 );
    WriteU32( Value, s_VarLenBitOptions[i] );
}

//=========================================================================

void    bitstream::ReadU64( u64& Value, s32 NBits ) const
{
    if( (NBits < 0) || (NBits > 64) )
    {
        SetReadError();
        Value = 0;
        return;
    }

    s32 NBits0 = MIN(NBits,32);
    s32 NBits1 = NBits - NBits0;
    u32 V0;
    u32 V1=0;

    V0 = ReadRaw32( NBits0 );

    if( NBits1 > 0 )
        V1 = ReadRaw32( NBits1 );

    Value = (((u64)V1)<<32) | V0;
}

//=========================================================================

void    bitstream::ReadRangedS32   ( s32& Value, s32 Min, s32 Max ) const
{
    ASSERT( Max > Min );
    u32 Range = (u32)Max - (u32)Min;
    u32 V = ReadRaw32( GetHighestBit(Range) );
    if( ReadError() || (V > Range) )
    {
        SetReadError();
        Value = Min;
        return;
    }
    Value = (s32)((s64)Min + V);
}

//=========================================================================

void    bitstream::ReadRangedU32   ( u32& Value, s32 Min, s32 Max ) const
{
    ASSERT( Max > Min );
    u32 Range = (u32)Max - (u32)Min;
    u32 V = ReadRaw32( GetHighestBit(Range) );
    if( ReadError() || (V > Range) )
    {
        SetReadError();
        Value = (u32)Min;
        return;
    }
    Value = V + (u32)Min;
}

//=========================================================================

void    bitstream::WriteF32        ( f32 Value )
{
    ASSERT( ( *(u32*)&Value ) != 0xFEEDC0DE );
    
    WriteRaw32( TO_U32(Value), 32 );
}

//=========================================================================

void    bitstream::WriteRangedF32  ( f32 Value, s32 NBits, f32 Min, f32 Max )
{
    ASSERT( ( *(u32*)&Value ) != 0xFEEDC0DE );
    ASSERT( Max > Min );
    ASSERT( Value >= Min );
    ASSERT( Value <= Max );

    if( NBits==32 )
    {
        WriteF32( Value );
        return;
    }

    Value -= Min;
    f32 Range = Max-Min;
    s32 Scalar = LOWER_BITS(NBits);
    u32 N = (u32)(((Value/Range) * (f32)Scalar) + 0.5f);
    ASSERT( N <= (u32)Scalar );

    WriteU32( N, NBits );
}

//=========================================================================

void    bitstream::ReadVariableLenU32 ( u32& Value ) const
{
    u32 nBitsOption;
    u32 V;

    ReadU32( nBitsOption, 4 );
    ReadU32( V, s_VarLenBitOptions[nBitsOption] );

    Value = (s32)V;
}

//=========================================================================

void    bitstream::ReadVariableLenS32 ( s32& Value ) const
{
    xbool Negative = ReadFlag();
    u32 Magnitude;
    ReadVariableLenU32( Magnitude );
    if( ReadError() || (Magnitude > (Negative ? 0x80000000u : 0x7FFFFFFFu)) )
    {
        SetReadError();
        Value = 0;
        return;
    }
    Value = (s32)(Negative ? -(s64)Magnitude : (s64)Magnitude);
}

//=========================================================================

void bitstream::WriteVariableLenF32( f32 Value )
{
    ASSERT( ( *(u32*)&Value ) != 0xFEEDC0DE );

    // Check if it can be zero?
    if( Value == 0.0f )
    {
        WriteFlag(1);
        WriteFlag(1);
        return;
    }

    // Check if it can be an int?
    if( Value == (f32)((s32)Value) )
    {
        WriteFlag(1);
        WriteFlag(0);
        WriteVariableLenS32((s32)Value);
        return;
    }

    // No shortcut found so write full precision
    WriteFlag(0);
    WriteF32(Value);
}

//=========================================================================

void    bitstream::ReadF32         ( f32& Value ) const
{
    u32 UValue = ReadRaw32( 32 );
    if( m_bReadFiniteFloats && ((UValue & 0x7F800000u) == 0x7F800000u) )
    {
        SetReadError();
        Value = 0.0f;
        return;
    }
    Value = TO_F32(UValue);
}

//=========================================================================

void    bitstream::ReadRangedF32   ( f32& Value, s32 NBits, f32 Min, f32 Max ) const
{
    if( NBits==32 )
    {
        ReadF32( Value );
        if( !((Value >= Min) && (Value <= Max)) )
        {
            SetReadError();
            Value = Min;
        }
        return;
    }
    ASSERT( Max>Min );
    f32 Range = Max-Min;
    s32 Scalar = LOWER_BITS(NBits);
    u32 N;
    ReadU32( N, NBits );
    Value = (((f32)N)/Scalar)*Range + Min;
    ASSERT( (Value>=Min) && (Value<=Max) );
}

//=========================================================================

void bitstream::ReadVariableLenF32( f32& Value ) const
{
    if( ReadFlag() )
    {
        // Check if value is zero
        if( ReadFlag() )
        {
            Value = 0.0f;
            return;
        }

        // Value is an int
        s32 iValue;
        ReadVariableLenS32(iValue);
        Value = (f32)iValue;
        return;
    }

    // No shortcut so just read full precision
    ReadF32(Value);
}

//=========================================================================

void bitstream::TruncateRangedF32( f32& Value, s32 NBits, f32 Min, f32 Max )
{
    if( NBits==32 ) 
        return;

    if( IN_RANGE( Min, Value, Max ) )
    {
        ASSERT( (Max>Min) && IN_RANGE( Min, Value, Max ) );
        Value -= Min;
        f32 Range = Max-Min;
        s32 Scalar = LOWER_BITS(NBits);
        u32 N = (u32)(((Value/Range)*(f32)Scalar)+0.5f);
        Value = (((f32)N)/Scalar)*Range + Min;
        ASSERT( IN_RANGE( Min, Value, Max ) );
    }
}

//=========================================================================

void bitstream::TruncateRangedVector( vector3& N, s32 NBits, f32 Min, f32 Max )
{
    TruncateRangedF32(N.GetX(), NBits, Min, Max) ;
    TruncateRangedF32(N.GetY(), NBits, Min, Max) ;
    TruncateRangedF32(N.GetZ(), NBits, Min, Max) ;
}

//=========================================================================

void    bitstream::WriteColor      ( xcolor Color )
{
    ASSERT( ( *(u32*)&Color ) != 0xFEEDC0DE );

    WriteU32( *((u32*)&Color) );
}

//=========================================================================

void    bitstream::ReadColor       ( xcolor& Color ) const
{
    u32 C;
    ReadU32(C);
    Color = (xcolor)C;
}

//=========================================================================

void    bitstream::WriteQuaternion     ( const quaternion& Q )
{
    WriteF32( Q.X );
    WriteF32( Q.Y );
    WriteF32( Q.Z );
    WriteF32( Q.W );
}

//=========================================================================

void    bitstream::ReadQuaternion      ( quaternion& Q ) const
{
    ReadF32( Q.X );
    ReadF32( Q.Y );
    ReadF32( Q.Z );
    ReadF32( Q.W );
}

//=========================================================================

void    bitstream::WriteVector         ( const vector3& N )
{
    WriteF32( N.GetX() );
    WriteF32( N.GetY() );
    WriteF32( N.GetZ() );
}

//=========================================================================

void    bitstream::WriteRangedVector   ( const vector3& N, s32 NBits, f32 Min, f32 Max )
{
    WriteRangedF32( N.GetX(), NBits, Min, Max );
    WriteRangedF32( N.GetY(), NBits, Min, Max );
    WriteRangedF32( N.GetZ(), NBits, Min, Max );
}

//=========================================================================

void    bitstream::WriteVariableLenVector( const vector3& N )
{
    WriteVariableLenF32( N.GetX() );
    WriteVariableLenF32( N.GetY() );
    WriteVariableLenF32( N.GetZ() );
}

//=========================================================================


void    bitstream::WriteUnitVector     ( const vector3& N, s32 TotalBits )
{
#ifdef X_ASSERT
    u32* pI = (u32*)&N;
    ASSERT( pI[0] != 0xFEEDC0DE );
    ASSERT( pI[1] != 0xFEEDC0DE );
    ASSERT( pI[2] != 0xFEEDC0DE );

    f32 L = N.GetX()*N.GetX() + N.GetY()*N.GetY() + N.GetZ()*N.GetZ();
    ASSERT( (L>=0.99f) && (L<=1.001f) );
#endif

    radian P,Y;
    N.GetPitchYaw(P,Y);

    while( Y<0     ) Y += R_360;
    while( Y>R_360 ) Y -= R_360;
    while( P<0     ) P += R_360;
    while( P>R_360 ) P -= R_360;
    if( P>R_180 ) P = P - R_360;
    if( P<-R_90 ) P = -R_90;
    if( P>R_90 ) P = R_90;

    WriteRangedF32( Y, (TotalBits/2),  R_0,  R_360 );
    WriteRangedF32( P, (TotalBits/2), -R_90, R_90 );
}

//=========================================================================

void    bitstream::ReadVector          ( vector3& N ) const
{
    ReadF32(N.GetX());
    ReadF32(N.GetY());
    ReadF32(N.GetZ());
}

//=========================================================================

void    bitstream::ReadRangedVector    ( vector3& N, s32 NBits, f32 Min, f32 Max ) const
{
    ReadRangedF32( N.GetX(), NBits, Min, Max );
    ReadRangedF32( N.GetY(), NBits, Min, Max );
    ReadRangedF32( N.GetZ(), NBits, Min, Max );
}

//=========================================================================

void    bitstream::ReadVariableLenVector( vector3& N ) const
{
    ReadVariableLenF32( N.GetX() );
    ReadVariableLenF32( N.GetY() );
    ReadVariableLenF32( N.GetZ() );
}

//=========================================================================

void    bitstream::ReadUnitVector     ( vector3& N, s32 TotalBits ) const
{
    radian P,Y;
    ReadRangedF32( Y, (TotalBits/2),  R_0,  R_360 );
    ReadRangedF32( P, (TotalBits/2), -R_90, R_90 );
    N.Set( P, Y );
}

//=========================================================================

void    bitstream::WriteRadian3         ( const radian3& R )
{
    WriteF32( R.Pitch );
    WriteF32( R.Yaw );
    WriteF32( R.Roll );
}

//=========================================================================

void    bitstream::ReadRadian3          ( radian3& R ) const
{
    ReadF32(R.Pitch);
    ReadF32(R.Yaw);
    ReadF32(R.Roll);
}

//=========================================================================

void    bitstream::WriteRangedRadian3  ( const radian3& Radian, s32 NBits )
{
    ASSERT( ( *(u32*)&Radian.Pitch ) != 0xFEEDC0DE );
    ASSERT( ( *(u32*)&Radian.Yaw   ) != 0xFEEDC0DE );
    ASSERT( ( *(u32*)&Radian.Roll  ) != 0xFEEDC0DE );

    f32 X = x_fmod(Radian.Pitch,R_360)*(1/R_360)*0.5f + 0.5f;
    f32 Y = x_fmod(Radian.Yaw,R_360)*(1/R_360)*0.5f + 0.5f;
    f32 Z = x_fmod(Radian.Roll,R_360)*(1/R_360)*0.5f + 0.5f;

    f32 LB = (f32)LOWER_BITS(NBits);

    u32 NX = (u32)(X*LB);
    u32 NY = (u32)(Y*LB);
    u32 NZ = (u32)(Z*LB);

    ASSERT( NX<=LB );
    ASSERT( NY<=LB );
    ASSERT( NZ<=LB );

    WriteU32( NX, NBits );
    WriteU32( NY, NBits );
    WriteU32( NZ, NBits );
}

//=========================================================================

void    bitstream::ReadRangedRadian3   ( radian3& Radian, s32 NBits ) const
{
    u32 NX,NY,NZ;
    ReadU32( NX, NBits );
    ReadU32( NY, NBits );
    ReadU32( NZ, NBits );

    Radian.Pitch = (f32)((NX*(1/(f32)LOWER_BITS(NBits)))-0.5f)*2*R_360;
    Radian.Yaw   = (f32)((NY*(1/(f32)LOWER_BITS(NBits)))-0.5f)*2*R_360;
    Radian.Roll  = (f32)((NZ*(1/(f32)LOWER_BITS(NBits)))-0.5f)*2*R_360;
}

//=========================================================================

void    bitstream::WriteString         ( const char* pBuf )
{
#ifdef X_ASSERT
    u8* pI = (u8*)pBuf;
    ASSERT(     !(      ( pI[0] == 0xFE )
                    &&  ( pI[1] == 0xED )
                    &&  ( pI[2] == 0xC0 )
                    &&  ( pI[3] == 0xDE ) ) );
#endif                    

    // Get len
    const char* pC = pBuf;
    while( *pC++ ) {};
    
    s32 L = pC-pBuf;
    ASSERT( L < 256 );

    WriteU32( L, 8 );
    WriteRawBits( pBuf, L*8 );
}

//=========================================================================

xbool   bitstream::ReadString          ( char* pBuf, s32 MaxLength ) const
{
    if( !pBuf || (MaxLength <= 0) )
    {
        SetReadError();
        return FALSE;
    }
    pBuf[0] = 0;

    u32 Length;
    ReadU32( Length, 8 );
    if( !Length || (Length > (u32)MaxLength) || !CanRead(Length * 8) )
    {
        SetReadError();
        return FALSE;
    }
    ReadRawBits( pBuf, Length * 8 );
    for( u32 i = 0; i + 1 < Length; i++ )
    {
        if( pBuf[i] == 0 )
        {
            pBuf[0] = 0;
            SetReadError();
            return FALSE;
        }
    }

    if( pBuf[Length - 1] != 0 )
    {
        pBuf[0] = 0;
        SetReadError();
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

void    bitstream::WriteWString         ( const xwchar* pBuf )
{
#ifdef X_ASSERT
    u8* pI = (u8*)pBuf;
    ASSERT(     !(      ( pI[0] == 0xFE )
                    &&  ( pI[1] == 0xED )
                    &&  ( pI[2] == 0xC0 )
                    &&  ( pI[3] == 0xDE ) ) );
#endif                    

    // Get len
    const xwchar* pC = pBuf;
    while( *pC++ ) {};
    
    s32 L = pC-pBuf;
    ASSERT( L < 256 );

    WriteU32( L, 8 );
    WriteRawBits( pBuf, L*16 );
}

//=========================================================================

xbool   bitstream::ReadWString          ( xwchar* pBuf, s32 MaxLength ) const
{
    if( !pBuf || (MaxLength <= 0) )
    {
        SetReadError();
        return FALSE;
    }
    pBuf[0] = 0;

    u32 Length;
    ReadU32( Length, 8 );
    if( !Length || (Length > (u32)MaxLength) || !CanRead(Length * 16) )
    {
        SetReadError();
        return FALSE;
    }
    ReadRawBits( pBuf, Length * 16 );
    for( u32 i = 0; i + 1 < Length; i++ )
    {
        if( pBuf[i] == 0 )
        {
            pBuf[0] = 0;
            SetReadError();
            return FALSE;
        }
    }

    if( pBuf[Length - 1] != 0 )
    {
        pBuf[0] = 0;
        SetReadError();
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

void    bitstream::WriteMatrix4    ( const matrix4& M )
{
    f32* pF = (f32*)&M;
    for( s32 i=0; i<16; i++ )
        WriteF32( *pF++ );
}

//=========================================================================

void    bitstream::ReadMatrix4     ( matrix4& M ) const
{
    f32* pF = (f32*)&M;
    for( s32 i=0; i<16; i++ )
        ReadF32( *pF++ );
}

//=========================================================================

void    bitstream::ReadBits        ( void* pData, s32 NBits ) const
{
    ReadRawBits( pData, NBits );
}

//=========================================================================

void    bitstream::WriteBits        ( const void* pData, s32 NBits )
{
    WriteRawBits( pData, NBits );
}

//=========================================================================
//=========================================================================
//=========================================================================
// THESE ROUTINES ACTUALLY READ/WRITE DATA
//=========================================================================
//=========================================================================
//=========================================================================


//=========================================================================

void    bitstream::Clear( void )
{
    x_memset( m_Data, 0, m_DataSize );
    m_Cursor = 0;
    m_HighestBitWritten = -1;
    m_SectionCursor = -1;
    m_bOverwrite = FALSE;
    m_bReadError = FALSE;
    m_bReadFiniteFloats = FALSE;
    m_WriteLimit = m_DataSizeInBits;
    m_SectionWriteLimit = -1;
}

//=========================================================================

void bitstream::Truncate( void )
{
    ASSERT( (m_Cursor >= 0) && (m_Cursor <= m_DataSizeInBits) );
    m_HighestBitWritten = m_Cursor - 1;
}

//=========================================================================

s32 bitstream::GetWriteLimit( void ) const
{
    return m_WriteLimit;
}

//=========================================================================

void bitstream::SetWriteLimit( s32 NBits )
{
    ASSERT( (NBits >= 0) && (NBits <= m_DataSizeInBits) );
    m_WriteLimit = NBits;
}

//=========================================================================

void    bitstream::AlignCursor( s32 PowerOfTwo )
{
    // Move cursor to alignment requested
    m_Cursor = (m_Cursor + ((1<<PowerOfTwo)-1)) & (-(1<<PowerOfTwo));
}

//=========================================================================

xbool   bitstream::WriteFlag       ( xbool Value )
{
    ASSERT( ( *(u32*)&Value ) != 0xFEEDC0DE );

    if( m_Cursor >= m_WriteLimit )
    {
        if( m_bOwnsData && (m_WriteLimit == m_DataSizeInBits) ) Grow();
        else
        {
            m_bOverwrite = TRUE;
            return( Value );
        }
    }

    if( Value )
        *(m_Data + (m_Cursor>>3)) |=  (1<<(7-(m_Cursor & 0x07)));
    else
        *(m_Data + (m_Cursor>>3)) &= ~(1<<(7-(m_Cursor & 0x07)));
    
    m_Cursor++;
    m_HighestBitWritten = MAX( m_Cursor-1, m_HighestBitWritten );

    return( Value );
}

//=========================================================================

xbool bitstream::ReadStream( bitstream& Stream, s32 NBits ) const
{
    if( !CanRead(NBits) || (&Stream == this) )
    {
        SetReadError();
        return FALSE;
    }
    s32 Offset = m_Cursor & 7;
    Stream.Kill();
    Stream.Init( m_Data ? m_Data + (m_Cursor >> 3) : NULL, (Offset + NBits + 7) >> 3 );
    Stream.m_Cursor = Offset;
    Stream.m_DataSizeInBits = Offset + NBits;
    Stream.m_WriteLimit = Stream.m_DataSizeInBits;
    Stream.m_bReadFiniteFloats = m_bReadFiniteFloats;
    m_Cursor += NBits;
    return TRUE;
}

//=========================================================================

void bitstream::RequireFiniteFloats( void ) const
{
    m_bReadFiniteFloats = TRUE;
}

//=========================================================================

xbool bitstream::CanRead( s32 NBits ) const
{
    return !m_bReadError && (NBits >= 0) && (m_Cursor >= 0) &&
           (m_Cursor <= m_DataSizeInBits) &&
           (NBits <= m_DataSizeInBits - m_Cursor);
}

//=========================================================================

xbool bitstream::ReadError( void ) const
{
    return m_bReadError;
}

//=========================================================================

void bitstream::SetReadError( void ) const
{
    m_bReadError = TRUE;
}

//=========================================================================

xbool   bitstream::ReadFlag        ( void ) const
{
    if( !CanRead(1) )
    {
        SetReadError();
        return FALSE;
    }
    xbool B = (*(m_Data + (m_Cursor >> 3)) & (1 << (7-(m_Cursor & 0x7)))) != 0; 
    m_Cursor++;
    return B;
}

//=========================================================================

xbool bitstream::ReadFlag( xbool& Flag ) const
{
    return( Flag = ReadFlag() ); 
}

//=========================================================================

void    bitstream::WriteRawBits       ( const void* pData, s32 NBits )
{
#ifdef X_ASSERT
    u8* pI = (u8*)pData;
    ASSERT( (NBits < 32) || !( ( pI[0] == 0xFE )
                    &&  ( pI[1] == 0xED )
                    &&  ( pI[2] == 0xC0 )
                    &&  ( pI[3] == 0xDE ) ) );
#endif                    

    if( NBits==0 )
        return;

    // Check that we actually have bits to write
    ASSERT(NBits>0);

    // Check if writing out of bounds
    while( NBits > m_WriteLimit - m_Cursor )
    {
        if( m_bOwnsData && (m_WriteLimit == m_DataSizeInBits) ) Grow();
        else
        {
            m_bOverwrite = TRUE;
            return;
        }
    }

    s32     NBitsRemaining = NBits;
    s32     NBitsInBuffer = 0;
    u32     BitBuffer=0;
    byte*   pSrc = (byte*)pData;
    byte*   pDst = m_Data + (m_Cursor>>3);
    s32     DstOffset = (m_Cursor & 0x7);

    while( NBitsRemaining )
    {
        // Determine how many bits we can write to dst
        s32 NBitsToWrite = MIN(NBitsRemaining,8-DstOffset);

        // Get at least that many bits in buffer
        while( NBitsInBuffer < NBitsToWrite )
        {
            BitBuffer |= ((u32)(*pSrc)) << NBitsInBuffer;
            pSrc++;
            NBitsInBuffer+=8;
        }

        // Write out bits to dest
        
        // Get mask highlighting dst bits to be overwritten
        u8 Mask = (0xFF>>DstOffset) & (0xFF<<(8-(DstOffset+NBitsToWrite)));
        u8 Byte = (BitBuffer&(0xFF>>(8-NBitsToWrite))) << ((8-DstOffset)-NBitsToWrite);

        Byte = ((*pDst) & (~Mask)) | (Byte & Mask);

        *pDst++ = Byte;
        DstOffset = 0;
        BitBuffer >>= NBitsToWrite;
        NBitsInBuffer -= NBitsToWrite;
        NBitsRemaining -= NBitsToWrite;
    }

    m_Cursor += NBits;
    m_HighestBitWritten = MAX( m_Cursor-1, m_HighestBitWritten );
}

//=========================================================================

void    bitstream::ReadRawBits        ( void* pData, s32 NBits ) const
{
    if( !CanRead(NBits) || ((NBits > 0) && !pData) )
    {
        SetReadError();
        return;
    }
    if( NBits == 0 )
        return;

    s32     NBitsRemaining = NBits;
    s32     NBitsInBuffer = 0;
    u32     BitBuffer=0;
    s32     SrcOffset = (m_Cursor & 0x7);
    byte*   pSrc = m_Data + (m_Cursor>>3);
    byte*   pDst = (byte*)pData;

    while( NBitsRemaining )
    {
        // Determine how many bits we can read from source
        s32 NBitsToRead = MIN(NBitsRemaining,8-SrcOffset);

        // Read bits into the buffer
        byte Byte = (*pSrc) >> ((8-SrcOffset)-NBitsToRead);
        BitBuffer |= (Byte & ((1u << NBitsToRead) - 1u)) << NBitsInBuffer;

        pSrc++;
        SrcOffset=0;
        NBitsInBuffer += NBitsToRead;

        // Empty buffer if we've got a byte's worth
        while( NBitsInBuffer >= 8 )
        {
            *pDst++ = (BitBuffer&0xFF);
            BitBuffer>>=8;
            NBitsInBuffer -= 8;
        }

        NBitsRemaining -= NBitsToRead;
    }

    // Empty buffer if we've got a byte's worth
    if( NBitsInBuffer )
    {
        *pDst++ = BitBuffer;
    }

    m_Cursor += NBits;
}

//=========================================================================

void    bitstream::WriteRaw32         ( u32 Value, s32 NBits )
{
    ASSERT( Value != 0xFEEDC0DE );

    if( NBits==0 )
        return;

    // Check that we actually have bits to write
    ASSERT((NBits>0) && (NBits<=32));

    // Check if writing out of bounds
    while( NBits > m_WriteLimit - m_Cursor )
    {
        if( m_bOwnsData && (m_WriteLimit == m_DataSizeInBits) ) Grow();
        else
        {
            m_bOverwrite = TRUE;
            return;
        }
    }

    byte*   pDst        = m_Data + (m_Cursor>>3);
    byte*   pEnd        = m_Data + ((m_Cursor+NBits-1)>>3) + 1;
    s32     LeftOffset  = (m_Cursor & 0x7);
    s32     RightOffset = (40-LeftOffset-NBits);

    // Move cursor
    m_Cursor += NBits;
    m_HighestBitWritten = MAX( m_Cursor-1, m_HighestBitWritten );

    // Get mask highlighting bits that will be overwritten
    const u64 BitMask = static_cast<u64>( 0xFFFFFFFFFFULL );
    u64 WriteMask = (BitMask >> LeftOffset) &
                    (BitMask << RightOffset);

    // Get data to align with mask
    u64 DataMask  = (((u64)Value) << RightOffset) & WriteMask;

    WriteMask = ~WriteMask;

    // Write out bytes
    s32 Shift = 32;
    while( pDst != pEnd )
    {
        *pDst = (byte)((DataMask>>Shift) | ((*pDst)&(WriteMask>>Shift)));
        pDst++;
        Shift-=8;
    }
}

//=========================================================================

u32    bitstream::ReadRaw32          ( s32 NBits ) const
{
    if( (NBits > 32) || !CanRead(NBits) )
    {
        SetReadError();
        return 0;
    }
    if( NBits == 0 )
        return 0;

    s32     LeftOffset  = (m_Cursor & 0x7);
    s32     RightOffset = (40-LeftOffset-NBits);
    byte*   pSrc        = m_Data + (m_Cursor>>3);
    byte*   pEnd        = m_Data + ((m_Cursor+NBits-1)>>3) + 1;

    // Move cursor
    m_Cursor += NBits;

    // Get mask highlighting bits that will be read
    const u64 BitMask = static_cast<u64>( 0xFFFFFFFFFFULL );
    u64 ReadMask = (BitMask >> LeftOffset) &
                   (BitMask << RightOffset);

    // Clear data
    u64 DataMask = 0;

    // Read data
    s32 Shift = 32;
    while( pSrc != pEnd )
    {
        DataMask |= ((u64)(*pSrc++))<<Shift;
        Shift-=8;
    }

    u32 Value = (u32)((DataMask & ReadMask) >> RightOffset);

    return Value;
}

//=========================================================================
