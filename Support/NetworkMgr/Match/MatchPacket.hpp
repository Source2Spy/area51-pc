#ifndef MATCH_PACKET_HPP
#define MATCH_PACKET_HPP

#include "MatchServer.hpp"

//==============================================================================

class match_packet
{
public:
    match_packet( void* pData, s32 Length ) : m_pData( (byte*)pData ), m_Length( Length ), m_Cursor( 0 ), m_Valid( pData && ( Length >= 0 ) )
    {
    }

    void Write( u64 Value, s32 Bytes )
    {
        if( !m_Valid || ( Bytes < 0 ) || ( Bytes > 8 ) || ( Bytes > m_Length - m_Cursor ) )
        {
            m_Valid = FALSE;
            return;
        }

        for( s32 i = Bytes - 1; i >= 0; i-- )
            m_pData[m_Cursor++] = (byte)( Value >> ( i * 8 ) );
    }

    u64 Read( s32 Bytes )
    {
        if( !m_Valid || ( Bytes < 0 ) || ( Bytes > 8 ) || ( Bytes > m_Length - m_Cursor ) )
        {
            m_Valid = FALSE;
            return 0;
        }

        u64 Value = 0;

        for( s32 i = 0; i < Bytes; i++ )
            Value = ( Value << 8 ) | m_pData[m_Cursor++];

        return Value;
    }

    void WriteString( const xwchar* pText, s32 Capacity )
    {
        s32 Length = 0;

        while( ( Length < Capacity ) && pText[Length] )
            Length++;

        if( Length == Capacity )
        {
            m_Valid = FALSE;
            return;
        }

        Write( Length, 1 );

        for( s32 i = 0; i < Length; i++ )
            Write( pText[i], 2 );
    }

    void ReadString( xwchar* pText, s32 Capacity )
    {
        if( !pText || ( Capacity <= 0 ) )
        {
            m_Valid = FALSE;
            return;
        }

        pText[0]   = 0;
        s32 Length = (s32)Read( 1 );

        if( !m_Valid || ( Length >= Capacity ) || ( Length > ( m_Length - m_Cursor ) / 2 ) )
        {
            m_Valid = FALSE;
            return;
        }

        xbool HighSurrogate = FALSE;

        for( s32 i = 0; i < Length; i++ )
        {
            u32   C            = (u32)Read( 2 );
            xbool LowSurrogate = ( C >= 0xDC00 ) && ( C <= 0xDFFF );

            if( ( C < 0x20 ) || ( C == 0x7F ) || ( HighSurrogate != LowSurrogate ) )
                m_Valid = FALSE;

            HighSurrogate = ( C >= 0xD800 ) && ( C <= 0xDBFF );
            pText[i]      = (xwchar)C;
        }

        if( HighSurrogate )
            m_Valid = FALSE;

        pText[Length] = 0;
    }

    void WriteServer( const server_info& Info )
    {
        Write( Info.Version, 4 );
        Write( Info.Players, 1 );
        Write( Info.MaxPlayers, 1 );
        Write( Info.GameTypeID, 1 );
        Write( Info.MutationMode, 1 );
        Write( Info.Level, 4 );
        Write( Info.Flags, 4 );
        Write( Info.PercentComplete, 1 );
        WriteString( Info.Name, NET_SERVER_NAME_LENGTH );
        WriteString( Info.MissionName, NET_MISSION_NAME_LENGTH );
        WriteString( Info.GameType, NET_GAME_TYPE_LENGTH );
        WriteString( Info.ShortGameType, NET_SHORT_GAME_TYPE_LENGTH );
    }

    xbool ReadServer( server_info& Result )
    {
        server_info Info;
        x_memset( &Info, 0, sizeof( Info ) );
        Info.Version         = (s32)Read( 4 );
        Info.Players         = (s32)Read( 1 );
        Info.MaxPlayers      = (s32)Read( 1 );
        Info.GameTypeID      = (s32)Read( 1 );
        Info.MutationMode    = (mutation_mode)Read( 1 );
        Info.Level           = (s32)Read( 4 );
        Info.Flags           = (s32)Read( 4 );
        Info.PercentComplete = (s32)Read( 1 );
        ReadString( Info.Name, NET_SERVER_NAME_LENGTH );
        ReadString( Info.MissionName, NET_MISSION_NAME_LENGTH );
        ReadString( Info.GameType, NET_GAME_TYPE_LENGTH );
        ReadString( Info.ShortGameType, NET_SHORT_GAME_TYPE_LENGTH );

        if( !m_Valid || !Info.Name[0] || ( Info.MaxPlayers < 1 ) ||
            ( Info.MaxPlayers > NET_MAX_PLAYERS ) || ( Info.Players > Info.MaxPlayers ) ||
            ( Info.GameTypeID > GAME_CNH ) || ( Info.MutationMode > MUTATE_MUTANT_HUNT ) ||
            ( Info.Level < 1000 ) || ( Info.Level > 9000 ) ||
            ( Info.PercentComplete > 100 ) || ( Info.Flags & ~0x1FFF ) ||
            ( Info.Flags & SERVER_IS_PRIVATE ) )
        {
            m_Valid = FALSE;
            return FALSE;
        }

        Info.Flags &= ~( SERVER_IS_A_FAVORITE );
        Info.ConnectionType  = CONNECT_DIRECT;
        Info.PasswordEnabled = !!( Info.Flags & SERVER_HAS_PASSWORD );
        Info.VoiceEnabled    = !!( Info.Flags & SERVER_VOICE_ENABLED );
        Result               = Info;
        return TRUE;
    }

    xbool IsValid( void ) const
    {
        return m_Valid;
    }

    xbool IsComplete( void ) const
    {
        return m_Valid && ( m_Cursor == m_Length );
    }

    s32 GetLength( void ) const
    {
        return m_Cursor;
    }

private:
    byte* m_pData;
    s32   m_Length;
    s32   m_Cursor;
    xbool m_Valid;
};

#endif // MATCH_PACKET_HPP
