//==============================================================================
//
//  XBMPViewer.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewer.h"

#include "aux_Bitmap.hpp"
#include "BitmapPreviewWidget.h"
#include "BitmapConvertService.h"
#include "ConfigManager.h"
#include "ConvertSettingsDialog.h"
#include "ConvertUtils.h"
#include "AboutDialog.h"
#include "ExplorerPanel.h"
#include "FileListModel.h"
#include "FullscreenPreviewDialog.h"
#include "FileListView.h"
#include "PreviewPanel.h"
#include "SettingsDialog.h"
#include "ThemeManager.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDockWidget>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QItemSelection>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QUrl>
#include <QDesktopServices>
#include <QMenu>
#include <QProcess>
#include <QVector>

//------------------------------------------------------------------------------

namespace
{
    enum
    {
        COLUMN_NAME = 0,
        COLUMN_SIZE,
        COLUMN_DIMENSIONS,
        COLUMN_MIPS,
        COLUMN_TYPE,
        COLUMN_FORMAT,
        COLUMN_COUNT
    };
}

//------------------------------------------------------------------------------

static
QImage BuildPreviewImage(xbitmap& Bitmap, s32 Mip, xbool Alpha)
{
    const s32 Width = Bitmap.GetWidth(Mip);
    const s32 Height = Bitmap.GetHeight(Mip);
    if ((Width <= 0) || (Height <= 0))
        return QImage();

    QImage Image(Width, Height, QImage::Format_ARGB32);
    for (s32 y = 0; y < Height; ++y)
    {
        QRgb* pLine = reinterpret_cast<QRgb*>(Image.scanLine(y));
        for (s32 x = 0; x < Width; ++x)
        {
            const xcolor Color = Bitmap.GetPixelColor(x, y, Mip);
            if (Alpha)
                pLine[x] = qRgba(Color.A, Color.A, Color.A, Color.A);
            else
                pLine[x] = qRgba(Color.R, Color.G, Color.B, Color.A);
        }
    }

    return Image;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

XBMPViewer::XBMPViewer(QWidget* pParent)
    : QMainWindow(pParent)
    , m_pFileModel(NULL)
    , m_pSortModel(NULL)
    , m_pFileList(NULL)
    , m_pPreviewPanel(NULL)
    , m_pMipSlider(NULL)
    , m_pStatusColor(NULL)
    , m_pStatusTotal(NULL)
    , m_pStatusSelected(NULL)
    , m_pStatusFocus(NULL)
    , m_pExplorerPanel(NULL)
    , m_pActionConvertTga(NULL)
    , m_pActionConvertXbmp(NULL)
    , m_pActionExit(NULL)
    , m_pActionSettings(NULL)
    , m_pActionHelp(NULL)
    , m_pActionAbout(NULL)
    , m_pActionAboutQt(NULL)
    , m_SelectedGameType("Area-51")
    , m_SelectedThemeType("Windows White")
    , m_HasBitmap(FALSE)
{
    setWindowTitle("xbmpViewer");
    setObjectName("XBMPViewerMain");

    m_pFileModel = new FileListModel(this);
    m_pFileModel->SetGameType(m_SelectedGameType);
    m_pSortModel = new QSortFilterProxyModel(this);
    m_pSortModel->setSourceModel(m_pFileModel);
    m_pSortModel->setSortRole(Qt::UserRole);

    m_pFileList = new FileListView(this);
    m_pFileList->setModel(m_pSortModel);

    setCentralWidget(m_pFileList);

    m_pExplorerPanel = new ExplorerPanel(this);
    QDockWidget* pExplorerDock = new QDockWidget("Explorer", this);
    pExplorerDock->setObjectName("ExplorerDock");
    pExplorerDock->setWidget(m_pExplorerPanel);
    addDockWidget(Qt::LeftDockWidgetArea, pExplorerDock);

    QDockWidget* pPreviewDock = new QDockWidget("Preview", this);
    pPreviewDock->setObjectName("PreviewDock");
    m_pPreviewPanel = new PreviewPanel(pPreviewDock);
    pPreviewDock->setWidget(m_pPreviewPanel);
    addDockWidget(Qt::BottomDockWidgetArea, pPreviewDock);

    m_pMipSlider = m_pPreviewPanel->GetMipSlider();
    m_pMipSlider->setEnabled(FALSE);

    m_pActionConvertTga = new QAction("Convert to TGA", this);
    m_pActionConvertTga->setEnabled(FALSE);
    m_pActionConvertXbmp = new QAction("Convert to XBMP", this);
    m_pActionConvertXbmp->setEnabled(FALSE);
    m_pActionExit = new QAction("Exit", this);
    m_pActionSettings = new QAction("Settings", this);
    m_pActionHelp = new QAction("Help", this);
    m_pActionAbout = new QAction("About", this);
    m_pActionAboutQt = new QAction("About Qt", this);

    QMenu* pFileMenu = menuBar()->addMenu("File");
    pFileMenu->addAction(m_pActionExit);
    QMenu* pConvertMenu = menuBar()->addMenu("Convert");
    pConvertMenu->addAction(m_pActionConvertTga);
    pConvertMenu->addAction(m_pActionConvertXbmp);

    QMenu* pViewMenu = menuBar()->addMenu("View");
    QAction* pToggleExplorer = pExplorerDock->toggleViewAction();
    pToggleExplorer->setText("Explorer");
    pViewMenu->addAction(pToggleExplorer);
    QAction* pTogglePreview = pPreviewDock->toggleViewAction();
    pTogglePreview->setText("Preview");
    pViewMenu->addAction(pTogglePreview);

    QMenu* pSettingsMenu = menuBar()->addMenu("Settings");
    pSettingsMenu->addAction(m_pActionSettings);

    QMenu* pHelpMenu = menuBar()->addMenu("Help");
    pHelpMenu->addAction(m_pActionHelp);
    pHelpMenu->addSeparator();
    pHelpMenu->addAction(m_pActionAbout);
    pHelpMenu->addSeparator();
    pHelpMenu->addAction(m_pActionAboutQt);

    m_pStatusColor = new QLabel(this);
    m_pStatusTotal = new QLabel(this);
    m_pStatusSelected = new QLabel(this);
    m_pStatusFocus = new QLabel(this);

    statusBar()->addWidget(m_pStatusColor, 1);
    statusBar()->addWidget(m_pStatusTotal, 0);
    statusBar()->addWidget(m_pStatusSelected, 0);
    statusBar()->addWidget(m_pStatusFocus, 1);

    m_pStatusColor->setMinimumWidth(180);
    m_pStatusTotal->setMinimumWidth(120);
    m_pStatusSelected->setMinimumWidth(120);
    m_pStatusFocus->setMinimumWidth(220);

    connect(m_pExplorerPanel, &ExplorerPanel::PathSelected,
            this, &XBMPViewer::OnExplorerPathSelected);
    connect(m_pFileList->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &XBMPViewer::OnFileSelected);
    connect(m_pFileList->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &XBMPViewer::OnSelectionChanged);
    connect(m_pFileList, &QTableView::customContextMenuRequested,
            this, &XBMPViewer::OnFileContextMenu);
    connect(m_pMipSlider, &QSlider::valueChanged,
            this, &XBMPViewer::OnMipChanged);

    connect(m_pActionConvertTga, &QAction::triggered,
            this, &XBMPViewer::OnConvertTga);
    connect(m_pActionConvertXbmp, &QAction::triggered,
            this, &XBMPViewer::OnConvertXbmp);
    connect(m_pActionExit, &QAction::triggered,
            this, &QWidget::close);
    connect(m_pActionSettings, &QAction::triggered,
            this, &XBMPViewer::OnShowSettings);
    connect(m_pActionHelp, &QAction::triggered,
            this, &XBMPViewer::OnShowHelp);
    connect(m_pActionAbout, &QAction::triggered,
            this, &XBMPViewer::OnShowAbout);
    connect(m_pActionAboutQt, &QAction::triggered,
            this, &XBMPViewer::OnShowAboutQt);

    m_pPreviewPanel->GetColorWidget()->SetHoverCallback(
        [this](s32 X, s32 Y, s32 R, s32 G, s32 B, s32 A)
        {
            OnPreviewHover(X, Y, R, G, B, A);
        });
    m_pPreviewPanel->GetAlphaWidget()->SetHoverCallback(
        [this](s32 X, s32 Y, s32 R, s32 G, s32 B, s32 A)
        {
            OnPreviewHover(X, Y, R, G, B, A);
        });
    connect(m_pPreviewPanel->GetColorWidget(), &BitmapPreviewWidget::ImageClicked,
            this, &XBMPViewer::OnPreviewClicked);
    connect(m_pPreviewPanel->GetAlphaWidget(), &BitmapPreviewWidget::ImageClicked,
            this, &XBMPViewer::OnPreviewClicked);

    QString ConfigGameType = m_SelectedGameType;
    QString ConfigThemeType = m_SelectedThemeType;
    QByteArray ConfigGeometry;
    QByteArray ConfigState;
    const xbool HasConfig = ConfigManager::LoadConfig(ConfigGameType, ConfigThemeType, ConfigGeometry, ConfigState);
    if (!HasConfig)
    {
        QStringList Items;
        Items << "Area-51" << "The Hobbit" << "Tribes:AA";
        bool Ok = false;
        const QString Choice = QInputDialog::getItem(this, "First launch", "Select game type", Items, 0, false, &Ok);
        if (Ok && !Choice.isEmpty())
            ConfigGameType = Choice;
    }

    m_SelectedGameType = ConfigGameType;
    m_SelectedThemeType = ConfigThemeType;
    m_pFileModel->SetGameType(m_SelectedGameType);

    ThemeManager::ApplyTheme(m_SelectedThemeType);
    if (!ConfigGeometry.isEmpty())
        restoreGeometry(ConfigGeometry);
    if (!ConfigState.isEmpty())
        restoreState(ConfigState);
    OnExplorerPathSelected(m_pExplorerPanel->GetPath());
}

//==============================================================================

void XBMPViewer::closeEvent(QCloseEvent* pEvent)
{
    if (!ConfigManager::SaveConfig(m_SelectedGameType, m_SelectedThemeType, saveGeometry(), saveState()))
        x_DebugMsg("xbmpViewer: failed to save config\n");
    QMainWindow::closeEvent(pEvent);
}

//==============================================================================

QVector<const FileRecord*> XBMPViewer::GetSelectedRecords(void) const
{
    const QModelIndexList Selection = m_pFileList->selectionModel()->selectedRows();
    QVector<const FileRecord*> Records;
    for (s32 i = 0; i < Selection.size(); ++i)
    {
        const QModelIndex SourceIndex = m_pSortModel->mapToSource(Selection[i]);
        const FileRecord* pRecord = m_pFileModel->GetFile(SourceIndex.row());
        if (pRecord)
            Records.push_back(pRecord);
    }
    return Records;
}

//==============================================================================

void XBMPViewer::OnExplorerPathSelected(const QString& Path)
{
    if (Path.isEmpty())
        return;

    m_CurrentPath = Path;
    m_pFileModel->SetDirectory(Path);
    m_pFileList->resizeColumnToContents(COLUMN_NAME);
    m_pFileList->resizeColumnToContents(COLUMN_SIZE);
    m_pFileList->resizeColumnToContents(COLUMN_DIMENSIONS);
    m_pFileList->resizeColumnToContents(COLUMN_MIPS);
    m_pFileList->resizeColumnToContents(COLUMN_TYPE);
    m_pFileList->resizeColumnToContents(COLUMN_FORMAT);

    UpdateStatusTotals();
    UpdateStatusFocus();
}

//==============================================================================

void XBMPViewer::OnFileSelected(const QModelIndex& Index)
{
    if (!Index.isValid())
        return;

    const QModelIndex SourceIndex = m_pSortModel->mapToSource(Index);
    const FileRecord* pRecord = m_pFileModel->GetFile(SourceIndex.row());
    if (!pRecord)
        return;

    LoadBitmap(pRecord->Path);
    UpdatePreview();
    UpdateStatus();
    UpdateConvertActions();
}

//==============================================================================

void XBMPViewer::OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected)
{
    (void)Selected;
    (void)Deselected;
    UpdateStatusTotals();
    UpdateStatusFocus();
    UpdateConvertActions();
}

