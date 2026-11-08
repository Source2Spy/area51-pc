// MainFrm.cpp : implementation of the CMainFrame class
//

#include "BaseStdAfx.h"
#include "Editor.h"

#include "MainFrm.h"
#include "BaseDocument.h"
#include "Editor.h"
#include "Outputbar.h"
#include "ProjectDoc.h"
#include "Project.hpp"
#include "../WorldEditor/WorldEditor.hpp"
#include "../WorldEditor/EditorDoc.h"
#include "../WinControls/StringEntryDlg.h"
#include "../EDRscDesc/EDRscDesc_Doc.h"
#include "Obj_mgr/obj_mgr.hpp"
#include "IOManager/io_mgr.hpp"
#include "../WorldEditor/Resource.h"
#include "Render/Render.hpp"

#include "x_log.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CEditorApp theApp;
CMainFrame* CMainFrame::s_pMainFrame = NULL;
extern char *g_CommandLine;

xbool g_bAutoLoad = FALSE;
xbool g_bAutoBuild = FALSE;
xbool g_ResetWindowLayout = FALSE;

extern s32 g_Changelist;
extern const char* g_pBuildDate;

/////////////////////////////////////////////////////////////////////////////
// CMainFrame
 
IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
    //{{AFX_MSG_MAP(CMainFrame)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_MESSAGE(WM_USER_MSG_UPDATE_STATUS_BAR, UpdateStatusBar)
    ON_MESSAGE(WM_USER_MSG_MODIFY_MENU_BAR, ModifyMenuBar)
    ON_UPDATE_COMMAND_UI(ID_ADD_RES_DESC_01, OnUpdateAdditionalViews)
    ON_COMMAND_RANGE(ID_ADD_RES_DESC_01, ID_ADD_RES_DESC_10, OnAdditionalView)
    ON_COMMAND(ID_MENU_FILE_CLOSE, OnMenuFileClose)
    ON_UPDATE_COMMAND_UI(ID_MENU_FILE_CLOSE, OnUpdateMenuFileClose)
    ON_COMMAND(ID_MENU_FILE_NEW, OnMenuFileNew)
    ON_UPDATE_COMMAND_UI(ID_MENU_FILE_NEW, OnUpdateMenuFileNew)
    ON_COMMAND(ID_MENU_FILE_OPEN, OnMenuFileOpen)
    ON_UPDATE_COMMAND_UI(ID_MENU_FILE_OPEN, OnUpdateMenuFileOpen)
    ON_COMMAND(ID_MENU_FILE_SAVE, OnMenuFileSave)
    ON_UPDATE_COMMAND_UI(ID_MENU_FILE_SAVE, OnUpdateMenuFileSave)
    ON_COMMAND(ID_EDITOR_SAVE_SETTINGS, OnEditorSaveSettings)
    ON_COMMAND(ID_MENU_FILE_EXPORT, OnMenuFileExport)
    ON_COMMAND(ID_MENU_FILE_EXPORT_MAX, OnMenuFileExportTo3DMax)
    ON_COMMAND(ID_MENU_BUILD_DFS, OnMenuFileBuildDFS)
    ON_UPDATE_COMMAND_UI(ID_MENU_FILE_EXPORT, OnUpdateMenuFileExport)
    ON_COMMAND(ID_MENU_FILE_IMPORT, OnMenuFileImport)
    ON_UPDATE_COMMAND_UI(ID_MENU_FILE_IMPORT, OnUpdateMenuFileImport)
    ON_UPDATE_COMMAND_UI(ID_MRU_LIST, OnUpdateMruList)
    ON_COMMAND_EX_RANGE(ID_MRU_LIST, ID_MRU_LIST_10, OnMenuFileMru)
    ON_COMMAND(ID_VIEW_RESETWINDOWS, OnViewResetWindows)
    ON_UPDATE_COMMAND_UI(ID_MAINTENANCE_ZONECLEANUP, OnUpdateCleanupZones)
    ON_COMMAND(ID_MAINTENANCE_ZONECLEANUP, OnCleanupZones)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

