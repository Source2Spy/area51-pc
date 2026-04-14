//==============================================================================
//
//  PreviewPanel.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PreviewPanel.h"

#include "BitmapPreviewWidget.h"

#include <QBoxLayout>
#include <QEvent>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

PreviewPanel::PreviewPanel(QWidget* pParent)
    : QWidget(pParent)
    , m_pLayout(NULL)
    , m_pBitmapColor(NULL)
    , m_pBitmapAlpha(NULL)
    , m_pMipSlider(NULL)
{
    setAutoFillBackground(true);

    m_pBitmapColor = new BitmapPreviewWidget(this);
    m_pBitmapAlpha = new BitmapPreviewWidget(this);
    m_pBitmapAlpha->SetAlpha(TRUE);

    m_pMipSlider = new QSlider(Qt::Vertical, this);
    m_pMipSlider->setRange(0, 0);
    m_pMipSlider->setTickPosition(QSlider::TicksBothSides);
    m_pMipSlider->setTickInterval(1);

    m_pLayout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    m_pLayout->setContentsMargins(1, 1, 1, 1);
    m_pLayout->setSpacing(4);
    m_pLayout->addWidget(m_pBitmapColor, 1);
    m_pLayout->addWidget(m_pMipSlider, 0);
    m_pLayout->addWidget(m_pBitmapAlpha, 1);
}

//==============================================================================

BitmapPreviewWidget* PreviewPanel::GetColorWidget(void) const
{
    return m_pBitmapColor;
}

//==============================================================================

BitmapPreviewWidget* PreviewPanel::GetAlphaWidget(void) const
{
    return m_pBitmapAlpha;
}

//==============================================================================

QSlider* PreviewPanel::GetMipSlider(void) const
{
    return m_pMipSlider;
}

//==============================================================================

void PreviewPanel::changeEvent(QEvent* pEvent)
{
    QWidget::changeEvent(pEvent);

    const QEvent::Type T = pEvent->type();
    if (T == QEvent::StyleChange || T == QEvent::PaletteChange)
        setAutoFillBackground(true);
}

//==============================================================================

void PreviewPanel::resizeEvent(QResizeEvent* pEvent)
{
    QWidget::resizeEvent(pEvent);

    // Switch layout direction and slider orientation based on aspect ratio.
    // Fix slider to 50px in its "thin" dimension and let the layout stretch it
    // in the other dimension.
    const s32 SliderSize = 50;
    if (width() >= height())
    {
        m_pLayout->setDirection(QBoxLayout::LeftToRight);
        m_pMipSlider->setOrientation(Qt::Vertical);
        m_pMipSlider->setMinimumSize(SliderSize, 0);
        m_pMipSlider->setMaximumSize(SliderSize, QWIDGETSIZE_MAX);
    }
    else
    {
        m_pLayout->setDirection(QBoxLayout::TopToBottom);
        m_pMipSlider->setOrientation(Qt::Horizontal);
        m_pMipSlider->setMinimumSize(0, SliderSize);
        m_pMipSlider->setMaximumSize(QWIDGETSIZE_MAX, SliderSize);
    }
}
