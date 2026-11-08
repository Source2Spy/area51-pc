//=========================================================================
// INCLUDES
//=========================================================================
#include "Render/PrimitiveDebug.hpp"
#include "Entropy.hpp"
#include "Pip.hpp"
#include "Camera.hpp"
#include "Render/Editor/EditorIcons.hpp"
#include "../MiscUtils/SimpleUtils.hpp"
#include "EventMgr/EventMgr.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"
#include "Render/Render.hpp"
#include "PlaySurface.hpp"

//=========================================================================
// OBJECT DESCRIPTION
//=========================================================================

static struct pip_desc : public object_desc
{
    pip_desc( void ) : object_desc( 
            object::TYPE_PIP, 
            "Pip", 
            "SCRIPT",
            object::ATTR_NEEDS_LOGIC_TIME,
            FLAGS_GENERIC_EDITOR_CREATE | FLAGS_TARGETS_OBJS |
            FLAGS_IS_DYNAMIC ) {}

    //---------------------------------------------------------------------
    virtual object* Create          ( void ) { return new pip; }

    //---------------------------------------------------------------------

#ifdef X_EDITOR

    virtual s32     OnEditorRender  ( object& Object ) const 
    { 
        object_desc::OnEditorRender( Object );

        // Render pip geometry?
        object_ptr<pip> pPip(Object.GetGuid());
        if ((pPip) && (pPip->GetType() == pip::TYPE_HUD))
        {
            //pPip->OnRender();
        }

        return static_cast<s32>( EditorIcon::PictureInPicture );
    }

#endif // X_EDITOR

} s_pip_Desc;

//=========================================================================

const object_desc& pip::GetTypeDesc( void ) const
{
    return s_pip_Desc;
}

//=========================================================================

const object_desc& pip::GetObjectType( void )
{
    return s_pip_Desc;
}
//=========================================================================
// FUNCTIONS
//=========================================================================

pip::pip( void )
{
    m_Width         = 256;
    m_Height        = 128;
    m_nTextureRefs  = 0;
    
    m_CameraGuid    = 0;
    m_Type          = TYPE_HUD;
    m_State         = STATE_INACTIVE;

    m_EarGuid       = 0 ;
    m_EarID         = 0 ;
    m_LastRenderFrameSerial = ~(u64)0;
}

//=============================================================================

void pip::OnEnumProp( prop_enum& List )
{
    // Call base class
    object::OnEnumProp( List );

    // Add properties
    m_RenderInst.OnEnumProp(List);

    List.PropEnumExternal( "RenderInst\\AnimFile", "Resource\0anim\0", "Resource Animation File", PROP_TYPE_MUST_ENUM );
    List.PropEnumHeader("Pip",                  "Properties for the picture-in-picture object.", 0 );
    List.PropEnumEnum  ("Pip\\Type",            "HUD\0WORLD\0", "Type of pip. HUD = Hud overlay type. WORLD = In world object.", PROP_TYPE_MUST_ENUM);
    List.PropEnumGuid  ("Pip\\CameraGuid",      "Guid of the camera to view from.", PROP_TYPE_EXPOSE);
    if (m_Type == TYPE_WORLD)
    {
        for (s32 i = 0; i < MAX_WORLD_OBJECTS; i++)
        {
            List.PropEnumHeader(xfs("Pip\\WorldObject[%d]", i), "World object info.", 0 );
            List.PropEnumGuid  (xfs("Pip\\WorldObject[%d]\\Guid", i), "Guid of world object to control.", 0 );
        }
    }
}

//=============================================================================

