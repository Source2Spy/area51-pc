#include "x_string.hpp"
#include "Entropy.hpp"
#include "TracerMgr.hpp"
#include "Render/PrimitiveBatch.hpp"

//=========================================================================

tracer_mgr  g_TracerMgr;

//=========================================================================

tracer_mgr::tracer_mgr( void )
{
    for ( s32 i = 0; i < NUM_TRACER_TYPES; i++ )
    {
        m_pTracers[i] = NULL;
    }
}

//=========================================================================

tracer_mgr::~tracer_mgr( void )
{
}

//=========================================================================

void tracer_mgr::Init( void )
{
    // allocate tracer arrays that aren't allowed to resize
    for ( s32 i = 0; i < NUM_TRACER_TYPES; i++ )
    {
        m_pTracers[i] = new tracer[MAX_TRACERS_PER_TYPE];
        x_memset( m_pTracers[i], 0, sizeof(tracer)*MAX_TRACERS_PER_TYPE );
    }

    // Load the renderer textures.
    m_Textures[TRACER_BMP_BULLET].SetName( PRELOAD_FILE("tracer_Bullet.xbmp") );
    if ( m_Textures[TRACER_BMP_BULLET].GetPointer() == NULL )
    {
        ASSERTS( 0, xfs( "Unable to load %s", m_Textures[TRACER_BMP_BULLET].GetName() ) );
    }

    m_Textures[TRACER_BMP_SWOOSH].SetName( PRELOAD_FILE("tracer_Bullet.xbmp") );
    if ( m_Textures[TRACER_BMP_SWOOSH].GetPointer() == NULL )
    {
        ASSERTS( 0, xfs( "Unable to load %s", m_Textures[TRACER_BMP_SWOOSH].GetName() ) );
    }

    m_Sequence = 0;

}

//=========================================================================

void tracer_mgr::Kill( void )
{
    for ( s32 i = 0; i < NUM_TRACER_TYPES; i++ )
    {
        if( m_pTracers[i] )
        {
            delete []m_pTracers[i];
            m_pTracers[i] = NULL;
        }
    }
}

//=========================================================================

tracer_id tracer_mgr::AddTracer( tracer_mgr::tracer_type Type,
                            f32                     FadeOutTime,
                            const vector3&          StartPos,
                            const vector3&          EndPos,
                            const xcolor&           Color )
{
    ASSERT(( Type < 256 ) && (Type >= 0));
    // find an open spot for a tracer
    f32   HighestT     = 0.0f;
    s32   HighestIndex = -1;
    s32   EmptySlot    = -1;
    for ( s32 i = 0; i < MAX_TRACERS_PER_TYPE; i++ )
    {
        if ( m_pTracers[Type][i].IsAlive == FALSE )
        {
            EmptySlot = i;
            break;
        }
        else
        {
            if ( m_pTracers[Type][i].FadeTime != 0.0f )
            {
                f32 T = m_pTracers[Type][i].ElapsedTime / m_pTracers[Type][i].FadeTime;
                if ( T > HighestT )
                {
                    HighestT     = T;
                    HighestIndex = i;
                }
            }
        }
    }
    if ( EmptySlot == -1 )
    {
        if ( HighestIndex != -1 )   EmptySlot = HighestIndex;
        else                        EmptySlot = 0;
    }

    ASSERT( (EmptySlot >= 0) && (EmptySlot < 256) );

    // add the tracer to our list
    tracer& Tracer = m_pTracers[Type][EmptySlot];
    Tracer.ElapsedTime = 0.0f;
    Tracer.FadeTime    = FadeOutTime;
    Tracer.IsAlive     = TRUE;
    Tracer.StartPos    = StartPos;
    Tracer.EndPos      = EndPos;
    Tracer.Color       = Color;
    Tracer.Sequence    = m_Sequence;

    tracer_id ID;

    ID = (Type << 24) | (EmptySlot << 16) | m_Sequence;

    m_Sequence++;   // Let it wrap around naturally

    return ID;
}

//=========================================================================