static UINT indicators[] =
{
    ID_SEPARATOR,           // status line indicator
    ID_INDICATOR_MODE,
    ID_INDICATOR_DATA,
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame() 
{
    // Recent files list
    m_pRecentFiles = NULL;
    m_pwndProgCtrl  = NULL;
    m_pwndProgCtrl2 = NULL;
}

//=========================================================================

CMainFrame::~CMainFrame()
{
    // Delete recent files list
    if( m_pRecentFiles )
        delete m_pRecentFiles;

    // Clear window layout from Registry?
    if( g_ResetWindowLayout )
    {
        // Legacy XT registry cleanup removed with the MFC migration.
    }

    s_pMainFrame = NULL;
}

//=========================================================================

static UINT arHiddenCmds[] =
{
    ID_EDIT_CUT,
    ID_EDIT_COPY,
    ID_EDIT_PASTE,
    ID_VIEW_STATUS_BAR
};

//=========================================================================

static void HandleDebugMsg( const char* pMessage )
{
    static FILE* File = NULL;

    if( File == NULL ) 
    {
        File = fopen( "c:\\A51EditorLog.txt", "wt" );
    }
    else
    {
        fprintf( File, "%s", pMessage );
        fflush( File );
    }

    OutputDebugString(pMessage);

    if( CMainFrame::s_pMainFrame )
    {
        COutputCtrl& OutputCtrl = CMainFrame::s_pMainFrame->m_wndOutputBar.m_DebugMsgOutput;

        OutputCtrl.SetSel( -1, -1 );
        OutputCtrl.ReplaceSel( pMessage, FALSE );
//        OutputCtrl.SendMessage(EM_SCROLLCARET, 0, 0);

        // Determine the number of visible lines in the control
        CRect r;
        OutputCtrl.GetRect( &r );
        s32 Height      = r.Height();
        s32 nVisible    = 0;
        s32 i           = OutputCtrl.GetFirstVisibleLine();
        s32 nLines      = OutputCtrl.GetLineCount();
        while( (i < nLines) && (OutputCtrl.GetCharPos( OutputCtrl.LineIndex( i ) ).y < Height) )
        {
            nVisible++;
            i++;
        }

        // Scroll the last line into view
        s32 LineCount  = OutputCtrl.GetLineCount() - nVisible;
        s32 FirstVisible = OutputCtrl.GetFirstVisibleLine();
        s32 Delta = LineCount - FirstVisible;
        OutputCtrl.LineScroll( Delta );

        // Don't send x_DebugMsg to the log system
        /*
        LOG_MESSAGE( "x_DebugMsg", pMessage );
        LOG_FLUSH();
        */
    }
}

//=========================================================================

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    //
    // Initialize the D3D Engine
    //
    {
        RECT Window;
        ::GetWindowRect( ::GetDesktopWindow(), &Window );
        eng_SetWindowHandle( GetSafeHwnd() );
        eng_SetParentWindowHandle( GetSafeHwnd() );
        eng_SetResolution( Window.right - Window.left, Window.bottom - Window.top );

        eng_Init();
//        theApp.InitGame();
    }


    // Create MRU files list
    ASSERT( m_pRecentFiles == NULL );
    m_pRecentFiles = new CRecentFileList( 0, _T("Recent Files" ), _T( "File%d" ), 9 );
    m_pRecentFiles->ReadList();

    //
    // Set the output bar as soon as possible
    //
    {
        // Create the output bar.
        if (!m_wndOutputBar.Create(this, IDW_OUTPUTBAR, _T("Output"),
            CSize(150, 150), CBRS_BOTTOM, 0)) //(AFX_IDW_TOOLBAR + 10) ))
        {
            TRACE0("Failed to create output dock window\n");
            return -1;        // fail to create
        }

        // Set the MSGDebug handler
        s_pMainFrame = this;
        x_DebugMsgSetFunction( HandleDebugMsg );
    }
/*
    if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
        | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, CRect(0, 0, 0, 0), (AFX_IDW_TOOLBAR + 2) ) ||
        !m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
    {
        TRACE0("Failed to create toolbar\n");
        return -1;      // fail to create
    }
*/

    if (!m_wndStatusBar.Create(this) ||
        !m_wndStatusBar.SetIndicators(indicators,
          sizeof(indicators)/sizeof(UINT)))
    {
        TRACE0("Failed to create status bar\n");
        return -1;      // fail to create
    }

    /*
    // Create the tray icon control.
    if (!m_trayIcon.Create(_T("Tray Icon Tool Tip"), NIF_MESSAGE|NIF_ICON|NIF_TIP,
        this, IDR_MAINFRAME))
    {
        TRACE0("Failed to create tray icon\n");
        return -1;
    }
    */


    // TODO: Delete these three lines if you don't want the toolbar to
    //  be dockable
    m_wndOutputBar.EnableDocking(CBRS_ALIGN_ANY);
    EnableDocking(CBRS_ALIGN_ANY);
//    DockControlBar(&m_wndToolBar);
    DockControlBar(&m_wndOutputBar,AFX_IDW_DOCKBAR_BOTTOM);

    // add the indicator to the status bar.
    int nIndex = m_wndStatusBar.CommandToIndex(ID_INDICATOR_MODE);
    ASSERT (nIndex != -1);
    {
        UINT nID = 0;
        UINT nStyle = 0;
        int  cxWidth = 0;
        m_wndStatusBar.GetPaneInfo(nIndex, nID, nStyle, cxWidth);
        m_wndStatusBar.SetPaneInfo(nIndex, nID, nStyle | SBPS_POPOUT, 150);
    }
    nIndex = m_wndStatusBar.CommandToIndex(ID_INDICATOR_DATA);
    ASSERT (nIndex != -1);
    {
        UINT nID = 0;
        UINT nStyle = 0;
        int  cxWidth = 0;
        m_wndStatusBar.GetPaneInfo(nIndex, nID, nStyle, cxWidth);
        m_wndStatusBar.SetPaneInfo(nIndex, nID, nStyle | SBPS_POPOUT, 300);
    }

    // Restore control bar postion.
    LoadBarState(_T("BarState - Main"));

    return 0;
}

//=========================================================================