xbool pip::OnProperty( prop_query& I )
{
    // Call base class
    if (object::OnProperty(I))
        return TRUE;

    // Render inst?
    if (m_RenderInst.OnProperty(I))
        return TRUE;

    // AnimFile?
    if( I.IsVar( "RenderInst\\AnimFile" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarExternal( m_hAnimGroup.GetName(), RESOURCE_NAME_SIZE );
        }
        else            
        {
            m_hAnimGroup.SetName( I.GetVarExternal() );

            // If we can load this animgoup then we need to extract some info
            if (m_hAnimGroup.GetPointer() )
                OnInit();
        }
        return TRUE;
    } 

    // Type
    if( I.IsVar("Pip\\Type" ) )
    {
        if( I.IsRead() )
        {
            switch(m_Type)
            {
                case TYPE_HUD:   I.SetVarEnum("HUD");   break;
                case TYPE_WORLD: I.SetVarEnum("WORLD"); break;
            }
        }
        else
        {
            if (!x_stricmp(I.GetVarEnum(), "HUD"))
                m_Type = TYPE_HUD;
            else if (!x_stricmp(I.GetVarEnum(), "WORLD"))
                m_Type = TYPE_WORLD;
        }
        return TRUE;
    }

    // Camera guid
    if (I.VarGUID("Pip\\CameraGuid", m_CameraGuid))
        return TRUE;
    
    // World object?
    if (I.IsSimilarPath("Pip\\WorldObject["))
    {
        s32 Index = I.GetIndex(0);

        // Guid?
        if (I.VarGUID("Pip\\WorldObject[]\\Guid", m_WorldObjectGuids[Index]))
            return TRUE;
    }
    
    return FALSE;
}

//=========================================================================

void pip::OnInit( void )
{
    // Initialize animation
    m_AnimPlayer.SetAnimGroup(m_hAnimGroup);
    m_LastRenderFrameSerial = ~(u64)0;

    // Create the PIP render target.
    m_Width  = 256;
    m_Height = 128;

    if( !m_RenderTarget.Create( m_Width, m_Height ) )
    {
        x_DebugMsg( "pip: Failed to create render target\n" );
    }

    // Reset state
    SetupState(STATE_INACTIVE);
}

//=========================================================================

void pip::OnKill( void )
{
    g_AudioMgr.CancelRequest( m_pEarRequest );
    // Turn off pip texture
    ActivatePipTexture(FALSE);

    m_RenderTarget.Destroy();

    // Destroy ear?
    if (m_EarID)
    {
        g_AudioMgr.DestroyEar(m_EarID) ;
        m_EarID   = 0 ;
        m_EarGuid = 0 ;
    }
}

//=========================================================================

void pip::OnRender( void )
{
    s32 i;

    X_PROFILE_SCOPE_CATEGORY( "Context", "pip::OnRender" );

    // Should only get here for the hud render!
    ASSERT(m_Type == pip::TYPE_HUD);

    // Get active view
    const view* pView = eng_GetView();

    // Get viewport
    irect Viewport;
    pView->GetViewport(Viewport.l, Viewport.t, Viewport.r, Viewport.b);

    // Clear all of z buffer
    render::SetDepthRect( Viewport, 1.0f );
    // Compute L2W to be infront of view
    matrix4 L2W;
    L2W.Identity();
    L2W.RotateY(0);
    L2W.SetTranslation(vector3(100, 80, 400));
    L2W = pView->GetV2W() * L2W;

    // Render geometry?
    geom* pGeom = m_RenderInst.GetGeom();
    if (pGeom)
    {
        // Compute flags
        u32 Flags = (GetFlagBits() & object::FLAG_CHECK_PLANES) ? render::CLIPPED : 0;
#ifdef X_EDITOR
        if ( GetAttrBits() & ATTR_EDITOR_SELECTED )
            Flags |= render::WIREFRAME;
#endif

        // Compute LOD mask
        u64 LODMask = m_RenderInst.GetLODMask( GetL2W() );
        if (LODMask == 0)
            return;

        // Setup ambient color
        xcolor Ambient(255,255,255,255);

        // Use animation?
        if (m_hAnimGroup.IsLoaded())
        {
            // Compute matrices
            m_AnimPlayer.SetL2W(L2W);
            const matrix4* pMatrices = m_AnimPlayer.GetBoneL2Ws();

            // Render
            m_RenderInst.Render( pMatrices, pMatrices, pGeom->m_nBones, Flags, LODMask, Ambient );
        }
        else
        {
            // Allocate matrices
            matrix4* pMatrices = (matrix4*)smem_BufferAlloc(pGeom->m_nBones * sizeof(matrix4));
            if (!pMatrices)
                return;

            // Use identity
            for (i = 0; i < pGeom->m_nBones; i++)
                pMatrices[i] = L2W;

            // Render
            m_RenderInst.Render( pMatrices, pMatrices, pGeom->m_nBones, Flags, LODMask, Ambient );
        }
    }
    else
    {
#if !defined( CONFIG_RETAIL )
        render::debug::Box( GetBBox() );
#endif // !defined( CONFIG_RETAIL )
    }
}

