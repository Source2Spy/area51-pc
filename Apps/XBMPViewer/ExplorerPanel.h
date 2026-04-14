//==============================================================================
//
//  ExplorerPanel.h
//
//==============================================================================

#ifndef XBMP_EXPLORER_PANEL_H
#define XBMP_EXPLORER_PANEL_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QModelIndex>
#include <QWidget>

//==============================================================================

class QFileSystemModel;
class QCompleter;
class QLineEdit;
class QTreeView;

//==============================================================================
//  ExplorerPanel CLASS
//==============================================================================

class ExplorerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerPanel      (QWidget* pParent = NULL);

    void    SetPath             (const QString& Path);
    QString GetPath             (void) const;

signals:
    void PathSelected           (const QString& Path);

protected:
    void changeEvent            (QEvent* pEvent) override;

private slots:
    void OnTreeSelectionChanged (const QModelIndex& Current, const QModelIndex& Previous);
    void OnPathEntered          (void);

private:
    void UpdateSelection        (const QString& Path);

private:
    QFileSystemModel*   m_pModel;
    QTreeView*          m_pTree;
    QLineEdit*          m_pPathEdit;
    QCompleter*         m_pPathCompleter;
    QString             m_CurrentPath;
};

//==============================================================================
#endif // XBMP_EXPLORER_PANEL_H
//==============================================================================