void CMainFrame::OnUpdateAdditionalViews(CCmdUI* pCmdUI) 
{
    ASSERT_VALID( this );

    reg_editor* pEditor = reg_editor::m_pHead;
    int nCount = 0;

    // Get the menu from the pCmdUI
    CMenu* pMenu = pCmdUI->m_pMenu;
    if( pMenu == NULL )
        return;

    // Any additional views?
    if( pEditor == NULL )
    {
        // No additional views
        pCmdUI->SetText( "<no additional views>" );
        pCmdUI->Enable( FALSE );
        return;
    }

    int i;
    for( i=0; i<10; i++ )
        pCmdUI->m_pMenu->DeleteMenu( pCmdUI->m_nID + i, MF_BYCOMMAND );

    i = 0;
    while( pEditor )
    {
        // insert mnemonic + the view name
        TCHAR buf[10];
        int nItem = (i + 1) % _AFX_MRU_MAX_COUNT;

        // number &1 thru &9, then 1&0, then 11 thru ...
        if (nItem > 10)
            wsprintf(buf, _T("%d "), nItem);
        else if (nItem == 10)
            lstrcpy(buf, _T("1&0 "));
        else
            wsprintf(buf, _T("&%d "), nItem);

        pCmdUI->m_pMenu->InsertMenu(pCmdUI->m_nIndex++,
            MF_STRING | MF_BYPOSITION, pCmdUI->m_nID++,
            CString(buf) + CString(pEditor->m_pEditorName));

        i++;
        pEditor = pEditor->m_pNext;
    }

    // update end menu count
    pCmdUI->m_nIndex--; // point to last menu added
    pCmdUI->m_nIndexMax = pCmdUI->m_pMenu->GetMenuItemCount();

    pCmdUI->m_bEnableChanged = TRUE;    // all the added items are enabled
}

//=========================================================================

void CMainFrame::OnAdditionalView(UINT nID)
{
    int nViewIndex = nID - ID_ADD_RES_DESC_01;

    {
        reg_editor* pEditor = reg_editor::m_pHead;
        int nCount = 0;
        while( pEditor )
        {            
            if( nCount == nViewIndex )
            {
                pEditor->m_pTemplate->OpenDocumentFile( NULL );
                break;
            }
            nCount++;
            pEditor = pEditor->m_pNext;
        }
    }

}

//=========================================================================

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{    
    if( !CMDIFrameWnd::PreCreateWindow(cs) )
        return FALSE;
    // TODO: Modify the Window class or styles here by modifying
    //  the CREATESTRUCT cs

    cs.style |= WS_MAXIMIZE;

    // Helps to reduce screen flicker.
    cs.lpszClass = AfxRegisterWndClass(0, NULL, NULL,
        AfxGetApp()->LoadIcon(IDR_MAINFRAME));

    return TRUE;
}

//=========================================================================
// CMainFrame diagnostics
//=========================================================================

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
    CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
    CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

//=========================================================================
// CMainFrame message handlers
//=========================================================================

LRESULT CMainFrame::UpdateStatusBar(WPARAM wParam, LPARAM lParam)
{
    int nIndex = m_wndStatusBar.CommandToIndex(wParam);
    ASSERT (nIndex != -1);
    if (nIndex != -1)
    {
        CString strText((char*)lParam);
        m_wndStatusBar.SetPaneText(nIndex , strText);
        return 1;
    }
    return 0;
}

//=========================================================================

// WPARAM is an ID of the menu that is to be used. (Should have 1 submenu, index 0)
// (LOWORD)LPARAM is TRUE to add, FALSE to remove menu 
// (HIWORD)LPARAM then it is assumed to be the index of the menu to insert before
// if (HIWORD)LPARAM is -1 and (LOWORD)LPARAM is TRUE then the menu item will be inserted at the end of the list
// LRESULT is the index of the inserted item or -1 for failure
LRESULT CMainFrame::ModifyMenuBar(WPARAM wParam, LPARAM lParam)
{
    CMenu* pMenu = GetMenu();
    int iInsertLocation = -1;

    // Insert the menu.
    if( pMenu )
    {
        if (LOWORD(lParam))
        {
            // get the menu to add
            CMenu objectMenu;
            objectMenu.LoadMenu(wParam);
            CMenu* pPopupMenu = objectMenu.GetSubMenu(0);
            if (pPopupMenu)
            {
                // get its string - codejock seems to ignore this anyways, so doesn't matter too much
                CString strMenuItem;
                objectMenu.GetMenuString(0, strMenuItem, MF_BYPOSITION);
                
                // Add it
                int nPos = HIWORD(lParam);
                int nCount = pMenu->GetMenuItemCount();
                if (nPos >= nCount)
                {
                    nPos = -1;
                }

                if ((pMenu->InsertMenu(nPos, MF_BYPOSITION | MF_POPUP, 
                    (UINT)pPopupMenu->GetSafeHmenu(), strMenuItem)))
                {
                    if (nPos>=0)
                        iInsertLocation = nPos;
                    else
                        iInsertLocation = nCount;
                    
                }
                
                // remove from the other menu!
                objectMenu.RemoveMenu(0, MF_BYPOSITION);   
        
                DrawMenuBar();
            }
        }
        else
        {
            //remove the item
            UINT uPos = HIWORD(lParam);
            if (uPos < pMenu->GetMenuItemCount())
            {
                VERIFY(pMenu->RemoveMenu( uPos, MF_BYPOSITION ));
                DrawMenuBar();
                iInsertLocation = 0;
            }
        }

    }
    return iInsertLocation;
}

//=========================================================================

// This is the system tray notification handler.
// On a right-button click, it creates a popup menu
// On a left-button double click, it executes the default popup menu
// item which unhides the application.
LRESULT CMainFrame::OnTrayIconNotify(WPARAM wParam, LPARAM lParam) 
{
    switch (lParam)
    {
        case WM_RBUTTONUP:
        {
            CMenu menu;
            VERIFY(menu.LoadMenu(IDR_MAINFRAME));
            
            CMenu* pPopup = menu.GetSubMenu(0);
            ASSERT(pPopup != NULL);
            
            // Make ID_FILE_OPEN menu item the default (bold font)
            ::SetMenuDefaultItem(pPopup->m_hMenu, ID_FILE_OPEN, FALSE);

            ::SetForegroundWindow(m_hWnd);

            CPoint point;
            GetCursorPos( &point );
            pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                point.x, point.y, this);

            break;
        }

        case WM_LBUTTONDBLCLK:
            // TODO: Add your double click handler.
            break;
    }

    return 0;
}