//==============================================================================

void XBMPViewer::OnMipChanged(s32 Value)
{
    (void)Value;
    UpdatePreview();
    UpdateStatus();
}

//==============================================================================

void XBMPViewer::OnPreviewHover(s32 X, s32 Y, s32 R, s32 G, s32 B, s32 A)
{
    if (X < 0 || Y < 0)
    {
        m_pStatusColor->setText(QString());
        return;
    }

    m_pStatusColor->setText(QString("XY (%1,%2) RGBA (%3,%4,%5,%6)")
        .arg(X)
        .arg(Y)
        .arg(R)
        .arg(G)
        .arg(B)
        .arg(A));
}

//==============================================================================

void XBMPViewer::OnFileContextMenu(const QPoint& Pos)
{
    const QModelIndex Index = m_pFileList->indexAt(Pos);
    if (!Index.isValid())
        return;

    const QModelIndex SourceIndex = m_pSortModel->mapToSource(Index);
    const FileRecord* pRecord = m_pFileModel->GetFile(SourceIndex.row());
    QString ConvertText("Convert");
    if (pRecord)
    {
        const QString Ext = QFileInfo(pRecord->Path).suffix().toLower();
        if (Ext == "xbmp" || Ext == "xbm")
            ConvertText = "Convert to TGA";
        else if (Ext == "psd" || Ext == "tga" || Ext == "bmp")
            ConvertText = "Convert to XBMP";
    }

    QMenu Menu(this);
    QAction* pOpenExplorer = Menu.addAction("Open in Explorer");
    Menu.addSeparator();
    QAction* pConvert = Menu.addAction(ConvertText);

    QAction* pAction = Menu.exec(m_pFileList->viewport()->mapToGlobal(Pos));
    if (pAction == pOpenExplorer)
        OnOpenInExplorer();
    else if (pAction == pConvert)
        OnConvertContext();
}

