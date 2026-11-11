//=========================================================================
//
//  GMMgr.cpp
//
//=========================================================================

#include "x_files.hpp"
#include "GMMgr.hpp"
#include "NetworkMgr/Connection/ConnMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"

//=========================================================================

gm_mgr::gm_mgr( void )
{
}

//=========================================================================

gm_mgr::~gm_mgr( void )
{
}

//=========================================================================

void gm_mgr::Init( void )
{
    m_Connected  = TRUE;

    m_DirtyBits  = 0xFFFFFFFF;
    m_ScoreBits  = 0xFFFFFFFF;
    m_PlayerBits = 0xFFFFFFFF;
}

//=========================================================================

void gm_mgr::Kill( void )
{
    m_Connected  = FALSE;
}

//=========================================================================

void gm_mgr::ProvideUpdate( conn_packet& Packet, bitstream& BitStream )
{
    if( !m_Connected ) 
        return;

    // Make backup copies of the dirty bits.
    u32 DirtyBits  = m_DirtyBits;
    u32 ScoreBits  = m_ScoreBits;
    u32 PlayerBits = m_PlayerBits;

    s32 Start = BitStream.GetCursor();
    BitStream.WriteU32( 0, 16 );

    // Let the GameMgr send whatever data it can given the current
    // dirty bits.  This function call will clear bits which are sent.
    GameMgr.ProvideUpdate( BitStream,
                           Packet.TargetClient,
                           m_DirtyBits, 
                           m_ScoreBits, 
                           m_PlayerBits );

    if( !BitStream.Overwrite() )
    {
        s32 End = BitStream.GetCursor();
        BitStream.SetCursor( Start );
        BitStream.WriteU32( End - Start - 16, 16 );
        BitStream.SetCursor( End );
    }

    // Now, save into the packet the bits which were written.  Simply XOR
    // the backup copies of the bit masks with the updated versions.
    Packet.GMDirtyBits   =  m_DirtyBits   ^  DirtyBits;
    Packet.GMScoreBits   =  m_ScoreBits   ^  ScoreBits;
    Packet.GMPlayerBits  =  m_PlayerBits  ^  PlayerBits;
}

//=========================================================================

void gm_mgr::AcceptUpdate( bitstream& BitStream )
{
    if( !m_Connected ) 
        return;

    u32 Length;
    BitStream.ReadU32( Length, 16 );
    bitstream Record;
    if( !BitStream.ReadStream(Record, Length) )
        return;
    GameMgr.AcceptUpdate( Record );
    if( Record.ReadError() )
        BitStream.SetReadError();
}

//=========================================================================

void gm_mgr::PacketAck( conn_packet& Packet, xbool Arrived )
{
    if( !m_Connected ) 
        return;

    if( !Arrived )
    {
        m_DirtyBits  |= Packet.GMDirtyBits; 
        m_ScoreBits  |= Packet.GMScoreBits;
        m_PlayerBits |= Packet.GMPlayerBits;
    }
}

//=========================================================================

xbool gm_mgr::IsConnected( void )
{
    return m_Connected;
}

//=========================================================================

void gm_mgr::AddDirtyBits( u32 DirtyBits, u32 ScoreBits, u32 PlayerBits )
{
    m_DirtyBits  |= DirtyBits;
    m_ScoreBits  |= ScoreBits;
    m_PlayerBits |= PlayerBits;
}

//=========================================================================