//=========================================================================

void CMainFrame::AddLogo()
{
    // Legacy XT status-bar logo pane removed with the MFC migration.
}

//=========================================================================

void CMainFrame::RemoveLogo()
{
    // Legacy XT status-bar logo pane removed with the MFC migration.
}

//=========================================================================

void CMainFrame::OnClose() 
{
    // Close any open project

    // Save the user settings
    CEditorDoc* pEditorDoc = NULL;
    POSITION posEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
    if (posEditor)
    {
        pEditorDoc = (CEditorDoc*)m_pWorldEditDocTemplate->GetNextDoc( posEditor );
        if (pEditorDoc) 
            pEditorDoc->SaveUserSettings();
    }


    if( CloseProject() )
    {
        // Save recent file list
        if( m_pRecentFiles )
            m_pRecentFiles->WriteList();

        // Save control bar postion.
        SaveBarState(_T("BarState - Main"));

        RemoveProgress();
        CMDIFrameWnd::OnClose();

        //
        // Destroy the engine ( NOTE/TODO: This may need to go into the OnDestroy message ).
        //
        g_IoMgr.Kill();
        eng_ResetAfterException();
        eng_Sync();
        render::Kill();
        eng_Kill();
    }
}

//=========================================================================

BOOL CMainFrame::ShowWindowEx(int nCmdShow)
{
    ASSERT_VALID(this);

    return ShowWindow(nCmdShow);
}

//=========================================================================

#define ID_INDICATOR_PROG 1010
void CMainFrame::AddProgress()
{
    // Legacy XT status-bar hosted progress controls removed with the MFC migration.
}

//=========================================================================

void CMainFrame::RemoveProgress()
{
    if( m_pwndProgCtrl2 )
    {
        if( ::IsWindow( m_pwndProgCtrl2->GetSafeHwnd() ) )
            m_pwndProgCtrl2->DestroyWindow();
        delete m_pwndProgCtrl2;
        m_pwndProgCtrl2 = NULL;
    }

    if( m_pwndProgCtrl )
    {
        if( ::IsWindow( m_pwndProgCtrl->GetSafeHwnd() ) )
            m_pwndProgCtrl->DestroyWindow();
        delete m_pwndProgCtrl;
        m_pwndProgCtrl = NULL;
    }
}

//=========================================================================

bool CMainFrame::CloseProject( void )
{
    bool Success = false;

    // Get the first document position
    POSITION pos = m_pProjectDocTemplate->GetFirstDocPosition( );
    if (pos)
    {
        // Get the first document
        CProjectDoc* pDoc = (CProjectDoc*)m_pProjectDocTemplate->GetNextDoc( pos );

        // Attempt to close, this may prompt to save because of changes
        if (pDoc && pDoc->FileClose())
        {
            // Closing
            Success = true;

            // handle world editor
            CBaseDocument* pEditorDoc = NULL;
            POSITION posEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
            if (posEditor)
            {
                pEditorDoc = (CBaseDocument*)m_pWorldEditDocTemplate->GetNextDoc( posEditor );
                if (pEditorDoc) pEditorDoc->OnProjectClose();
            }

            // handle all registered editors
            reg_editor* pEditor = reg_editor::m_pHead;
            int nCount = 0;
            while( pEditor ) //for each editor
            {            
                posEditor = pEditor->m_pTemplate->GetFirstDocPosition( );
                while (posEditor) //for each document of that type
                {
                    pEditorDoc = (CBaseDocument*)pEditor->m_pTemplate->GetNextDoc( posEditor );
                    if (pEditorDoc) pEditorDoc->OnProjectClose();
                }
                pEditor = pEditor->m_pNext;
            }
        }
    }

    // Success closing?
    return Success;
}

void CMainFrame::OnMenuFileClose() 
{
    // Close the project
    if( CloseProject() )
    {
        CEditorDoc* pDoc = NULL;
        POSITION pEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
        if (pEditor)
        {
            pDoc = (CEditorDoc*)m_pWorldEditDocTemplate->GetNextDoc( pEditor );
            if (pDoc) 
                pDoc->SaveUserSettings();
        }
    }
}

//=========================================================================

void CMainFrame::OnUpdateMenuFileClose(CCmdUI* pCmdUI) 
{
    pCmdUI->Enable(g_Project.IsProjectOpen() );    
    pCmdUI->SetCheck(FALSE);    
}

//=========================================================================

