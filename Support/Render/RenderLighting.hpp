//=========================================================================
//
//  RenderLighting.hpp
//
//  Scope-owned lighting records used by geometry submission.
//
//=========================================================================

#ifndef RENDER_LIGHTING_HPP
#define RENDER_LIGHTING_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_array.hpp"
#include "x_math.hpp"
#include "x_types.hpp"

//=========================================================================
//  CONSTANTS
//=========================================================================

#define MAX_GEOM_LIGHTS 4

static const s32 RENDER_MAX_LIGHTING_SETS = 32768;

//=========================================================================
//  TYPES
//=========================================================================

struct cb_geom_lighting
{
    s32     LightCount;
    s32     DynamicLightIndex[MAX_GEOM_LIGHTS];
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 LightDir[MAX_GEOM_LIGHTS];
    vector4 LightCone[MAX_GEOM_LIGHTS];
    vector4 LightCookieU[MAX_GEOM_LIGHTS];
    vector4 LightCookieV[MAX_GEOM_LIGHTS];
    vector4 AmbCol;
};

//-------------------------------------------------------------------------

// Generation makes stale or cross-scope handles fail closed.
struct RenderLightingHandle
{
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderLightingHandle( void );
    xbool IsValid( void ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    u32 Index;
    u32 Generation;
};

//=========================================================================
//  CLASSES
//=========================================================================

// RenderPipeline owns one reusable table; draw submissions store handles.
class RenderLightingTable
{
public:

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    RenderLightingTable( void );
    void BeginScope( void );
    void EndScope( void );

    //-------------------------------------------------------------------------
    //  LIGHTING RECORDS
    //-------------------------------------------------------------------------

    RenderLightingHandle    Allocate( void );
    cb_geom_lighting*       GetMutable( const RenderLightingHandle& Handle );
    const cb_geom_lighting* Get( const RenderLightingHandle& Handle ) const;
    s32                     GetCount( void ) const;
    const cb_geom_lighting& GetAt( s32 Index ) const;

private:

    //-------------------------------------------------------------------------
    //  HELPER FUNCTIONS
    //-------------------------------------------------------------------------

    xbool IsValid( const RenderLightingHandle& Handle ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    u32                      m_Generation;
    xbool                    m_isActive;
    xarray<cb_geom_lighting> m_Sets;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
RenderLightingHandle::RenderLightingHandle( void )
    : Index( 0xffffffffu ),
      Generation( 0 )
{
}

//=========================================================================

inline
xbool RenderLightingHandle::IsValid( void ) const
{
    return Index != 0xffffffffu;
}

//=========================================================================

inline
RenderLightingTable::RenderLightingTable( void )
    : m_Generation( 0 ),
      m_isActive( FALSE ),
      m_Sets()
{
    m_Sets.SetCapacity( RENDER_MAX_LIGHTING_SETS + 1 );
}

//=========================================================================

inline
void RenderLightingTable::BeginScope( void )
{
    ++m_Generation;

    if( m_Generation == 0 )
    {
        ++m_Generation;
    }

    m_Sets.SetCount( 1 );
    m_Sets[0]  = cb_geom_lighting();
    m_isActive = TRUE;
}

//=========================================================================

inline
void RenderLightingTable::EndScope( void )
{
    m_isActive = FALSE;
    m_Sets.SetCount( 0 );
}

//=========================================================================

inline
RenderLightingHandle RenderLightingTable::Allocate( void )
{
    RenderLightingHandle Result;

    if( !m_isActive || ( m_Sets.GetCount() >= m_Sets.GetCapacity() ) )
    {
        return Result;
    }

    Result.Index      = static_cast<u32>( m_Sets.GetCount() );
    Result.Generation = m_Generation;
    m_Sets.Append()   = cb_geom_lighting();

    return Result;
}

//=========================================================================

inline
cb_geom_lighting* RenderLightingTable::GetMutable( const RenderLightingHandle& Handle )
{
    if( !IsValid( Handle ) )
    {
        return NULL;
    }

    return &m_Sets[static_cast<s32>( Handle.Index )];
}

//=========================================================================

inline
const cb_geom_lighting* RenderLightingTable::Get( const RenderLightingHandle& Handle ) const
{
    if( !IsValid( Handle ) )
    {
        return NULL;
    }

    return &m_Sets[static_cast<s32>( Handle.Index )];
}

//=========================================================================

inline
s32 RenderLightingTable::GetCount( void ) const
{
    return MAX( 0, m_Sets.GetCount() - 1 );
}

//=========================================================================

inline
const cb_geom_lighting& RenderLightingTable::GetAt( s32 Index ) const
{
    return m_Sets[Index + 1];
}

//=========================================================================

inline
xbool RenderLightingTable::IsValid( const RenderLightingHandle& Handle ) const
{
    return m_isActive && Handle.IsValid() && ( Handle.Generation == m_Generation ) &&
           ( Handle.Index < static_cast<u32>( m_Sets.GetCount() ) );
}

#endif // RENDER_LIGHTING_HPP