//==============================================================================

void XBMPViewer::OnOpenInExplorer(void)
{
    const QVector<const FileRecord*> Records = GetSelectedRecords();
    if (Records.isEmpty())
        return;

    QStringList Args;
    Args << "/select," << QDir::toNativeSeparators(Records.front()->Path);
    QProcess::startDetached("explorer.exe", Args);
}

//==============================================================================

void XBMPViewer::OnConvertContext(void)
{
    const QVector<const FileRecord*> Records = GetSelectedRecords();
    if (Records.isEmpty())
        return;

    BitmapConvertService ConvertService;
    const BitmapConvertService::ContextNeeds Needs = ConvertService.GetContextNeeds(Records);

    QString Platform;
    QString Format;
    s32 MipLevels = 0;
    if (Needs.NeedXbmp)
    {
        ConvertSettingsDialog Dialog(this);
        if (Dialog.exec() != QDialog::Accepted)
            return;
        Platform = Dialog.GetPlatform();
        Format = Dialog.GetFormat();
        MipLevels = Dialog.GetMipLevels();
    }

    const QString OutputDir = QFileDialog::getExistingDirectory(this, "Select Output Folder", m_CurrentPath);
    if (OutputDir.isEmpty())
        return;

    xbitmap::format TargetFormat = xbitmap::FMT_NULL;
    if (Needs.NeedXbmp)
    {
        TargetFormat = ConvertFormatString(Format);
        if (TargetFormat == xbitmap::FMT_NULL)
        {
            x_DebugMsg("xbmpViewer: invalid format selection: %s\n", Format.toLocal8Bit().constData());
            return;
        }
    }

    ConvertService.ConvertContext(Records, OutputDir, Platform, TargetFormat, MipLevels, m_SelectedGameType);
}