void CMainFrame::OnMenuFileNew() 
{
   CEditorDoc* pDoc = NULL;
    POSITION pEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
    if (pEditor)
    {
        pDoc = (CEditorDoc*)m_pWorldEditDocTemplate->GetNextDoc( pEditor );
        if (pDoc) 
            pDoc->SaveUserSettings();
    }


    POSITION pos = m_pProjectDocTemplate->GetFirstDocPosition( );
    if (pos)
    {
        CProjectDoc* pDoc = (CProjectDoc*)m_pProjectDocTemplate->GetNextDoc( pos );
        if (pDoc->FileNew())
        {
            //we opened a new file

            //handle world editor
            CBaseDocument* pEditorDoc = NULL;
            POSITION posEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
            if (posEditor)
            {
                pEditorDoc = (CBaseDocument*)m_pWorldEditDocTemplate->GetNextDoc( posEditor );
                if (pEditorDoc) pEditorDoc->OnProjectNew();
            }

            //handle all registered editors
            reg_editor* pEditor = reg_editor::m_pHead;
            int nCount = 0;
            while( pEditor ) //for each editor
            {            
                posEditor = pEditor->m_pTemplate->GetFirstDocPosition( );
                while (posEditor) //for each document of that type
                {
                    pEditorDoc = (CBaseDocument*)pEditor->m_pTemplate->GetNextDoc( posEditor );
                    if (pEditorDoc) pEditorDoc->OnProjectNew();
                }
                pEditor = pEditor->m_pNext;
            }
        }
    }
}

//=========================================================================

void CMainFrame::OnUpdateMenuFileNew(CCmdUI* pCmdUI) 
{
//    pCmdUI->Enable( !g_Project.IsProjectOpen() );    
    pCmdUI->Enable(TRUE);    
    pCmdUI->SetCheck(FALSE);    
}

//=========================================================================

bool CMainFrame::DoFileOpen( const char* pFileName )
{
    bool Success = false;

    //extern xarray<xstring> g_AuditionPackages;
    //g_AuditionPackages.Clear();

    {
        X_PROFILE_SCOPE_CATEGORY( "Context", "CMainFrame::DoFileOpen" );

        POSITION pos = m_pProjectDocTemplate->GetFirstDocPosition( );
        if (pos)
        {
            CProjectDoc* pDoc = (CProjectDoc*)m_pProjectDocTemplate->GetNextDoc( pos );

            if( pDoc->HandleChangeSave() == FALSE )
                return false;

            CEditorDoc* pDocSave = NULL;
            POSITION pEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
            if (pEditor)
            {
                pDocSave = (CEditorDoc*)m_pWorldEditDocTemplate->GetNextDoc( pEditor );
                if (pDocSave) 
                    pDocSave->SaveUserSettings();
            }


            if( (!pFileName && pDoc->FileOpen()) ||
                ( pFileName && pDoc->LoadProject(pFileName)) )
            {
                // Add file it MRU list
                if( m_pRecentFiles )
                {
                    char FileName[4096];
                    g_Project.GetFileName( FileName );
                    m_pRecentFiles->Add( FileName );
                }

                //handle world editor
                CBaseDocument* pEditorDoc = NULL;
                POSITION posEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
                if (posEditor)
                {
                    pEditorDoc = (CBaseDocument*)m_pWorldEditDocTemplate->GetNextDoc( posEditor );
                    if (pEditorDoc) pEditorDoc->OnProjectOpen();
                }

                //handle all registered editors
                reg_editor* pEditor = reg_editor::m_pHead;
                int nCount = 0;
                while( pEditor ) //for each editor
                {            
                    posEditor = pEditor->m_pTemplate->GetFirstDocPosition( );
                    while (posEditor) //for each document of that type
                    {
                        pEditorDoc = (CBaseDocument*)pEditor->m_pTemplate->GetNextDoc( posEditor );
                        if (pEditorDoc) pEditorDoc->OnProjectOpen();
                    }
                    pEditor = pEditor->m_pNext;
                }

                Success = true;
            }
        }
    }

    return Success;
}

//=========================================================================

void CMainFrame::OnMenuFileOpen() 
{
    DoFileOpen();
}

//=========================================================================

void CMainFrame::OnUpdateMenuFileOpen(CCmdUI* pCmdUI) 
{
//    pCmdUI->Enable( !g_Project.IsProjectOpen() );    
    pCmdUI->Enable(TRUE);    
    pCmdUI->SetCheck(FALSE);    
}

//=========================================================================

void CMainFrame::OnMenuFileSave() 
{
    POSITION pos = m_pProjectDocTemplate->GetFirstDocPosition( );
    if (pos)
    {
        CProjectDoc* pDoc = (CProjectDoc*)m_pProjectDocTemplate->GetNextDoc( pos );
        if (pDoc->FileSave())
        {
            //we saved a project

            //handle world editor
            CBaseDocument* pEditorDoc = NULL;
            POSITION posEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
            if (posEditor)
            {
                pEditorDoc = (CBaseDocument*)m_pWorldEditDocTemplate->GetNextDoc( posEditor );
                if (pEditorDoc) pEditorDoc->OnProjectSave();
            }

            //handle all registered editors
            reg_editor* pEditor = reg_editor::m_pHead;
            int nCount = 0;
            while( pEditor ) //for each editor
            {            
                posEditor = pEditor->m_pTemplate->GetFirstDocPosition( );
                while (posEditor) //for each document of that type
                {
                    pEditorDoc = (CBaseDocument*)pEditor->m_pTemplate->GetNextDoc( posEditor );
                    if (pEditorDoc) pEditorDoc->OnProjectSave();
                }
                pEditor = pEditor->m_pNext;
            }
        }
    }
}

//=========================================================================

void CMainFrame::OnUpdateMenuFileSave(CCmdUI* pCmdUI) 
{
    pCmdUI->Enable(g_Project.IsProjectOpen() );    
    pCmdUI->SetCheck(FALSE);    
}

//=========================================================================

