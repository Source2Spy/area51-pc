//==============================================================================
//
//  fx_SPEmitter.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "fx_SPEmitter.hpp"
#include "fx_Render.hpp"
#include "x_profile.hpp"

#ifdef DEBUG_FX
#include "e_Engine.hpp"
#endif

//==============================================================================
//  STATIC-LIBRARY REGISTRATION ANCHOR
//==============================================================================

s32 fx_SPEmitter;

//==============================================================================
//  FUNCTIONS
//==============================================================================

inline 
static byte* Align16Ptr( byte* pData )
{
    return (byte*)(((uaddr)pData + 15) & ~((uaddr)15));
}

//==============================================================================

s32 SPEmitterMemoryFn( const fx_element_def& ElementDef )
{
    fx_edef_spemitter& EmitterDef = (fx_edef_spemitter&)ElementDef;

    s32 nParticles = EmitterDef.NParticles;

    u32 Size = sizeof( fx_spemitter );

    Size += sizeof(fx_sparticle) * nParticles;
    Size += sizeof(vector4     ) * nParticles;
    Size += sizeof(vector4     ) * nParticles;

    Size = ALIGN_16( Size );
    Size += (EmitterDef.Flags & SPE_REVERSE_MODE) ? sizeof(vector3     ) * nParticles : 0;

    Size = ALIGN_16( Size );
    Size += sizeof(vector3) * nParticles;

    Size = ALIGN_16( Size );
    Size += sizeof(u32) * nParticles;

    Size = ALIGN_16( Size );
    Size += (EmitterDef.Flags & SPE_VELOCITY_ORIENTED) ? 0 : sizeof(vector2) * nParticles;

    Size = ALIGN_16( Size );
    Size += sizeof(s32) * nParticles;

    Size = ALIGN_16( Size );
    Size += sizeof(s32) * nParticles;

    // The extra 15 here is because the allocator on the PC is not 16 byte aligned but the preceeding
    // code assumes 16 byte alignment, the 15 gives room for any error.
    return ALIGN_16( Size + 15 );
}

//==============================================================================

void fx_spemitter::Initialize( const fx_element_def* pElementDef, 
                                     f32*            pInput )
{
    // Let the base class take care of the basics.
    fx_element::Initialize( pElementDef, pInput );

    //
    // Now, on to the sprite particle emitter specific details.
    //

    s32                i;
    fx_edef_spemitter& EmitterDef = (fx_edef_spemitter&)(*pElementDef);

    //
    // TO DO - Do a better job with this stuff in here.
    //

    if( EmitterDef.Flags & SPE_BURST_MODE )
        m_EmitGap = (EmitterDef.TimeStop - EmitterDef.TimeStart) / EmitterDef.NParticles;
    else
        m_EmitGap = EmitterDef.LifeSpan / EmitterDef.NParticles;

    m_NActive      = 0;
    m_PCursor      = 0;
    m_EmitClock    = 0;
    m_EmitCycle    = EmitterDef.LifeSpan;
    m_Emitting     = TRUE;
    m_PrevL2WReady = FALSE;
    m_Color.Set( 255, 255, 255, (u8)0 );

    s32 nParticles = EmitterDef.NParticles;
    byte* pData = (byte*)(this+1);
    m_pParticles    = (fx_sparticle*)pData; pData += sizeof(fx_sparticle) * nParticles;
    m_pPositions    = (vector4*     )pData; pData += sizeof(vector4     ) * nParticles;
    m_pVelocities   = (vector4*     )pData; pData += sizeof(vector4     ) * nParticles;
    
    pData = Align16Ptr( pData );
    m_pStartPos     = (EmitterDef.Flags & SPE_REVERSE_MODE) ? (vector3*)pData : (vector3*)0;
    pData += (EmitterDef.Flags & SPE_REVERSE_MODE) ? sizeof(vector3) * nParticles : 0;

    pData = Align16Ptr( pData );
    m_pStartVel     = (vector3*)pData;
    pData += sizeof(vector3) * nParticles;

    pData = Align16Ptr( pData );
    m_pColors       = (u32*)pData;
    pData += sizeof(u32) * nParticles;

    pData = Align16Ptr( pData );
    m_pRotAndScales = (EmitterDef.Flags & SPE_VELOCITY_ORIENTED) ? (vector2*)0 : (vector2*)pData;
    pData += (EmitterDef.Flags & SPE_VELOCITY_ORIENTED) ? 0 : sizeof(vector2) * nParticles;

    pData = Align16Ptr( pData );
    m_pActiveIndices = (s32*)pData;
    pData += sizeof(s32) * nParticles;

    pData = Align16Ptr( pData );
    m_pActivePositions = (s32*)pData;
    pData += sizeof(s32) * nParticles;

    vector4     InactivePos( 0.0f, 0.0f, 0.0f, 0.0f );
    InactivePos.GetIW() = 0x8000;

    for( i = 0; i < nParticles; i++ )
    {
        fx_sparticle& P = m_pParticles[i];
        m_pPositions[i] = InactivePos;
        m_pActivePositions[i] = -1;
        P.EmitTime      = i * m_EmitGap;
//      P.AgeRate       = 1.0f / EmitterDef.LifeSpan;
        P.StartSpin     = (EmitterDef.MinSpinRate == EmitterDef.MaxSpinRate) 
                          ? EmitterDef.MinSpinRate
                          : x_frand( R_0, R_360 );
    }
}