//=========================================================================

void pip::OnAdvanceSimulation( f32 DeltaTime )
{
    // Advance animation
    if (m_hAnimGroup.IsLoaded())
    {
        m_AnimPlayer.Advance(DeltaTime);
        g_EventMgr.HandleSuperEvents(m_AnimPlayer, this);
    }

    // Advance state
    AdvanceState(DeltaTime);

    // Update ear
    UpdateEar() ;
}

//=========================================================================

void pip::OnActivate( xbool bFlag )
{
    // Activate?
    if (bFlag)
        SetupState(STATE_ENTER);
    else
        SetupState(STATE_EXIT);
}

//=========================================================================

void pip::RenderView( void )
{
    u64 const FrameSerial = eng_GetCurrentFrameSerial();
    if( m_LastRenderFrameSerial == FrameSerial )
        return;

    // Get camera
    object_ptr<camera> pCamera(m_CameraGuid);
    if (pCamera)
    {
        // Setup viewport
        irect Viewport;

        if( !m_RenderTarget.bValid )
        {
            if( !m_RenderTarget.Create( m_Width, m_Height ) )
                return;
            ActivatePipTexture( TRUE );
        }

        f32 const RenderScale = render::GetRenderPipeline().GetRenderScale();
        u32 const SceneWidth = MAX( 1u, static_cast<u32>( x_ceil( static_cast<f32>( m_Width ) * RenderScale ) ) );
        u32 const SceneHeight = MAX( 1u, static_cast<u32>( x_ceil( static_cast<f32>( m_Height ) * RenderScale ) ) );
        if( (m_RenderTarget.DepthTarget.Desc.Width != SceneWidth) ||
            (m_RenderTarget.DepthTarget.Desc.Height != SceneHeight) )
        {
            ActivatePipTexture( FALSE );
            if( !m_RenderTarget.Create( m_Width, m_Height ) )
            {
                return;
            }
            ActivatePipTexture( TRUE );
        }

        Viewport.l = 0;
        Viewport.t = 0;
        Viewport.r = m_Width;
        Viewport.b = m_Height;

        m_LastRenderFrameSerial = FrameSerial;
        render::RenderScopeDesc Scope;
        Scope.pColorTarget = &m_RenderTarget.ColorTarget;
        Scope.pDepthTarget = &m_RenderTarget.DepthTarget;
        Scope.Purpose = render::RENDER_VIEW_SCENE_CAPTURE;
        Scope.ScopeId = m_RenderTarget.ScopeId;
        Scope.SampleCount = 1;
        Scope.EnableShadows = FALSE;
        Scope.EnableTemporalHistory = FALSE;
        Scope.EnablePostProcessing = FALSE;
        pCamera->RenderView( Viewport, Scope );
    }
}

//=========================================================================