void CMainFrame::OnEditorSaveSettings() 
{
    theApp.SaveSettings();
}

//=========================================================================

void CMainFrame::OnMenuFileExportTo3DMax()
{
    CStringEntryDlg DlgBox;
    DlgBox.SetDisplayText( "Please enter the name of the Level to export." );
    CString strInputName(g_Project.GetName());
    strInputName += ".3DMax";
    DlgBox.SetEntryText(strInputName);
    if( DlgBox.DoModal() == IDOK )
    {
        CString strName = DlgBox.GetEntryText();
        if (!strName.IsEmpty())
        {
            CWaitCursor wc;

            g_WorldEditor.ExportToLevelTo3dMax(strName);
        }
    }
}

//=========================================================================

void CMainFrame::OnMenuFileExport() 
{
    CStringEntryDlg DlgBox;
    DlgBox.SetDisplayText( "Please enter the name of the Level to export." );
    CString strInputName(g_Project.GetName());
    strInputName += ".Level";
    DlgBox.SetEntryText(strInputName);
    if( DlgBox.DoModal() == IDOK )
    {
        CString strName = DlgBox.GetEntryText();
        if (!strName.IsEmpty())
        {
            CWaitCursor wc;
            if (!g_WorldEditor.ExportToLevel(strName))
            {
                if (!g_bAutoBuild)
                {
                    ::AfxMessageBox("Level could not be exported due to errors. Please refer to the log for exact problem descriptions.");
                }
                else
                {
                    LOG_ERROR("AutoBuild","Level could not be exported due to errors. Please refer to the log for exact problem descriptions.");
                }
            }
        }
    }
}

//=========================================================================

void CMainFrame::OnUpdateMenuFileExport(CCmdUI* pCmdUI) 
{
    pCmdUI->Enable(g_Project.IsProjectOpen() );    
    pCmdUI->SetCheck(FALSE);    
}

//=========================================================================

void CMainFrame::OnMenuFileImport() 
{
    //handle world editor
    CBaseDocument* pEditorDoc = NULL;
    POSITION posEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
    if (posEditor)
    {
        pEditorDoc = (CBaseDocument*)m_pWorldEditDocTemplate->GetNextDoc( posEditor );
        if (pEditorDoc) pEditorDoc->OnProjectImport();
    }

    POSITION pos = m_pProjectDocTemplate->GetFirstDocPosition( );
    if (pos)
    {
        CProjectDoc* pDoc = (CProjectDoc*)m_pProjectDocTemplate->GetNextDoc( pos );
        if (pDoc) pDoc->OnProjectRefresh();
    }

    //refresh all registered editors
    reg_editor* pEditor = reg_editor::m_pHead;
    int nCount = 0;
    while( pEditor ) //for each editor
    {            
        posEditor = pEditor->m_pTemplate->GetFirstDocPosition( );
        while (posEditor) //for each document of that type
        {
            pEditorDoc = (CBaseDocument*)pEditor->m_pTemplate->GetNextDoc( posEditor );
            if (pEditorDoc) 
            {
                pEditorDoc->OnProjectRefresh();
                pEditorDoc->OnProjectSave();
            }
        }
        pEditor = pEditor->m_pNext;
    }
}

//=========================================================================

void CMainFrame::OnUpdateMenuFileImport(CCmdUI* pCmdUI) 
{
    pCmdUI->Enable(g_Project.IsProjectOpen() );    
    pCmdUI->SetCheck(FALSE);    
}

//=========================================================================

void CMainFrame::OnMenuFileBuildDFS()
{
    CStringEntryDlg DlgBox;
    DlgBox.SetDisplayText( "Enter the name of the DFS file (PS2 has 8.3 limit)" );
    CString strInputName(g_Project.GetName());
    strInputName += ".dfs";
    DlgBox.SetEntryText(strInputName);
    if( DlgBox.DoModal() == IDOK )
    {
        CString strName = DlgBox.GetEntryText();
        if (!strName.IsEmpty())
        {
            CWaitCursor wc;

            long ID = ::MessageBox( NULL, xfs("Export to DFS not implemented.\n"
                                        "Filename would have been %s",(const char*)strName), "Warning",MB_ICONWARNING|MB_OK );
            xbool ok= g_WorldEditor.BuildDFS(strName);
            if (!ok)
            {
                ::MessageBox( NULL, "Export to DFS Failed.", "Error",MB_ICONERROR|MB_OK );
            }
        }
    }
}

//=========================================================================

CString strip_command(CString& strCmd)
{
    CString strCommand = "";
    CString strReturn = "";

    int Index = strCmd.Find('-');
    if ( Index != -1 )
    {
        strCommand = strCmd.Right(strCmd.GetLength() - (Index+1));
    }

    Index = strCommand.Find(' ');
    if ( Index != -1 )
    {
        strReturn = strCommand.Left(Index);
        strCmd = strCommand.Right(strCommand.GetLength() - Index);
    }

    strReturn.MakeUpper();

    return strReturn;
}

//=========================================================================

CString strip_argument(CString& strCmd)
{
    CString strReturn = "";
    CString strCommand = "";

    int Index = strCmd.Find('\'');
    
    // Check to see if there is no argument until the next command
    int Index2 = strCmd.Find('-');
    if( (Index2>-1) && (Index2 < Index) )
    {
        return CString("");
    }

    if ( Index != -1 )
    {
        strCommand = strCmd.Right(strCmd.GetLength() - Index - 1);
    }

    //strip any leading colons
    Index = strCommand.Find('\'');
    if ( Index != -1 )
    {
        strReturn = strCommand.Left(Index);
        strCmd    = strCommand.Right(strCommand.GetLength() - (Index+1));
    }

    strReturn.MakeUpper();

    return strReturn;
}