//==============================================================================

void XBMPViewer::OnPreviewClicked(const QImage& Image, xbool Alpha)
{
    (void)Alpha;
    if (Image.isNull())
        return;

    FullscreenPreviewDialog Dialog(this);
    Dialog.SetImage(Image);
    Dialog.exec();
}

//==============================================================================

void XBMPViewer::OnConvertTga(void)
{
    const QVector<const FileRecord*> Records = GetSelectedRecords();
    if (Records.isEmpty())
        return;

    const QString OutputDir = QFileDialog::getExistingDirectory(this, "Select Output Folder", m_CurrentPath);
    if (OutputDir.isEmpty())
        return;

    BitmapConvertService ConvertService;
    ConvertService.ConvertToTga(Records, OutputDir);
}

//==============================================================================

void XBMPViewer::OnConvertXbmp(void)
{
    ConvertSettingsDialog Dialog(this);
    if (Dialog.exec() != QDialog::Accepted)
        return;

    const QString Platform = Dialog.GetPlatform();
    const QString Format = Dialog.GetFormat();
    const s32 MipLevels = Dialog.GetMipLevels();
    const xbool GenericCompression = Dialog.GetGenericCompression();
    (void)GenericCompression;

    const QVector<const FileRecord*> Records = GetSelectedRecords();
    if (Records.isEmpty())
        return;

    const QString OutputDir = QFileDialog::getExistingDirectory(this, "Select Output Folder", m_CurrentPath);
    if (OutputDir.isEmpty())
        return;

    const xbitmap::format TargetFormat = ConvertFormatString(Format);
    if (TargetFormat == xbitmap::FMT_NULL)
    {
        x_DebugMsg("xbmpViewer: invalid format selection: %s\n", Format.toLocal8Bit().constData());
        return;
    }

    BitmapConvertService ConvertService;
    ConvertService.ConvertToXbmp(Records, OutputDir, Platform, TargetFormat, MipLevels, m_SelectedGameType);
}

