//=============================================================================
//
//  RigidInst.cpp
//
//=============================================================================

#include "Entropy.hpp"
#include "Objects/Render/RigidInst.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

//=============================================================================
// LOADER FOR THE RIGID COLOR RESOURCE
//=============================================================================

static struct rigid_color : public rsc_loader
{
    //-------------------------------------------------------------------------

    rigid_color( void ) : rsc_loader( "RIGID COLOR", ".rigidcolor" ) {}

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* FP )
    {
        MEMORY_OWNER( "RIGID COLOR DATA" );

        RigidColorData* pData = new RigidColorData;
        xstring           Error;
        if( !rigid_color_file::Load( FP, *pData, Error ) )
        {
            delete pData;
            x_DebugMsg( "RIGIDCOLOR: load failed: %s\n",
                        (const char*)Error );
            x_throw( (const char*)Error );
        }

        return( pData );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        return( pData );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        RigidColorData* pRigidColor = (RigidColorData*)pData;
        ASSERT( pRigidColor );
        delete pRigidColor;
    }

} s_Rigid_Color_Loader;

//=============================================================================
//  FUNCTIONS
//=============================================================================

rigid_inst::rigid_inst( void ) :
    render_inst(),
    m_pRigidColor   (NULL),
    m_nColors       (0),
    m_iColor        (0)
{
}

//=============================================================================

rigid_inst::~rigid_inst( void )
{
    if ( m_hInst.IsNonNull() )
    {
        render::UnregisterRigidInstance( m_hInst );
    }
}

//=============================================================================

s32 rigid_inst::GetNumColors( void ) const
{
    return( m_nColors );
}

//=============================================================================

const u32* rigid_inst::GetColorTable( asset_platform PlatformType ) const
{
    ASSERT( (PlatformType == ASSET_PLATFORM_XBOX) ||
            (PlatformType == ASSET_PLATFORM_DESKTOP) );

    if( (PlatformType != ASSET_PLATFORM_XBOX) &&
        (PlatformType != ASSET_PLATFORM_DESKTOP) )
    {
        return( NULL );
    }

    return( GetColorTable() );
}

//=============================================================================

const u32* rigid_inst::GetColorTable( void ) const
{
    if( !m_pRigidColor )
        return( NULL );

    return( m_pRigidColor + m_iColor );
}

//=============================================================================

void rigid_inst::SetColorTable( const u32* pColorTable, s32 iColor, s32 nColors )
{
    ASSERT( iColor >= 0 );
    ASSERT( nColors >= 0 );

    m_pRigidColor = pColorTable;
    m_iColor      = iColor;
    m_nColors     = nColors;
}

//=============================================================================

void rigid_inst::LoadColorTable( const char* pFileName )
{
    rhandle<RigidColorData> hRigidColor;
    hRigidColor.SetName( pFileName );

    RigidColorData* pInfo = hRigidColor.GetPointer();
    if( !pInfo )
    {
        m_pRigidColor = NULL;
        return;
    }

    const s32 TableCount = pInfo->Colors.GetCount();
    if( (m_iColor < 0) ||
        (m_nColors < 0) ||
        (m_iColor > TableCount) ||
        (m_nColors > (TableCount - m_iColor)) )
    {
        m_pRigidColor = NULL;
        x_throw( xfs( "Rigid color range [%d, %d) exceeds table size %d.",
                      m_iColor,
                      m_iColor + m_nColors,
                      TableCount ) );
    }

    m_pRigidColor = pInfo->Colors.GetPtr();
}

//=============================================================================

const char* rigid_inst::GetRigidGeomName( void ) const
{
    return( m_hRigidGeom.GetName() );
}

//=============================================================================

void rigid_inst::RenderShadowCast( const matrix4* pL2W,
                                   u32            Flags,
                                   u64            ProjMask )
{
    RenderShadowCast( pL2W, Flags, ProjMask, m_VMeshMask.VMeshMask );
}

//=============================================================================

void rigid_inst::RenderShadowCast( const matrix4* pL2W,
                                   u32            Flags,
                                   u64            ProjMask,
                                   u32            VMeshMask )
{
    if( !pL2W || !m_hInst.IsNonNull() || ( m_Alpha == 0 ) )
        return;

    rigid_geom* pRigidGeom = GetRigidGeom();
    if( !pRigidGeom )
        return;

    u64 LODMask = GetLODMask( *pL2W, VMeshMask );
    if( LODMask == 0 )
        return;

    (void)Flags;

    // add the shadow
    render::GetRenderPipeline().GetShadowSubmission().AddRigidCaster( m_hInst,
                            pL2W,
                            LODMask,
                            ProjMask );
}

//=============================================================================