//=========================================================================

long _stdcall ExceptionFilter( struct _EXCEPTION_POINTERS* pExceptions )
{
    return EXCEPTION_EXECUTE_HANDLER;
}

//=========================================================================

void CMainFrame::LoadProjectFromCommandLine( void )
{
    // Setup so exceptions go to actual dialog box
    #ifdef X_DEBUG
    g_bErrorBreak = TRUE;
    #else
    g_bErrorBreak = FALSE;
    #endif

    POSITION pos = m_pProjectDocTemplate->GetFirstDocPosition( );

    if (pos && ((g_CommandLine != NULL) && g_CommandLine[0]) )
    {
        xbool bOldBreakVal = g_bErrorBreak;


        LOG_MESSAGE("AutoBuild","AUTOBUILD_LAUNCHED");
        LOG_MESSAGE("AutoBuild","Editor launched from commandline for autobuild");

        CString strExportPath;
        CString strLightingType;
        CString strLevelName;
        BOOL    bExitOnEnd   = FALSE;
        BOOL    bCompilePS2  = FALSE;
        BOOL    bCompileXBOX = FALSE;
        BOOL    bCompileGC   = FALSE;
        BOOL    bCompilePC   = FALSE;
        BOOL    bExportPS2  = FALSE;
        BOOL    bExportXBOX = FALSE;
        BOOL    bExportGC   = FALSE;
        BOOL    bExportPC   = FALSE;

        CProjectDoc* pDoc = (CProjectDoc*)m_pProjectDocTemplate->GetNextDoc( pos );

        //
        // Make sure to turn off the windows dialog for the exceptions
        //
        SetUnhandledExceptionFilter( ExceptionFilter );
           
        //
        // Parse command line options
        //
        CString strCmd(g_CommandLine);
        while( 1 )
        {
            //
            // Pull command and argument from command line
            //
            CString strCommand = strip_command(strCmd);
            CString strArgument= strip_argument(strCmd);
            if( (strCommand=="") )
                break;

            //
            // Branch based on command
            //
            if( strCommand == "EXPORTNAME" )
            {
                strExportPath = strArgument;
                g_bErrorBreak = FALSE;
                g_bAutoBuild  = TRUE;
                g_LogControl.EnableCallbackForMessages = TRUE;
            }
            else
            if( strCommand == "LIGHTING" )
            {
                strLightingType = strArgument;
                g_bErrorBreak = FALSE;
            }
            else
            if( strCommand == "EXIT" )
            {
                bExitOnEnd = TRUE;
                g_bErrorBreak = FALSE;
            }
            else
            if( strCommand == "PLATFORM" )
            {
                g_bAutoBuild  = TRUE;
                g_LogControl.EnableCallbackForMessages = TRUE;
                g_bErrorBreak = FALSE;
                bCompilePC    = TRUE;

                if (strArgument.CompareNoCase("PS2") == 0)
                {
                    bCompilePS2 = TRUE;  
                    bExportPS2 = TRUE;
                }
                else if (strArgument.CompareNoCase("XBOX") == 0)
                {
                    bCompileXBOX = TRUE;
                    bExportXBOX = TRUE;
                }
                else if (strArgument.CompareNoCase("GC") == 0)
                {
                    bCompileGC = TRUE;
                    bExportGC = TRUE;
                }
                else if (strArgument.CompareNoCase("PC") == 0)
                {
                    bCompilePC = TRUE;
                    bExportPC = TRUE;
                }
            }
            else
            if( strCommand == "PROJECTPATH" )
            {
                strLevelName = strArgument;
            }


        }

        // Shut off all dialogs for catch, throw, assert
        if( g_bAutoBuild )
        {
            extern xbool g_bSkipThrowCatchAssertDialogs;
            g_bSkipThrowCatchAssertDialogs = TRUE;
        }

        // Spit out options to logger
        LOG_MESSAGE("AutoBuild","EXPORTNAME:            %s\n",(const char*)strExportPath);
        LOG_MESSAGE("AutoBuild","LIGHTING:              %s\n",(const char*)strLightingType);
        LOG_MESSAGE("AutoBuild","EXIT:                  %d\n",bExitOnEnd);
        LOG_MESSAGE("AutoBuild","FORCE COMPILE PS2:     %d\n",bCompilePS2);
        LOG_MESSAGE("AutoBuild","FORCE COMPILE XBOX:    %d\n",bCompileXBOX);
        LOG_MESSAGE("AutoBuild","FORCE COMPILE GCN:     %d\n",bCompileGC);
        LOG_MESSAGE("AutoBuild","PROJECT:               %s\n",(const char*)strLevelName);
        LOG_MESSAGE("AutoBuild","AUTOBUILD_LOAD_STARTING");
        LOG_FLUSH();

        g_bAutoLoad = TRUE;
        if( pDoc->LoadProject( strLevelName ) )
        {
            //we opened a new file
        
            //handle world editor
            CBaseDocument* pEditorDoc = NULL;
            POSITION posEditor = m_pWorldEditDocTemplate->GetFirstDocPosition( );
            if (posEditor)
            {
                pEditorDoc = (CBaseDocument*)m_pWorldEditDocTemplate->GetNextDoc( posEditor );
                if (pEditorDoc) pEditorDoc->OnProjectOpen();
            }
        
            EDRscDesc_Doc *pRscDoc = NULL;

            //handle all registered editors
            reg_editor* pEditor = reg_editor::m_pHead;
            int nCount = 0;
            while( pEditor ) //for each editor
            {            
                posEditor = pEditor->m_pTemplate->GetFirstDocPosition( );
                while (posEditor) //for each document of that type
                {
                    pEditorDoc = (CBaseDocument*)pEditor->m_pTemplate->GetNextDoc( posEditor );
                    if (pEditorDoc) pEditorDoc->OnProjectOpen();

                    if (pEditorDoc->IsKindOf(RUNTIME_CLASS( EDRscDesc_Doc )))
                    {
                        pRscDoc = (EDRscDesc_Doc*)pEditorDoc;
                    }
                }
                pEditor = pEditor->m_pNext;
            }

            LOG_MESSAGE("AutoBuild","AUTOBUILD_LOAD_FINISHED");
            LOG_MESSAGE("AutoBuild","AUTOBUILD_COMPILE_STARTING");

            if (bCompilePC && pRscDoc)
            {
                pRscDoc->ForceCompilePC();   

                if (bCompilePS2)
                    pRscDoc->ForceCompilePS2();   

                if (bCompileXBOX)
                    pRscDoc->ForceCompileXBox();   

                if (bCompileGC)
                    pRscDoc->ForceCompileNintendo();   

                pRscDoc->Refresh();
                pRscDoc->Build();
            }

            // Setup the export flags for the platforms
            s32 nPlatform = g_Settings.GetPlatformCount();
            for( s32 i=0 ; i<nPlatform ; i++ )
            {
                asset_platform    PlatformType = g_Settings.GetPlatformTypeI( i );
                switch( PlatformType )
                {
                case ASSET_PLATFORM_DESKTOP:
                    g_Settings.SetPlatfromExportI( i, bExportPC );
                    break;
                case ASSET_PLATFORM_PS2:
                    g_Settings.SetPlatfromExportI( i, bExportPS2 );
                    break;
                case ASSET_PLATFORM_XBOX:
                    g_Settings.SetPlatfromExportI( i, bExportXBOX );
                    break;
                case ASSET_PLATFORM_GCN:
                    g_Settings.SetPlatfromExportI( i, bExportGC );
                    break;
                default:
                    g_Settings.SetPlatfromExportI( i, FALSE );
                    break;
                }
            }

            g_WorldEditor.SetAutomatedBuildParams(strLightingType, strExportPath, bExitOnEnd, bOldBreakVal);
        }
        else
        {
            g_bAutoLoad = FALSE;
            g_bErrorBreak = bOldBreakVal;
            AfxGetMainWnd()->PostMessage(WM_CLOSE, 0, 0);
        }
    }

    // Install exception handler if we are not in the autobuild mode
    if( !g_bAutoBuild )
    {
        theApp.InstallExceptionHandler();
    }
}