//==============================================================================

void fx_spemitter::AdvanceLogic( const fx_effect_base* pEffect, f32 DeltaTime )
{
    EmissionLogic( pEffect, DeltaTime );
    ParticleLogic( pEffect, DeltaTime );
}

//==============================================================================

inline 
f32 fast_rand( f32 Min, f32 Max )
{
    static s32 Seed = 15827;
    Seed = Seed * 214013 + 2531011;
    f32 r = (f32)((Seed >> 16) & X_RAND_MAX);
    return( ((r / (f32)X_RAND_MAX) * (Max-Min)) + Min );
}

//==============================================================================

void fx_spemitter::EmissionLogic( const fx_effect_base* pEffect, f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "fx_spemitter::EmissionLogic" );

    const matrix4&     L2W = GetL2W( pEffect );
    matrix4            StartVelL2W;    
    fx_edef_spemitter& EmitterDef = (fx_edef_spemitter&)(*m_pElementDef);

    if( m_Emitting && !pEffect->IsSuspended() )
    {
        // Time to stop m_Emitting?
        if( pEffect->GetAge() >= EmitterDef.TimeStop )
            m_Emitting = FALSE;

        if( EmitterDef.Flags & SPE_WORLD_SPACE )
        {
            StartVelL2W = L2W;
            StartVelL2W.ClearTranslation();
        }

        // Read some values for the loop
        const vector3p& VMin            = EmitterDef.MinVelocity;
        const vector3p& VMax            = EmitterDef.MaxVelocity;
        s32             nActive         = m_NActive;
        s32             iCursor         = m_PCursor;
        const u32       EmitterDefFlags = EmitterDef.Flags;
        vector3         p;
        vector3         v;

        // Kill old particles and emit new ones.
        m_EmitClock += DeltaTime;
        while( m_Emitting && (m_pParticles[iCursor].EmitTime < m_EmitClock) )
        {
            fx_sparticle& P    = m_pParticles[iCursor];

            // Due to floating point error, the particles will sometimes have
            // a sliver of life left in them and thus still active.  Time for
            // an early retirement!
            xbool const bWasActive = ( m_pPositions[iCursor].GetIW() != 0x8000 );
            if( bWasActive )
            {
                s32 const activePosition = m_pActivePositions[iCursor];
                ASSERT( ( activePosition >= 0 ) && ( activePosition < nActive ) );
                s32 const replacementIndex = m_pActiveIndices[nActive - 1];
                m_pActiveIndices[activePosition] = replacementIndex;
                m_pActivePositions[replacementIndex] = activePosition;
                m_pActivePositions[iCursor] = -1;
                nActive -= 1;
            }

            // Emit this particle.
            m_pPositions[iCursor].GetIW() = 0x0000;
            P.SpinRate = fast_rand( EmitterDef.MinSpinRate, EmitterDef.MaxSpinRate );
            P.Age      = m_EmitClock - m_pParticles[iCursor].EmitTime;

            if( EmitterDefFlags & SPE_EMIT_FROM_VOLUME )
            {
                f32 x = fast_rand( -0.5f, 0.5f );
                f32 y = fast_rand( -0.5f, 0.5f );
                f32 z = fast_rand( -0.5f, 0.5f );
                p.Set( x, y, z );
            }
            else
            {
                p.Zero();
            }

            v.Set( fast_rand( VMin.X, VMax.X ),
                   fast_rand( VMin.Y, VMax.Y ),
                   fast_rand( VMin.Z, VMax.Z ) );

            if( EmitterDefFlags & SPE_WORLD_SPACE )
            {
                v = StartVelL2W * v;

                if( (m_PrevL2WReady) &&
                    (DeltaTime > 0.0f) && 
                    (DeltaTime > (m_EmitGap * 1.25f)) )
                {
                    // It is very likely that more than one particle will be 
                    // emitted during this logic.  To prevent "clumping", we
                    // must interpolate the point from which the particles 
                    // emit.  We have the previous L2W and the current L2W.

                    f32     t  = P.Age / DeltaTime;
                    vector3 P1 = m_PreviousL2W * p;
                    vector3 P2 =           L2W * p;
                    p = (P1 * t) + (P2 * (1.0f - t));
                }
                else
                {
                    p = L2W * p;
                }
            }

            if( EmitterDefFlags & SPE_REVERSE_MODE )
            {
                m_pStartPos[iCursor] = p;
            }

            m_pStartVel[iCursor] = v;
            m_pPositions [iCursor] = p;
            m_pVelocities[iCursor] = v;

            ASSERT( nActive < EmitterDef.NParticles );
            m_pActiveIndices[nActive] = iCursor;
            m_pActivePositions[iCursor] = nActive;
            nActive += 1;
            iCursor += 1;

            if( iCursor >= EmitterDef.NParticles )
            {
                iCursor    = 0;
                m_EmitClock -= m_EmitCycle;

                if( EmitterDefFlags & SPE_BURST_MODE )
                {
                    m_Emitting = FALSE;
                }
            }
        }

        // Write back out the cache variables
        m_NActive = nActive;
        m_PCursor = iCursor;

        if( EmitterDefFlags & SPE_WORLD_SPACE )
        {
            m_PrevL2WReady = TRUE;
            m_PreviousL2W  = L2W;
        }
    }
}