void rigid_inst::Render( const matrix4* pL2W, u32 Flags, u64 Mask, xcolor Ambient )
{
    if ( m_Alpha == 0 )
        return;

    if ( m_Alpha != 255 )
        Flags |= render::FADING_ALPHA;

    // Add a Rigid Instance
    render::GetRenderPipeline().GetSubmission().AddRigidInstance( m_hInst,
                              GetColorTable(),
                              pL2W,
                              Mask,
                              Flags,
                              m_Alpha,
                              render::RENDER_CONTRIBUTION_DEFAULT,
                              Ambient );
}

//=============================================================================

void rigid_inst::Render( const matrix4* pL2W, u32 Flags, u64 Mask, u8 Alpha, xcolor Ambient )
{
    if ( Alpha == 0 )
        return;

    if ( Alpha != 255 )
        Flags |= render::FADING_ALPHA;

    // Add a Rigid Instance
    render::GetRenderPipeline().GetSubmission().AddRigidInstance( m_hInst,
        GetColorTable(),
        pL2W,
        Mask,
        Flags,
        Alpha,
        render::RENDER_CONTRIBUTION_DEFAULT,
        Ambient );
}

//=============================================================================

void rigid_inst::Render( const matrix4* pL2W, u32 Flags, u32 VTextureMask, s32 Alpha, xcolor Ambient )
{
    if ( Alpha == 0 )
        return;

    if ( Alpha != 255 )
        Flags |= render::FADING_ALPHA;

    // Add a Rigid Instance
    render::GetRenderPipeline().GetSubmission().AddRigidInstance( m_hInst,
        GetColorTable(),
        pL2W,
        GetLODMask( *pL2W ),
        VTextureMask,
        Flags,
        Alpha,
        render::RENDER_CONTRIBUTION_DEFAULT,
        Ambient );
}

//=============================================================================

void rigid_inst::Render( const matrix4* pL2W, u32 Flags, xcolor Ambient )
{
    // Add a Rigid Instance
    Render( pL2W, Flags, GetLODMask( *pL2W ), Ambient );
}

//=============================================================================

void rigid_inst::OnEnumProp( prop_enum& List )
{
    // Important: The Header and External MUST be enumerated first!
    List.PropEnumHeader  ( "RenderInst", "Render Instance", 0 );
    List.PropEnumExternal( "RenderInst\\File", "Resource\0rigidgeom\0", "Resource File", PROP_TYPE_MUST_ENUM );

    render_inst::OnEnumProp( List );

    List.PropEnumInt( "RenderInst\\iColor",  "iColor",  PROP_TYPE_INT | PROP_TYPE_DONT_SHOW );
    List.PropEnumInt( "RenderInst\\nColors", "nColors", PROP_TYPE_INT | PROP_TYPE_DONT_SHOW );
}

//=============================================================================

xbool rigid_inst::OnProperty( prop_query& I )
{
static u32 Count = 0;
Count ++;
    if( render_inst::OnProperty( I ) )
        return( TRUE );

    // External
    if( I.IsVar( "RenderInst\\File" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarExternal( m_hRigidGeom.GetName(), RESOURCE_NAME_SIZE );
        }
        else
        {
            // Get the FileName
            const char* pString = I.GetVarExternal();
            ASSERT( pString );

            // Clear?
            if( x_strcmp( pString, "<null>" ) == 0 )
            {
                SetUpRigidGeom( "" );
            }
            else if( pString[0] )
            {
                // Setup
                SetUpRigidGeom( pString );
            }

            // if the filename has changed, this means we need to reset the vmesh mask
#if defined(X_EDITOR)
            m_VMeshMask.nVMeshes  = 0;
#endif
            m_VMeshMask.VMeshMask = 0xffffffff;
        }
        return( TRUE );
    }

    if( I.VarInt( "RenderInst\\iColor", m_iColor ) )
        return( TRUE );

    if( I.VarInt( "RenderInst\\nColors", m_nColors ) )
        return( TRUE );

    return( FALSE );
}

//=============================================================================


xbool rigid_inst::SetUpRigidGeom                ( const char* pFileName )
{

    if( m_hInst.IsNonNull() )
    {
        render::UnregisterRigidInstance( m_hInst );
        m_hInst = HNULL;
    }

    m_hRigidGeom.SetName( pFileName );
    rigid_geom* pRigidGeom = m_hRigidGeom.GetPointer();

    if( pRigidGeom )
    {
        // Register the instance with the Render Manager
        m_hInst = render::RegisterRigidInstance( *pRigidGeom );
        return TRUE;

    }

    return FALSE;
}

//=============================================================================

#ifdef X_EDITOR
void rigid_inst::UnregiserInst( void )
{
    if ( m_hInst.IsNonNull() )
    {
        render::UnregisterRigidInstance( m_hInst );
        m_hInst = HNULL;
    }
}
#endif // X_EDITOR

//=============================================================================

#ifdef X_EDITOR
void rigid_inst::RegisterInst( void )
{
    rigid_geom* pGeom = m_hRigidGeom.GetPointer();
    if ( pGeom )
    {
        m_hInst = render::RegisterRigidInstance( *pGeom );
    }
}
#endif // X_EDITOR

//=============================================================================

// EOF