//=========================================================================

void CMainFrame::OnUpdateMruList(CCmdUI* pCmdUI) 
{
    ASSERT_VALID( this );

    if( m_pRecentFiles == NULL )
        pCmdUI->Enable( FALSE );
    else
    {
        m_pRecentFiles->UpdateMenu( pCmdUI );
    }
}

//=========================================================================

BOOL CMainFrame::OnMenuFileMru( UINT nID ) 
{
    ASSERT_VALID(this);
    ASSERT(m_pRecentFiles != NULL);

    ASSERT(nID >= ID_MRU_LIST);
    ASSERT(nID < ID_MRU_LIST + (UINT)m_pRecentFiles->GetSize());
    int nIndex = nID - ID_MRU_LIST;
    ASSERT((*m_pRecentFiles)[nIndex].GetLength() != 0);

    TRACE2("MRU: open file (%d) '%s'.\n", (nIndex) + 1,
            (LPCTSTR)(*m_pRecentFiles)[nIndex]);

    if( !DoFileOpen( (*m_pRecentFiles)[nIndex] ) )
        m_pRecentFiles->Remove(nIndex);

    return TRUE;
}

//=========================================================================

void CMainFrame::OnUpdateFrameTitle(BOOL Nada) 
{
    // Set new title
    CString Title;
    Title.Format( "Inevitable Editor - (Build %d) (%s)", g_Changelist, g_pBuildDate );
    SetWindowText( Title );
}

//=========================================================================

void CMainFrame::OnViewResetWindows( void )
{
    if( MessageBox( "This will reset the window and toolbar layouts the next time the editor is loaded. Are you sure?", "Reset Windows & Toolbars", MB_YESNO ) == IDYES )
    {
        g_ResetWindowLayout = TRUE;
    }
}

//=========================================================================

void CMainFrame::OnUpdateCleanupZones(CCmdUI* pCmdUI) 
{
    pCmdUI->Enable(g_Project.IsProjectOpen()&& g_WorldEditor.IsZoneFileEditable());    
    pCmdUI->SetCheck(FALSE);    
}

//=========================================================================

void CMainFrame::OnCleanupZones() 
{
    if (::AfxMessageBox("Are you sure you want to perform this maintenance option, doing so will delete any unresolved zones? (note: Changes will not be saved until you next save the level.)",MB_YESNO) == IDYES)
    {
        g_WorldEditor.CleanupZones();
    }
}