//==============================================================================

void fx_spemitter::ParticleLogic( const fx_effect_base* pEffect, f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "fx_spemitter::ParticleLogic" );

    s32                 i;
    f32                 MaxRadius;
    s32                 BoxSkip    = 0;
    fx_edef_spemitter&  EmitterDef = (fx_edef_spemitter&)(*m_pElementDef);
    vector3             Gravity3( 0.0f, EmitterDef.Gravity, 0.0f );

    m_BBox.Clear();

    MaxRadius = 0.0f;

    if( m_NActive == 0 )
    {
        m_BBox = pEffect->GetL2W() * m_Translate;
        return;
    }

    f32     UniScale  = pEffect->GetUniformScale();

    vector4 ElementColor( m_Color.R / 255.0f,
                          m_Color.G / 255.0f,
                          m_Color.B / 255.0f,
                          m_Color.A / 255.0f );

    if( EmitterDef.Flags & SPE_SCALE_SPRITE_SIZE )
        UniScale *= GetUniformScale();

    // Get Max frames from the EmitterDef such that 0 <= Max < EmitterDef.NKeyFrames-1
    f32 Max = EmitterDef.NKeyFrames - 1.00001f;
    if( Max < 0.0f )
        Max = 0.0f;

//    vector4     Gravity4( 0.0f, EmitterDef.Gravity, 0.0f, 0.0f );
    vector4     InactivePos( 0.0f, 0.0f, 0.0f, 0.0f );
    InactivePos.GetIW() = 0x8000;

    // Update only active particles.
    for( i = 0; i < m_NActive; )
    {
        s32 const particleIndex = m_pActiveIndices[i];
        fx_sparticle& P = m_pParticles[particleIndex];
        P.Age += DeltaTime;     // P.Age += P.AgeRate * DeltaTime;

        if( P.Age >= EmitterDef.LifeSpan )
        {
            m_pPositions[particleIndex] = InactivePos;
            s32 const replacementIndex = m_pActiveIndices[m_NActive - 1];
            m_pActiveIndices[i] = replacementIndex;
            m_pActivePositions[replacementIndex] = i;
            m_pActivePositions[particleIndex] = -1;
            --m_NActive;
            continue;
        }
        else
        {
//                f32 Age       = (EmitterDef.Flags & SPE_REVERSE_MODE) ? 
//                                (EmitterDef.LifeSpan - P.Age) :
//                                (P.Age);
//                f32 Factor    = Age * Age * 0.5f;

                // Figure out which keyframes to use and a blend factor.
                // TO DO - Rig the parametric age to be the frames.
                f32 Frame  = (P.Age / EmitterDef.LifeSpan) * Max;
                s32 Lo     = (s32)Frame;
                s32 Hi     = Lo + 1;
                f32 LoMix  = Hi - Frame;
                f32 HiMix  = Frame - Lo;

                if( Max == 0 )
                {
                    Hi    = 0;
                    LoMix = 1.0f;
                    HiMix = 0.0f;
                }

                // Calculate Color
                xcolor Color;
                Color.R = (u8)(((EmitterDef.Key[Lo].Color.R * LoMix)  + 
                                (EmitterDef.Key[Hi].Color.R * HiMix)) *
                                (ElementColor.GetX()));

                Color.G = (u8)(((EmitterDef.Key[Lo].Color.G * LoMix)  + 
                                (EmitterDef.Key[Hi].Color.G * HiMix)) *
                                (ElementColor.GetY()));

                Color.B = (u8)(((EmitterDef.Key[Lo].Color.B * LoMix)  + 
                                (EmitterDef.Key[Hi].Color.B * HiMix)) *
                                (ElementColor.GetZ()));

                Color.A = (u8)(((EmitterDef.Key[Lo].Color.A * LoMix)  + 
                                (EmitterDef.Key[Hi].Color.A * HiMix)) *
                                (ElementColor.GetW()));

                m_pColors[particleIndex] = (Color.A << 24) | (Color.B << 16) | (Color.G << 8) | Color.R;

                // Calculate Scale
                f32 Scale = ((EmitterDef.Key[Lo].Scale * LoMix) + (EmitterDef.Key[Hi].Scale * HiMix));

                vector4 v;
                vector3 p;

                if( EmitterDef.Flags & SPE_REVERSE_MODE )
                {

                    f32 Age    = (EmitterDef.LifeSpan - P.Age);
                    f32 Factor = Age * Age * 0.5f;

                    vector3 sp = m_pStartPos[particleIndex];
                    vector3 sv = m_pStartVel[particleIndex];

                    // Calc position
                    p = sp + sv * ((EmitterDef.Acceleration * Factor) + Age) - Gravity3 * Factor;

                    // Calc velocity
                    v = sv + sv * (EmitterDef.Acceleration * P.Age) - Gravity3 * P.Age;
                    v.GetW() = Scale;

                    m_pPositions[particleIndex] = p;
                    m_pVelocities[particleIndex] = v;
                }
                else
                {
                    // Update position
                    vector4& Position = m_pPositions[particleIndex];
                    vector4& Velocity = m_pVelocities[particleIndex];
                    p = *(vector3*)&Position;
                    p += *(vector3*)&Velocity * DeltaTime;
                    *(vector3*)&Position = p;
                    Position.GetIW() = 0x0000;

                    // Update velocity
                    v = Velocity;
                    v += m_pStartVel[particleIndex] * (EmitterDef.Acceleration * DeltaTime);
                    v.GetY() -= EmitterDef.Gravity * DeltaTime;
                    v.GetW() = Scale;
                    Velocity = v;
                }

                // Velocity & Scale or Rotation & Scale
                if( !(EmitterDef.Flags & SPE_VELOCITY_ORIENTED) )
                {
                    radian Rotation = P.StartSpin + P.SpinRate * P.Age;
                    Rotation = x_ModAngle2( Rotation ); // TODO: Optimize this?
                    m_pRotAndScales[particleIndex].Set( Rotation, Scale );
                }

                if( BoxSkip-- <= 0 )
                {
                    m_BBox    += p;
                    MaxRadius  = MAX( MaxRadius, Scale * UniScale * 0.75f );
                    BoxSkip    = m_NActive >> 2;
                }
            }
        ++i;
    }

    if( m_NActive == 0 )
    {
        m_BBox = pEffect->GetL2W() * m_Translate;
        return;
    }

    if( !(EmitterDef.Flags & SPE_WORLD_SPACE) )
    {
        m_BBox.Transform( GetL2W( pEffect ) );
    }
    m_BBox.Inflate( MaxRadius, MaxRadius, MaxRadius );
}

