//==============================================================================
//  
//  XBMPViewer.h
//  
//==============================================================================

#ifndef XBMP_VIEWER_H
#define XBMP_VIEWER_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QMainWindow>
#include <QString>
#include <QVector>

//==============================================================================

class QAction;
class QLabel;
class QItemSelection;
class QSlider;
class FileListView;
class ExplorerPanel;
class FileListModel;
struct FileRecord;
class PreviewPanel;
class QSortFilterProxyModel;

//==============================================================================
//  XBMPViewer CLASS
//==============================================================================

class XBMPViewer : public QMainWindow
{
    Q_OBJECT

public:
    explicit XBMPViewer         (QWidget* pParent = NULL);

protected:
    void closeEvent             (QCloseEvent* pEvent) override;

private slots:
    void OnFileSelected         (const QModelIndex& Index);
    void OnSelectionChanged     (const QItemSelection& Selected, const QItemSelection& Deselected);
    void OnMipChanged           (s32 Value);
    void OnPreviewHover         (s32 X, s32 Y, s32 R, s32 G, s32 B, s32 A);
    void OnFileContextMenu      (const QPoint& Pos);
    void OnConvertTga           (void);
    void OnConvertXbmp          (void);
    void OnConvertContext       (void);
    void OnOpenInExplorer       (void);
    void OnPreviewClicked       (const QImage& Image, xbool Alpha);
    void OnShowSettings         (void);
    void OnShowHelp             (void);
    void OnShowAbout            (void);
    void OnShowAboutQt          (void);
    void OnExplorerPathSelected (const QString& Path);

private:
    QVector<const FileRecord*> GetSelectedRecords (void) const;
    void    LoadBitmap                            (const QString& Path);
    void    UpdatePreview                         (void);
    void    UpdateStatus                          (void);
    void    UpdateStatusTotals                    (void);
    void    UpdateStatusFocus                     (void);
    void    UpdateConvertActions                  (void);
    QString FormatSizeString                      (s64 Bytes) const;
    QString FormatFileSizeKB                      (s64 Bytes) const;

private:
    FileListModel*         m_pFileModel;
    QSortFilterProxyModel* m_pSortModel;
    FileListView*          m_pFileList;
    PreviewPanel*          m_pPreviewPanel;
    QSlider*               m_pMipSlider;
    QLabel*                m_pStatusColor;
    QLabel*                m_pStatusTotal;
    QLabel*                m_pStatusSelected;
    QLabel*                m_pStatusFocus;
    ExplorerPanel*         m_pExplorerPanel;
    QAction*               m_pActionConvertTga;
    QAction*               m_pActionConvertXbmp;
    QAction*               m_pActionExit;
    QAction*               m_pActionSettings;
    QAction*               m_pActionHelp;
    QAction*               m_pActionAbout;
    QAction*               m_pActionAboutQt;
    QString                m_SelectedGameType;
    QString                m_SelectedThemeType;
    QString                m_CurrentPath;
    xbitmap                m_Bitmap;
    xbool                  m_HasBitmap;
};

//==============================================================================
#endif // XBMP_VIEWER_H
//==============================================================================
