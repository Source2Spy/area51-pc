//==============================================================================
//
//  ConnectionProtocol.hpp
//
//==============================================================================

#ifndef CONNECTION_PROTOCOL_HPP
#define CONNECTION_PROTOCOL_HPP

#include "x_files.hpp"
#include "e_Network.hpp"
#include "Network/NetStream.hpp"
#include <random>

//==============================================================================

enum conn_packet_type
{
            CONN_PACKET_FIRST          = 0,
            CONN_PACKET_LOGIN_REQUEST   ,
            CONN_PACKET_LOGIN_RESPONSE  ,        
            CONN_PACKET_HB_REQUEST      ,
            CONN_PACKET_HB_RESPONSE     ,
            CONN_PACKET_GAME_DATA       ,
            CONN_PACKET_MISSION_REQUEST ,
            CONN_PACKET_MISSION_RESPONSE,
            CONN_PACKET_VERIFY_REQUEST  ,
            CONN_PACKET_VERIFY_RESPONSE ,
            CONN_PACKET_END_MISSION     ,
            CONN_PACKET_GAME_STAGE      ,
            CONN_PACKET_DISCONNECT      ,
            CONN_PACKET_INSYNC          ,
            CONN_PACKET_CS              ,
            CONN_PACKET_CANCEL_LOGIN,
            CONN_PACKET_CANCEL_ACK,
            CONN_PACKET_BUILD_CHALLENGE,
            CONN_PACKET_LAST            ,
};

const   u16                 CONN_PACKET_IDENTIFIER  = 0xf00d;

const f32 CONNECTION_SYNC_TIMEOUT     = 30.0f;
const f32 CONNECTION_COOLDOWN_TIMEOUT = 5.0f;
const f32 CONNECTION_CANCEL_TIMEOUT   = 10.0f;
const f32 CONNECTION_CANCEL_INTERVAL  = 0.25f;
const f32 CONNECTION_RETIRED_TIMEOUT  = 30.0f;

//==============================================================================

inline xbool CreateConnectionID( u64& ID )
{
    try
    {
        std::random_device Random;
        ID = ((u64)Random() << 32) | (u64)Random();
        return ID != 0;
    }
    catch( ... )
    {
        return FALSE;
    }
}

inline void OpenConnectionPacket( netstream& Stream, conn_packet_type Type, u64 ID )
{
    ASSERT( ID != 0 );
    Stream.Open( CONN_PACKET_IDENTIFIER, Type );
    Stream.WriteU64( ID );
}

inline xbool ReadConnectionPacket( netstream& Stream, conn_packet_type& Type, u64& ID )
{
    Stream.RequireFiniteFloats();

    if( (Stream.GetNBytes() < 14) || !Stream.Validate() )
        return FALSE;
    u16 Identifier;
    u16 PacketType;
    Stream.ReadU16( Identifier );
    Stream.ReadU16( PacketType );
    Stream.ReadU64( ID );
    if( Stream.Overwrite() || (Identifier != CONN_PACKET_IDENTIFIER) ||
        (PacketType <= CONN_PACKET_FIRST) || (PacketType >= CONN_PACKET_LAST) || !ID )
        return FALSE;
    Type = (conn_packet_type)PacketType;
    return TRUE;
}

inline xbool CanReadConnectionBits( const netstream& Stream, s32 Bits )
{
    return Stream.CanRead( Bits );
}

inline xbool ReadConnectionString( netstream& Stream, char* pText, s32 Capacity )
{
    return Stream.ReadString( pText, Capacity );
}

inline xbool FinishConnectionPacket( const bitstream& Stream )
{
    s32 Remaining = Stream.GetCursorRemaining();
    if( Stream.ReadError() || (Remaining < 0) || (Remaining > 7) )
    {
        Stream.SetReadError();
        return FALSE;
    }

    while( Remaining-- )
    {
        if( Stream.ReadFlag() )
        {
            Stream.SetReadError();
            return FALSE;
        }
    }

    return !Stream.ReadError();
}