//==============================================================================

void fx_spemitter::SubmitRender( const fx_effect_base* pEffect ) const
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "fx_spemitter::SubmitRender" );

    fx_edef_spemitter& EmitterDef = (fx_edef_spemitter&)(*m_pElementDef);

    if( m_NActive == 0 )
        return;

    if( (m_NActive + fx_mgr::GetSpritesThisFrame()) > fx_mgr::GetSpriteBudget() )
    {
        return;
    }
    else
    {
        fx_mgr::AddSpritesThisFrame( m_NActive );
    }

    f32 UniScale = pEffect->GetUniformScale();

    const texture* pDiffuse = pEffect->GetDiffuseTexture( EmitterDef.BitmapIndex );

    if( !pDiffuse )
        return;

    if( EmitterDef.Flags & SPE_SCALE_SPRITE_SIZE )
        UniScale *= GetUniformScale();

    const render::primitive_draw_desc Material =
        fx_CreateMaterial( *pDiffuse, EmitterDef.CombineMode, EmitterDef.ReadZ,
                           render::PRIMITIVE_SAMPLER_LINEAR_CLAMP );

    if( EmitterDef.Flags & SPE_VELOCITY_ORIENTED )
    {
        matrix4  LocalToWorld;
        matrix4* pLocalToWorld = NULL;
        matrix4  VelocityMatrix;

        VelocityMatrix.Identity();

        if( (EmitterDef.Flags & SPE_WORLD_SPACE) == 0 )
        {
            LocalToWorld = GetL2W( pEffect );
            pLocalToWorld = &LocalToWorld;
            VelocityMatrix.Setup( m_Scale, m_Rotate, vector3( 0.0f, 0.0f, 0.0f ) );
        }

        VERIFY( render::SubmitPrimitiveVelocityBillboards( Material,
                                                           m_NActive,
                                                           UniScale * 0.5f,
                                                           pLocalToWorld,
                                                           &VelocityMatrix,
                                                           m_pPositions,
                                                           m_pVelocities,
                                                           m_pColors,
                                                           m_pActiveIndices ) );
    }
    else
    {
        matrix4  LocalToWorld;
        matrix4* pLocalToWorld = NULL;
        if( (EmitterDef.Flags & SPE_WORLD_SPACE) == 0 )
        {
            LocalToWorld = GetL2W( pEffect );
            pLocalToWorld = &LocalToWorld;
        }

        VERIFY( render::SubmitPrimitiveBillboards( Material,
                                                   m_NActive,
                                                   UniScale * 0.5f,
                                                   pLocalToWorld,
                                                   m_pPositions,
                                                   m_pRotAndScales,
                                                   m_pColors,
                                                   m_pActiveIndices ) );
    }

    // *************** //
    // DEBUG RENDERING //
    // *************** //

