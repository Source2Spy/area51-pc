//=============================================================================
//
//  NetAddress.cpp
//
//=============================================================================

//=============================================================================
//  INCLUDES
//=============================================================================

#include "x_files.hpp"
#include "NetAddress.hpp"

//=============================================================================
// FUNCTIONS
//=============================================================================

net_address::net_address( void )
{
    Clear();
}

//=============================================================================

net_address::net_address( s32 IP, s32 Port )
{
    Setup( IP, Port );
}

//=============================================================================

net_address::net_address( const char* pIPStr, s32 Port )
{
    m_Port = Port;
    SetStrIP( pIPStr );
}

//=============================================================================

void net_address::Clear( void )
{
    m_IP   = 0;
    m_Port = 0; 

    UpdateStrings();
}

//=============================================================================

xbool net_address::IsEmpty( void ) const
{
    return( (m_IP==0) && (m_Port==0) );
}

//=============================================================================

xbool net_address::IsValid( void ) const
{
    if( m_IP ==      0x00000000 )
    {
        return( FALSE );
    }
    if( m_IP ==      0x7F000001 )
    {
        return( FALSE );
    }
    if( m_IP == (s32)0xFFFFFFFF )
    {
        return( FALSE );
    }

    return( TRUE );
}

//=============================================================================

void net_address::Setup( s32 IP, s32 Port )
{
    m_IP   = IP;
    m_Port = Port;

    UpdateStrings();
}

//=============================================================================

void net_address::SetIP( s32 IP )
{
    m_IP = IP;
    UpdateStrings();
}

//=============================================================================

void net_address::SetPortNumber( s32 Port )
{
    m_Port = Port;
    UpdateStrings();
}

//=============================================================================

void net_address::SetStrIP( const char* pIPStr )
{
    m_IP = 0;
    for( s32 i=0; i<4; i++ )
    {
        u32  D = 0;
        char C = *pIPStr++;

        while( (C>='0') && (C<='9') )
        {
            D = D*10 + (C-'0');
            C = *pIPStr++;
            ASSERT( D < 256 );
        }

        if( i < 3 )
        {
            ASSERT( C == '.' );
        }

        m_IP = (m_IP << 8) | D;
    }

    UpdateStrings();
}

//=============================================================================

xbool net_address::Parse( const char* pAddrStr, s32 DefaultPort )
{
    if( !pAddrStr || (DefaultPort < 1) || (DefaultPort > 65535) )
    {
        return FALSE;
    }

    u32 Address = 0;
    for( s32 i = 0; i < 4; i++ )
    {
        if( (*pAddrStr < '0') || (*pAddrStr > '9') )
        {
            return FALSE;
        }

        u32 Octet = 0;
        s32 Digits = 0;
        while( (*pAddrStr >= '0') && (*pAddrStr <= '9') )
        {
            Octet = Octet * 10 + (*pAddrStr++ - '0');
            if( (++Digits > 3) || (Octet > 255) )
            {
                return FALSE;
            }
        }
        Address = (Address << 8) | Octet;
        if( i < 3 )
        {
            if( *pAddrStr != '.' )
            {
                return FALSE;
            }
            pAddrStr++;
        }
    }

    s32 Port = DefaultPort;
    if( *pAddrStr == ':' )
    {
        pAddrStr++;
        Port = 0;
        if( (*pAddrStr < '0') || (*pAddrStr > '9') )
        {
            return FALSE;
        }
        s32 Digits = 0;
        while( (*pAddrStr >= '0') && (*pAddrStr <= '9') )
        {
            Port = Port * 10 + (*pAddrStr++ - '0');
            if( (++Digits > 5) || (Port > 65535) )
            {
                return FALSE;
            }
        }
    }

    if( *pAddrStr || !Port || !Address || ((Address >> 24) >= 224) )
    {
        return FALSE;
    }

    Setup( (s32)Address, Port );
    return TRUE;
}

//=============================================================================

void net_address::SetStrAddress( const char* pAddrStr )
{
    char C;

    m_IP   = 0;
    m_Port = 0;

    // Read IP.
    for( s32 i=0; i<4; i++ )
    {
        u32 D = 0;
        C = *pAddrStr++;

        while( (C>='0') && (C<='9') )
        {
            D = D*10 + (C-'0');
            C = *pAddrStr++;
            ASSERT( D < 256 );
        }
        
        if( i < 3 )
        {
            ASSERT( C == '.' );
        }

        m_IP = (m_IP << 8) | D;
    }

    // Read port.
    if( C == ':' ) 
    {
        C = *pAddrStr++;
        while( (C>='0') && (C<='9') )
        {
            m_Port = m_Port*10 + (C-'0');
            C      = *pAddrStr++;
        }
    }

    UpdateStrings();
}
    
//=============================================================================

s32 net_address::GetIP( void ) const
{
    return( m_IP );
}

//=============================================================================

s32 net_address::GetPort( void ) const
{
    return( m_Port );
}

//=============================================================================

const char* net_address::GetStrIP( void ) const
{
    return( m_IPStr );
}

//=============================================================================

const char* net_address::GetStrAddress( void ) const
{
    return( m_AddrStr );
}

//=============================================================================

xbool net_address::operator == ( const net_address& A ) const
{
    return( (m_IP == A.m_IP) && (m_Port == A.m_Port) );
}

//=============================================================================

xbool net_address::operator != ( const net_address& A ) const
{
    return( (m_IP != A.m_IP) || (m_Port != A.m_Port) );
}

//=============================================================================

void net_address::UpdateStrings( void )
{
    x_sprintf( m_IPStr,   "%d.%d.%d.%d",
                          (m_IP>>24)&0xFF,                         
                          (m_IP>>16)&0xFF,                         
                          (m_IP>> 8)&0xFF,                         
                          (m_IP>> 0)&0xFF );

    x_sprintf( m_AddrStr, "%d.%d.%d.%d:%d",
                          (m_IP>>24)&0xFF,                         
                          (m_IP>>16)&0xFF,                         
                          (m_IP>> 8)&0xFF,                         
                          (m_IP>> 0)&0xFF,
                          m_Port );

    ASSERT( x_strlen(m_IPStr  ) < 16 );
    ASSERT( x_strlen(m_AddrStr) < 22 );
}

//=============================================================================