void pip::AddTextureRefs( object* pObject )
{ 
    // No object?
    if (!pObject)
        return;

    // Lookup inst
    render_inst* pRenderInst = pObject->GetRenderInstPtr();
    if (!pRenderInst)
        return;

    // Lookup geometry
    const geom* pGeom = pObject->GetGeomPtr();
    if (!pGeom)
        return;

    // Search for a mesh names "PIP"
    s32 iSubMesh = 0;
    for (s32 i = 0; i < pGeom->m_nMeshes; i++)
    {
        // Is this a pip mesh?
        if (x_stricmp(pGeom->GetMeshName( i ), "PIP") == 0)
        {
            // Use first sub-mesh
            iSubMesh = pGeom->m_pMesh[i].iSubMesh;
            break;
        }
    }

    // Force the render library to use the top mip because the PIP target has one level.
    pGeom->m_pSubMesh[iSubMesh].WorldPixelSize = 1000.0f;

    // Lookup material on this submesh
    material& Material = render::GetMaterial(pRenderInst->GetInst(), iSubMesh);

    // Lookup texture handle
    texture::handle hTexture = Material.m_DiffuseMap;

    // Lookup texture
    texture* pTexture = hTexture.GetPointer();
    if (!pTexture)
    {
        return;
    }

    // Setup next texture ref
    ASSERT(m_nTextureRefs <= MAX_TEXTURE_REFS);
    if( m_nTextureRefs >= MAX_TEXTURE_REFS )
    {
        return;
    }

    texture_ref& Ref = m_TextureRefs[m_nTextureRefs++];
    Ref.m_hTexture = hTexture;
    Ref.m_pOriginalResourceOverride = pTexture->GetShaderResourceOverride();
}

//=========================================================================

void pip::InitTextureRefs( void )
{
    // Clear texture ref list
    ASSERT(m_nTextureRefs == 0);

    // Add hud object?
    if (m_Type == TYPE_HUD)
        AddTextureRefs(this);

    // Using world type?
    if (m_Type == TYPE_WORLD)
    {
        // Check all objects
        for (s32 i = 0; i < MAX_WORLD_OBJECTS; i++)
        {
            // Link to object textures
            object* pObject = g_ObjMgr.GetObjectByGuid(m_WorldObjectGuids[i]);
            if( pObject)
                AddTextureRefs( pObject );
        }
    }
}

//=========================================================================

void pip::ActivatePipTexture( xbool bEnable )
{
    // Lookup textures?
    if (m_nTextureRefs == 0)
        InitTextureRefs();

    if( m_nTextureRefs == 0 )
        return;

    const shader_resource* pPipResource = NULL;
    if( bEnable )
    {
        if( !m_RenderTarget.bValid ||
            !rtarget_HasShaderResource( m_RenderTarget.ColorTarget ) )
        {
            return;
        }

        pPipResource = rtarget_GetShaderResource( m_RenderTarget.ColorTarget );
        if( !pPipResource )
            return;
    }

    // Update all texture refs
    for (s32 i = 0; i < m_nTextureRefs; i++)
    {
        // Lookup texture ref
        texture_ref& Ref = m_TextureRefs[i]; 

        // Lookup texture
        texture* pTexture = Ref.m_hTexture.GetPointer();
        ASSERT(pTexture);

        // Turn on?
        if (bEnable)
        {
            pTexture->SetShaderResourceOverride( pPipResource );
        }
        else
        {
            pTexture->SetShaderResourceOverride( Ref.m_pOriginalResourceOverride );
        }
    }
}

//=========================================================================