#ifdef DEBUG_FX
    s32 i;
//    fx_sparticle* pParticle = (fx_sparticle*)(this+1);

    if( !FXDebug.ElementReserved )
        return;

    if( FXDebug.ElementSpriteCenter )
    {
        if( EmitterDef.Flags & SPE_WORLD_SPACE )
        {
            // WORLD MOVEMENT //

            for( i = 0; i < EmitterDef.NParticles; i++ )
            {
//                const fx_sparticle& P = pParticle[i];
                if( m_pPositions[i].GetIW() != 0x8000 )
                {
                    render::debug::Point( *(vector3*)&m_pPositions[i], XCOLOR_WHITE );
                }
            }
        }
        else
        {
            // LOCAL MOVEMENT //

            const matrix4& L2W = GetL2W( pEffect );

            for( i = 0; i < EmitterDef.NParticles; i++ )
            {
//                const fx_sparticle& P = pParticle[i];
                if( m_pPositions[i].GetIW() != 0x8000 )
                {
                    render::debug::Point( L2W * *(vector3*)&m_pPositions[i], XCOLOR_WHITE );
                }
            }
        }        
    }

    if( FXDebug.ElementWire )
    {
        if( EmitterDef.Flags & SPE_VELOCITY_ORIENTED )
        {
            // *** VELOCITY ORIENTED *** //
            const render::primitive_draw_desc wireMaterial( NULL,
                                                            render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                            render::PRIMITIVE_BLEND_OPAQUE,
                                                            render::PRIMITIVE_DEPTH_READ_ONLY,
                                                            render::PRIMITIVE_RASTER_WIREFRAME_NO_CULL,
                                                            render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);
            render::PrimitiveBatch wireBatch( wireMaterial );

            vector3 LineOfSight = eng_GetView()->GetViewZ();
            vector3 Velocity;
            vector3 Right;
            vector3 Up;
            vector3 Fore;
            vector3 Aft;

            const matrix4& PL2W = GetL2W( pEffect );
                  matrix4  VelocityToWorld = PL2W;
            VelocityToWorld.ClearTranslation();

            for( i = 0; i < EmitterDef.NParticles; i++ )
            {
//                const fx_sparticle& P = pParticle[i];

                if( m_pPositions[i].GetIW() != 0x8000 )
                {
                    f32 Scale  = m_pVelocities[i].GetW() * UniScale;
//                    f32 DeltaV = EmitterDef.Acceleration * P.Age;

                    Velocity         = *(vector3*)&m_pVelocities[i];

                    if( !(EmitterDef.Flags & SPE_WORLD_SPACE) )
                        Velocity = VelocityToWorld * Velocity;

                    Velocity.Normalize();

                    Right  = Velocity;
                    Up     = LineOfSight.Cross( Right );
                    Right *= Scale;
                    Up    *= Scale;

                    if( EmitterDef.Flags & SPE_WORLD_SPACE )
                    {
                        Fore   = *(vector3*)&m_pPositions[i] + Right;
                        Aft    = *(vector3*)&m_pPositions[i] - Right;
                    }
                    else
                    {
                        Fore   = PL2W * *(vector3*)&m_pPositions[i]; 
                        Aft    = Fore;              
                        Fore  += Right;             
                        Aft   -= Right;             
                    }

                    const vector3 positions[4] = { Fore - Up, Aft - Up, Aft + Up, Fore + Up };
                    const vector2 uvs[4] = { vector2( 0.0f, 0.0f ), vector2( 1.0f, 0.0f ),
                                             vector2( 1.0f, 1.0f ), vector2( 0.0f, 1.0f ) };
                    const xcolor colors[4] = { XCOLOR_BLUE, XCOLOR_BLUE, XCOLOR_BLUE, XCOLOR_BLUE };
                    wireBatch.AddQuad( positions, uvs, colors );
                }
            }
            matrix4 identity;
            identity.Identity();
            wireBatch.Submit( identity );
        }
        else
        {
            vector2 WH;
            vector2 UV0( 0, 0 );
            vector2 UV1( 1, 1 );

            const render::primitive_draw_desc wireMaterial( NULL,
                                                            render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                            render::PRIMITIVE_BLEND_OPAQUE,
                                                            render::PRIMITIVE_DEPTH_READ_WRITE,
                                                            render::PRIMITIVE_RASTER_WIREFRAME_NO_CULL,
                                                            render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);

            if( EmitterDef.Flags & SPE_WORLD_SPACE )
            {
                // *** SPIN ORIENTED / WORLD MOVEMENT *** //

                for( i = 0; i < EmitterDef.NParticles; i++ )
                {
//                    const fx_sparticle& P = pParticle[i];
                    if( m_pPositions[i].GetIW() != 0x8000 )
                    {
                        f32 S = m_pRotAndScales[i].Y * UniScale;
                        WH( S, S );
                        render::SubmitPrimitiveSprite( wireMaterial, *(vector3*)&m_pPositions[i], WH, UV0, UV1,
                                                       XCOLOR_BLUE, m_pRotAndScales[i].X );
                    }
                }
            }
            else
            {
                // *** SPIN ORIENTED / LOCAL MOVEMENT *** //

                const matrix4& L2W = GetL2W( pEffect );

                for( i = 0; i < EmitterDef.NParticles; i++ )
                {
//                    const fx_sparticle& P = pParticle[i];
                    if( m_pPositions[i].GetIW() != 0x8000 )
                    {
                        f32 S = m_pRotAndScales[i].Y * UniScale;
                        WH( S, S );
                        render::SubmitPrimitiveSprite( wireMaterial, L2W * *(vector3*)&m_pPositions[i], WH, UV0,
                                                       UV1, XCOLOR_BLUE, m_pRotAndScales[i].X );
                    }
                }
            } 
            
        }
    }

    if( FXDebug.ElementSpriteCount )
    {
        render::debug::Label( pEffect->GetL2W() * m_Translate, XCOLOR_YELLOW, "%d", m_NActive );
    }

    fx_element::SubmitRender( pEffect );
#endif // DEBUG_FX
}

//==============================================================================

xbool fx_spemitter::IsFinished( const fx_effect_base* pEffect ) const
{
    return( (m_NActive == 0) && fx_element::IsFinished( pEffect ) );
}

//==============================================================================

void fx_spemitter::Reset( void )
{
    s32                i;
    fx_edef_spemitter& EmitterDef = (fx_edef_spemitter&)(*m_pElementDef);

    fx_element::Reset();

    //
    // TO DO - Do a better job with this stuff in here.
    //

    m_NActive       = 0;
    m_PCursor       = 0;
    m_EmitClock     = 0;
    m_EmitCycle     = EmitterDef.LifeSpan;
    m_Emitting      = TRUE;
    m_PrevL2WReady  = FALSE;

//    fx_sparticle* pParticle = (fx_sparticle*)(this + 1);

    for( i = 0; i < EmitterDef.NParticles; i++ )
    {
//        fx_sparticle& P = pParticle[i];
        m_pPositions[i].GetIW() = 0x8000;
        m_pActivePositions[i] = -1;
    }
}

//==============================================================================

#undef new
REGISTER_FX_ELEMENT_CLASS( fx_spemitter, "SPEMITTER", SPEmitterMemoryFn );

//==============================================================================