void tracer_mgr::OnUpdate( f32 DeltaTime )
{
    // update the live list of tracer
    for ( s32 Type = 0; Type < NUM_TRACER_TYPES; Type++ )
    {
        for ( s32 i = 0; i < MAX_TRACERS_PER_TYPE; i++ )
        {
            tracer& Tracer = m_pTracers[Type][i];
            if ( Tracer.IsAlive )
            {
                Tracer.ElapsedTime += DeltaTime;
                if ( Tracer.ElapsedTime > Tracer.FadeTime )
                {
                    Tracer.IsAlive = FALSE;
                }
            }
        }
    }
}

//=========================================================================

void tracer_mgr::Render( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "tracer_mgr::Render" );

    const view* pView = eng_GetView();
    if( !pView )
        return;

    const tracer_bmp_types Textures[NUM_TRACER_TYPES] =
    {
        TRACER_BMP_BULLET,
        TRACER_BMP_SWOOSH
    };
    const f32 Radii[NUM_TRACER_TYPES] = { 10.0f, 20.0f };

    matrix4 Identity;
    Identity.Identity();

    for( s32 Type = 0; Type < NUM_TRACER_TYPES; Type++ )
    {
        const texture* pTexture = m_Textures[Textures[Type]].GetPointer();
        if( !pTexture )
            continue;

        const render::primitive_draw_desc Material( pTexture,
                                                    render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                    render::PRIMITIVE_BLEND_ADDITIVE,
                                                    render::PRIMITIVE_DEPTH_READ_ONLY,
                                                    render::PRIMITIVE_RASTER_SOLID_NO_CULL,
                                                    render::PRIMITIVE_SAMPLER_ANISOTROPIC_WRAP);
        render::PrimitiveBatch Batch( Material );
        Batch.Reserve( MAX_TRACERS_PER_TYPE * 4, MAX_TRACERS_PER_TYPE * 6 );

        for( s32 i = 0; i < MAX_TRACERS_PER_TYPE; i++ )
        {
            const tracer& Tracer = m_pTracers[Type][i];
            if( !Tracer.IsAlive )
                continue;

            xcolor Color = Tracer.Color;
            Color.A = (Tracer.FadeTime == 0.0f)
                    ? 255
                    : (u8)(255.0f * (1.0f - Tracer.ElapsedTime / Tracer.FadeTime));

            Batch.AddViewOrientedQuad( Tracer.StartPos,
                                       Tracer.EndPos,
                                       pView->GetPosition(),
                                       vector2( 0.0f, 0.0f ),
                                       vector2( 1.0f, 1.0f ),
                                       Color,
                                       Color,
                                       Radii[Type],
                                       Radii[Type] );
        }

        Batch.Submit( Identity );
    }
}

//=========================================================================

void tracer_mgr::UpdateTracer(  tracer_id         ID,
                                const vector3*    pStartPos,
                                const vector3*    pEndPos,
                                const xcolor*     pColor )
{
    if (ID == NULL_TRACER_ID)
        return;

    u32             Sequence = 0;
    u32             iSlot    = 0;
    tracer_type     iType    = TRACER_TYPE_UNKNOWN;

    Sequence =  ID & 0x0000FFFF;
    iSlot    = (ID >> 16) & 0x000000FF;
    iType    = (tracer_type)((ID >> 24) & 0x000000FF);
    
    ASSERT( iSlot < MAX_TRACERS_PER_TYPE );
    ASSERT( iType < NUM_TRACER_TYPES );

    // Catch potential release mode errors
    if (iSlot >= MAX_TRACERS_PER_TYPE)
        return;
    if (iType >= NUM_TRACER_TYPES)
        return;

    tracer& T = m_pTracers[ iType ][ iSlot ];

    if (T.Sequence != Sequence)
    {
        // This is a stale ID
        return;
    }

    if (pStartPos)
        T.StartPos  = *pStartPos;
    if (pEndPos)
        T.EndPos    = *pEndPos;
    if (pColor)
        T.Color     = *pColor;
}


//=========================================================================

//=========================================================================

//=========================================================================