//==============================================================================

void XBMPViewer::OnShowSettings(void)
{
    SettingsDialog Dialog(this);
    Dialog.SetGameType(m_SelectedGameType);
    Dialog.SetThemeType(m_SelectedThemeType);
    if (Dialog.exec() == QDialog::Accepted)
    {
        m_SelectedGameType = Dialog.GetGameType();
        m_SelectedThemeType = Dialog.GetThemeType();
        ThemeManager::ApplyTheme(m_SelectedThemeType);
        m_pFileModel->SetGameType(m_SelectedGameType);
        OnExplorerPathSelected(m_pExplorerPanel->GetPath());
    }
}

//==============================================================================

void XBMPViewer::OnShowHelp(void)
{
    QDesktopServices::openUrl(QUrl("https://discord.gg/HvYKQ22exg"));
}

//==============================================================================

void XBMPViewer::OnShowAbout(void)
{
    AboutDialog Dialog(m_SelectedGameType, m_SelectedThemeType, this);
    Dialog.exec();
}

//==============================================================================

void XBMPViewer::OnShowAboutQt(void)
{
    QApplication::aboutQt();
}

//==============================================================================

void XBMPViewer::LoadBitmap(const QString& Path)
{
    m_HasBitmap = FALSE;
    m_Bitmap.Kill();
    m_pMipSlider->setRange(0, 0);
    m_pMipSlider->setEnabled(FALSE);
    m_pMipSlider->setValue(0);

    const QByteArray PathBytes = Path.toLocal8Bit();
    if (!auxbmp_Load(m_Bitmap, PathBytes.constData()))
    {
        x_DebugMsg("xbmpViewer: failed to load bitmap: %s\n", PathBytes.constData());
        return;
    }

    if (m_Bitmap.GetFlags() & xbitmap::FLAG_GCN_DATA_SWIZZLED)
        m_Bitmap.GCNUnswizzleData();

    switch (m_Bitmap.GetFormat())
    {
    case xbitmap::FMT_DXT1:
    case xbitmap::FMT_DXT3:
    case xbitmap::FMT_DXT5:
        auxbmp_Decompress(m_Bitmap);
        break;
    default:
        break;
    }

    if (m_SelectedGameType == "The Hobbit")
        m_Bitmap.SetFormat(RemapFormatForHobbit(m_Bitmap.GetFormat()));

    s32 MipCount = m_Bitmap.GetNMips();
    if (MipCount < 0)
        MipCount = 0;

    m_pMipSlider->setRange(0, MipCount);
    m_pMipSlider->setEnabled(TRUE);
    if (m_pMipSlider->value() > MipCount)
        m_pMipSlider->setValue(MipCount);

    m_HasBitmap = TRUE;
}

//==============================================================================

