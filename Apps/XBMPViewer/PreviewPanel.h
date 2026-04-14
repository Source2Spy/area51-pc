//==============================================================================
//
//  PreviewPanel.h
//
//==============================================================================

#ifndef XBMP_PREVIEW_PANEL_H
#define XBMP_PREVIEW_PANEL_H

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"

#include <QSlider>
#include <QWidget>

//==============================================================================

class BitmapPreviewWidget;
class QBoxLayout;

//==============================================================================
//  PreviewPanel CLASS
//==============================================================================

class PreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewPanel(QWidget* pParent = NULL);

    BitmapPreviewWidget* GetColorWidget  (void) const;
    BitmapPreviewWidget* GetAlphaWidget  (void) const;
    QSlider*             GetMipSlider    (void) const;

protected:
    void changeEvent     (QEvent* pEvent) override;
    void resizeEvent     (QResizeEvent* pEvent) override;

private:
    QBoxLayout*          m_pLayout;
    BitmapPreviewWidget* m_pBitmapColor;
    BitmapPreviewWidget* m_pBitmapAlpha;
    QSlider*             m_pMipSlider;
};

//==============================================================================
#endif // XBMP_PREVIEW_PANEL_H
//==============================================================================