void pip::SetupState( state State )
{
    // Record and setup new state
    m_State = State;

    switch(State)
    {
        case STATE_INACTIVE:
            // No logic needed anymore...
            SetAttrBits(GetAttrBits() & ~ATTR_NEEDS_LOGIC_TIME);
            ActivatePipTexture(FALSE);
            break;

        case STATE_ENTER:
        {
            // Only one pip at a time allowed so turn off all other pips
            select_slot_iterator SlotIter;
            g_ObjMgr.SelectByAttribute( object::ATTR_ALL, object::TYPE_PIP );
            for( SlotIter.Begin(); !SlotIter.AtEnd(); SlotIter.Next() )
            {
                // Get pip
                pip* pPip = (pip*)SlotIter.Get();
                ASSERT( pPip );

                // Is this pip about to enter or active?
                if(( pPip != this ) && (( pPip->GetState() == STATE_ENTER) || ( pPip->GetState() == STATE_ACTIVE )))
                {
                    // Turn off now!
                    pPip->SetupState(STATE_EXIT);
                }
            }
            SlotIter.End();

            // Need logic time
            SetAttrBits(GetAttrBits() | ATTR_NEEDS_LOGIC_TIME);

            // Start animation
            if (m_hAnimGroup.IsLoaded())
                m_AnimPlayer.SetAnim("ENTER");

            // Turn on texture
            ActivatePipTexture(TRUE);
        }
        break;

        case STATE_ACTIVE:
            // Nothing to do...
            break;

        case STATE_EXIT:
            g_AudioMgr.CancelRequest( m_pEarRequest );
            g_AudioMgr.DestroyEar(m_EarID) ;
            m_EarID = 0 ;
            // Start animation
            if (m_hAnimGroup.IsLoaded())
                m_AnimPlayer.SetAnim("EXIT");
            break;
    }
}

//=========================================================================

void pip::AdvanceState( f32 DeltaTime )
{
    (void)DeltaTime;

    // Advance state
    switch(m_State)
    {
        case STATE_INACTIVE:
            // Nothing to do...
            break;

        case STATE_ENTER:
            // If done, goto active state
            if ( (m_hAnimGroup.IsLoaded() == FALSE) || (m_AnimPlayer.IsAtEnd()) )
                SetupState(STATE_ACTIVE);
            break;

        case STATE_ACTIVE:
            // Nothing to do...
            break;

        case STATE_EXIT:
            // If done, goto sleep
            if ( (m_hAnimGroup.IsLoaded() == FALSE) || (m_AnimPlayer.IsAtEnd()) )
                SetupState(STATE_INACTIVE);
            break;
    }
}

//=========================================================================

void pip::UpdateEar( void )
{
    // Nothing to do if inactive
    if( m_State == STATE_INACTIVE )
    {
        g_AudioMgr.CancelRequest( m_pEarRequest );
        // Destroy ear?
        if (m_EarID)
        {
            g_AudioMgr.DestroyEar(m_EarID) ;
            m_EarID   = 0 ;
        }
        return ;
    }

    // Must have a camera before ear is used
    object_ptr<camera> pCamera( m_CameraGuid );
    if( pCamera )
    {
        // Does ear need to be created?
        if( !m_EarID )
        {
            // Create if pip is entering or active
            if( ( m_State == STATE_ENTER ) || ( m_State == STATE_ACTIVE ) )
            {
                if( !m_pEarRequest )
                {
                    matrix4 W2V = pCamera->GetL2W();
                    W2V.InvertRT();
                    m_pEarRequest = g_AudioMgr.RequestCreateEar( W2V, pCamera->GetPosition(),
                                                               pCamera->GetZone1(), pCamera->GetPipEarVolume() );
                }
                g_AudioMgr.PollCreateEar( m_pEarRequest, m_EarID );
            }
        }

        // Update ear?
        if( m_EarID )
        {
            // Compute camera world -> view
            matrix4 W2V = pCamera->GetL2W();
            W2V.InvertRT();

            // Update ear
            g_AudioMgr.SetEar(m_EarID, 
                                W2V, 
                                pCamera->GetPosition(),
                                pCamera->GetZone1(),
                                pCamera->GetPipEarVolume());
        }
    }
}

//=========================================================================

render_inst* pip::GetRenderInstPtr( void )
{
    // Geometry is only used for HUD object pips
    if( m_Type == TYPE_HUD )
        return &m_RenderInst;
    else
        return NULL;
}

//=========================================================================

anim_group::handle* pip::GetAnimGroupHandlePtr ( void )
{
    // Animation is only used for HUD object pips
    if( m_Type == TYPE_HUD )
        return &m_hAnimGroup;
    else
        return NULL;
}

//=========================================================================