inline xbool IsMissionPacket( conn_packet_type Type )
{
    return (Type == CONN_PACKET_MISSION_REQUEST) || (Type == CONN_PACKET_MISSION_RESPONSE) ||
           (Type == CONN_PACKET_VERIFY_REQUEST) || (Type == CONN_PACKET_VERIFY_RESPONSE);
}

//==============================================================================

class connection_cancel_queue
{
public:
    void Add( const net_address& Remote, u64 ID )
    {
        if( !ID )
            return;
        for( s32 i = 0; i < m_Entries.GetCount(); i++ )
            if( (m_Entries[i].Remote == Remote) && (m_Entries[i].ID == ID) )
                return;
        entry& Entry = m_Entries.Append();
        Entry.Remote = Remote;
        Entry.ID = ID;
        Entry.Remaining = CONNECTION_CANCEL_TIMEOUT;
        Entry.Delay = 0.0f;
    }

    void Update( net_socket& Socket, f32 DeltaTime )
    {
        for( s32 i = m_Entries.GetCount() - 1; i >= 0; i-- )
        {
            entry& Entry = m_Entries[i];
            Entry.Remaining -= DeltaTime;
            if( (Entry.Remaining <= 0.0f) || Socket.IsEmpty() )
            {
                m_Entries.Delete( i );
                continue;
            }
            Entry.Delay -= DeltaTime;
            if( Entry.Delay <= 0.0f )
            {
                netstream Stream;
                OpenConnectionPacket( Stream, CONN_PACKET_CANCEL_LOGIN, Entry.ID );
                Stream.Close();
                Stream.Send( Socket, Entry.Remote );
                Entry.Delay = CONNECTION_CANCEL_INTERVAL;
            }
        }
    }

    xbool ReceivePacket( const net_address& Remote, netstream& Stream )
    {
        conn_packet_type Type;
        u64 ID;
        if( !ReadConnectionPacket(Stream, Type, ID) || (Type != CONN_PACKET_CANCEL_ACK) ||
            !FinishConnectionPacket(Stream) )
            return FALSE;
        for( s32 i = m_Entries.GetCount() - 1; i >= 0; i-- )
            if( (m_Entries[i].Remote == Remote) && (m_Entries[i].ID == ID) )
                m_Entries.Delete( i );
        return TRUE;
    }

    void Clear( void ) { m_Entries.Clear(); }
    s32 GetCount( void ) const { return m_Entries.GetCount(); }

private:
    struct entry
    {
        net_address Remote;
        u64         ID;
        f32         Remaining;
        f32         Delay;
    };
    xarray<entry> m_Entries;
};

//==============================================================================

class retired_connections
{
public:
    void Update( f32 DeltaTime )
    {
        for( s32 i = m_Entries.GetCount() - 1; i >= 0; i-- )
        {
            m_Entries[i].Remaining -= DeltaTime;
            if( m_Entries[i].Remaining <= 0.0f )
                m_Entries.Delete( i );
        }
    }

    xbool Contains( const net_address& Remote, u64 ID ) const
    {
        for( s32 i = 0; i < m_Entries.GetCount(); i++ )
            if( (m_Entries[i].Remote == Remote) && (m_Entries[i].ID == ID) )
                return TRUE;
        return FALSE;
    }

    xbool Add( const net_address& Remote, u64 ID )
    {
        for( s32 i = 0; i < m_Entries.GetCount(); i++ )
        {
            if( (m_Entries[i].Remote == Remote) && (m_Entries[i].ID == ID) )
            {
                m_Entries[i].Remaining = CONNECTION_RETIRED_TIMEOUT;
                return TRUE;
            }
        }
        if( m_Entries.GetCount() >= 256 )
            return FALSE;
        entry& Entry = m_Entries.Append();
        Entry.Remote = Remote;
        Entry.ID = ID;
        Entry.Remaining = CONNECTION_RETIRED_TIMEOUT;
        return TRUE;
    }

    void Clear( void ) { m_Entries.Clear(); }

private:
    struct entry
    {
        net_address Remote;
        u64         ID;
        f32         Remaining;
    };
    xarray<entry> m_Entries;
};

#endif // CONNECTION_PROTOCOL_HPP
