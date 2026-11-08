//=============================================================================
//
//  Desktop-specific routines
//
//=============================================================================

//=============================================================================
//  HELPER FUNCTIONS
//=============================================================================

f32 Spd = 2000.0f;

void FreeCam( f32 DeltaTime )
{
    view& View = g_View;

    f32 Move = Spd  * DeltaTime;
    f32 Rot  = R_10 * DeltaTime;
    f32 X    = 0.0f;
    f32 Y    = 0.0f;
    f32 Z    = 0.0f;

    input_snapshot const& FrameInput = g_Input.GetFrameSnapshot();

    if( FrameInput.IsPressed( INPUT_MOUSE_BTN_L ) ) Move *= 4.0f;
    if( FrameInput.IsPressed( INPUT_MOUSE_BTN_R ) ) Move *= 0.2f;
    
    if( FrameInput.IsPressed( INPUT_KBD_A ) ) X =  Move;
    if( FrameInput.IsPressed( INPUT_KBD_D ) ) X = -Move;
    if( FrameInput.IsPressed( INPUT_KBD_Q ) ) Y =  Move;
    if( FrameInput.IsPressed( INPUT_KBD_Z ) ) Y = -Move;
    if( FrameInput.IsPressed( INPUT_KBD_W ) ) Z =  Move;
    if( FrameInput.IsPressed( INPUT_KBD_S ) ) Z = -Move;
    
    View.Translate( vector3(    X, 0.0f,    Z ), view::VIEW  );
    View.Translate( vector3( 0.0f,    Y, 0.0f ), view::WORLD );
    
    radian Pitch, Yaw;
    View.GetPitchYaw( Pitch, Yaw );
    
    Pitch += (f32)g_Input.GetFrameMouseDeltaY() * Rot;
    Yaw   -= (f32)g_Input.GetFrameMouseDeltaX() * Rot;
    View.SetRotation( radian3( Pitch, Yaw, R_0 ) );
    
    // Move the player.
    player* pPlayer = SMP_UTIL_GetActivePlayer();
    if ( pPlayer )
    {
        pPlayer->OnMoveFreeCam( View );
    }    
}

//=============================================================================
//  IMPLEMENTATION
//=============================================================================

xbool HandleInputPlatform( f32 DeltaTime )
{
    static s32 s_DisplayStats = 0;
    static s32 s_DisplayMode  = 0;

    if( g_FreeCam == FALSE )
        return( TRUE );

    FreeCam( DeltaTime );

    input_snapshot const& FrameInput = g_Input.GetFrameSnapshot();
    if( FrameInput.IsPressed ( INPUT_KBD_LSHIFT ) &&
        FrameInput.WasPressed( INPUT_KBD_C ) )
        SaveCamera();

#if ENABLE_RENDER_STATS
    if( FrameInput.IsPressed ( INPUT_KBD_LSHIFT ) &&
        FrameInput.WasPressed( INPUT_KBD_P ) )
    {
        switch( s_DisplayStats )
        {
            case 0 : s_DisplayMode = render::stats::TO_SCREEN;
                     break;
            
            case 1 : s_DisplayMode = render::stats::TO_SCREEN | render::stats::VERBOSE;
                     break;
        
            case 2 : render::GetStats().ClearAll();
                     s_DisplayMode = 0;
                     break;
        }
        
        s_DisplayStats++;
        if( s_DisplayStats > 2 )
            s_DisplayStats = 0;
    }
    
    render::GetStats().Print( s_DisplayMode );
#endif //ENABLE_RENDER_STATS

    return( TRUE );
}

//=============================================================================

void PrintStatsPlatform( void )
{
}

//=============================================================================