void XBMPViewer::UpdatePreview(void)
{
    if (!m_HasBitmap)
    {
        m_pPreviewPanel->GetColorWidget()->SetImage(QImage());
        m_pPreviewPanel->GetAlphaWidget()->SetImage(QImage());
        return;
    }

    s32 Mip = m_pMipSlider->value();
    const s32 MipCount = m_Bitmap.GetNMips();
    if (MipCount >= 0 && Mip > MipCount)
        Mip = MipCount;

    QImage ColorImage = BuildPreviewImage(m_Bitmap, Mip, FALSE);
    QImage AlphaImage = BuildPreviewImage(m_Bitmap, Mip, TRUE);

    m_pPreviewPanel->GetColorWidget()->SetImage(ColorImage);
    m_pPreviewPanel->GetAlphaWidget()->SetImage(AlphaImage);
}

//==============================================================================

void XBMPViewer::UpdateConvertActions(void)
{
    const QVector<const FileRecord*> Records = GetSelectedRecords();

    xbool HasXbmp = FALSE;
    xbool HasSource = FALSE;

    for (s32 i = 0; i < Records.size(); ++i)
    {
        const QString Ext = QFileInfo(Records[i]->Path).suffix().toLower();
        if (Ext == "xbmp" || Ext == "xbm")
            HasXbmp = TRUE;
        else if (Ext == "tga" || Ext == "psd" || Ext == "bmp")
            HasSource = TRUE;
    }

    m_pActionConvertTga->setEnabled(HasXbmp);
    m_pActionConvertXbmp->setEnabled(HasSource);
}

//==============================================================================

void XBMPViewer::UpdateStatus(void)
{
    UpdateStatusTotals();
    UpdateStatusFocus();
}

//==============================================================================

void XBMPViewer::UpdateStatusTotals(void)
{
    const s32 TotalFiles = m_pFileModel->GetFileCount();
    const s64 TotalBytes = m_pFileModel->GetTotalBytes();

    m_pStatusTotal->setText(QString("   Total %1 %2 %3   ")
        .arg(TotalFiles)
        .arg(TotalFiles == 1 ? "file" : "files")
        .arg(FormatSizeString(TotalBytes)));

    const QVector<const FileRecord*> Records = GetSelectedRecords();
    s64 SelectedBytes = 0;
    for (s32 i = 0; i < Records.size(); ++i)
        SelectedBytes += Records[i]->SizeBytes;

    m_pStatusSelected->setText(QString("   Selected %1 %2 %3   ")
        .arg(Records.size())
        .arg(Records.size() == 1 ? "file" : "files")
        .arg(FormatSizeString(SelectedBytes)));
}

//==============================================================================

void XBMPViewer::UpdateStatusFocus(void)
{
    const QVector<const FileRecord*> Records = GetSelectedRecords();
    if (Records.isEmpty())
    {
        m_pStatusFocus->setText(QString());
        return;
    }

    const FileRecord* pRecord = Records.front();
    QString Detail;
    if ((pRecord->Width > 0) && (pRecord->Height > 0))
    {
        Detail = QString(" (%1x%2x%3b) (%4 %5) (%6)")
            .arg(pRecord->Width)
            .arg(pRecord->Height)
            .arg(pRecord->BitDepth)
            .arg(pRecord->Mips)
            .arg(pRecord->Mips == 1 ? "mip" : "mips")
            .arg(pRecord->Format);
    }

    m_pStatusFocus->setText(QString("   %1 (%2)%3   ")
        .arg(pRecord->Name)
        .arg(FormatFileSizeKB(pRecord->SizeBytes))
        .arg(Detail));
}

//==============================================================================

QString XBMPViewer::FormatSizeString(s64 Bytes) const
{
    if (Bytes > (1024 * 1024))
        return QString("(%1 MB)").arg(QString::number(Bytes / (1024.0 * 1024.0), 'f', 1));

    return QString("(%1 KB)").arg(QString::number(Bytes / 1024.0, 'f', 1));
}

//==============================================================================

QString XBMPViewer::FormatFileSizeKB(s64 Bytes) const
{
    s64 KB = Bytes / 1024;
    QString Raw = QString::number(KB);
    QString WithCommas;
    s32 Count = Raw.length();
    for (s32 i = 0; i < Raw.length(); ++i)
    {
        WithCommas += Raw.at(i);
        Count--;
        if (((Count % 3) == 0) && (Count > 0))
            WithCommas += ',';
    }
    WithCommas += " KB";
    return WithCommas;
}
