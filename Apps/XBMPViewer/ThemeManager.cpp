//==============================================================================
//
//  ThemeManager.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QWidget>
#include <QtGlobal>

//==============================================================================

namespace
{
struct ThemeColors
{
    QColor Window;
    QColor WindowText;
    QColor Base;
    QColor AlternateBase;
    QColor ToolTipBase;
    QColor ToolTipText;
    QColor Text;
    QColor Button;
    QColor ButtonText;
    QColor BrightText;
    QColor Link;
    QColor Highlight;
    QColor HighlightedText;
};

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

static
QColor BlendColor(const QColor& Source, const QColor& Target, int BlendPercent)
{
    const int ClampedPercent = qBound(0, BlendPercent, 100);
    const int SourcePercent = 100 - ClampedPercent;

    return QColor((Source.red() * SourcePercent + Target.red() * ClampedPercent) / 100,
                  (Source.green() * SourcePercent + Target.green() * ClampedPercent) / 100,
                  (Source.blue() * SourcePercent + Target.blue() * ClampedPercent) / 100);
}

//==============================================================================

static
void ApplyColorGroup(QPalette& Palette, QPalette::ColorGroup Group, const ThemeColors& Colors)
{
    Palette.setColor(Group, QPalette::Window,          Colors.Window);
    Palette.setColor(Group, QPalette::WindowText,      Colors.WindowText);
    Palette.setColor(Group, QPalette::Base,            Colors.Base);
    Palette.setColor(Group, QPalette::AlternateBase,   Colors.AlternateBase);
    Palette.setColor(Group, QPalette::ToolTipBase,     Colors.ToolTipBase);
    Palette.setColor(Group, QPalette::ToolTipText,     Colors.ToolTipText);
    Palette.setColor(Group, QPalette::Text,            Colors.Text);
    Palette.setColor(Group, QPalette::Button,          Colors.Button);
    Palette.setColor(Group, QPalette::ButtonText,      Colors.ButtonText);
    Palette.setColor(Group, QPalette::BrightText,      Colors.BrightText);
    Palette.setColor(Group, QPalette::Link,            Colors.Link);
    Palette.setColor(Group, QPalette::Highlight,       Colors.Highlight);
    Palette.setColor(Group, QPalette::HighlightedText, Colors.HighlightedText);
}

//==============================================================================

static
ThemeColors MakeMutedColors(const ThemeColors& ActiveColors, int BackgroundBlend, int TextBlend, int HighlightBlend)
{
    ThemeColors Colors = ActiveColors;

    Colors.Base          = BlendColor(ActiveColors.Base,          ActiveColors.Window,   BackgroundBlend);
    Colors.AlternateBase = BlendColor(ActiveColors.AlternateBase, ActiveColors.Window,   BackgroundBlend);
    Colors.ToolTipBase   = BlendColor(ActiveColors.ToolTipBase,   ActiveColors.Window,   BackgroundBlend);
    Colors.Button        = BlendColor(ActiveColors.Button,        ActiveColors.Window,   BackgroundBlend);

    Colors.WindowText    = BlendColor(ActiveColors.WindowText,    ActiveColors.Window,     TextBlend);
    Colors.Text          = BlendColor(ActiveColors.Text,          ActiveColors.Base,       TextBlend);
    Colors.ToolTipText   = BlendColor(ActiveColors.ToolTipText,   ActiveColors.ToolTipBase,TextBlend);
    Colors.ButtonText    = BlendColor(ActiveColors.ButtonText,    ActiveColors.Button,     TextBlend);
    Colors.BrightText    = BlendColor(ActiveColors.BrightText,    ActiveColors.Window,     TextBlend);
    Colors.Link          = BlendColor(ActiveColors.Link,          ActiveColors.Window,     TextBlend);
    Colors.Highlight     = BlendColor(ActiveColors.Highlight,     ActiveColors.Window, HighlightBlend);
    Colors.HighlightedText = BlendColor(ActiveColors.HighlightedText, ActiveColors.Highlight, TextBlend);

    return Colors;
}

//==============================================================================

static
ThemeColors MakeInactiveColors(const ThemeColors& ActiveColors)
{
    return MakeMutedColors(ActiveColors, 8, 30, 55);
}

//==============================================================================

static
ThemeColors MakeDisabledColors(const ThemeColors& ActiveColors)
{
    return MakeMutedColors(ActiveColors, 18, 60, 80);
}

//==============================================================================

static
void ApplyHeaderPaletteStyle(void)
{
    const char* pStyle =
        "QHeaderView::section {"
        " background-color: palette(Button);"
        " color: palette(ButtonText);"
        "}"
        "QTableCornerButton::section {"
        " background-color: palette(Button);"
        "}";

    qApp->setStyleSheet(pStyle);
}
}

//==============================================================================

void ThemeManager::ApplyTheme(const QString& ThemeType)
{
    qApp->setStyleSheet("");

    QString      StyleName;
    ThemeColors  ActiveColors;
    bool         NeedHeaderStyle = false;

    if (ThemeType == "Fusion Dark")
    {
        StyleName = "Fusion";
        ActiveColors.Window          = QColor(53, 53, 53);
        ActiveColors.WindowText      = Qt::white;
        ActiveColors.Base            = QColor(35, 35, 35);
        ActiveColors.AlternateBase   = QColor(53, 53, 53);
        ActiveColors.ToolTipBase     = Qt::white;
        ActiveColors.ToolTipText     = Qt::white;
        ActiveColors.Text            = Qt::white;
        ActiveColors.Button          = QColor(53, 53, 53);
        ActiveColors.ButtonText      = Qt::white;
        ActiveColors.BrightText      = Qt::red;
        ActiveColors.Link            = QColor(42, 130, 218);
        ActiveColors.Highlight       = QColor(42, 130, 218);
        ActiveColors.HighlightedText = Qt::black;
    }
    else if (ThemeType == "Fusion White")
    {
        StyleName = "Fusion";
        ActiveColors.Window          = Qt::white;
        ActiveColors.WindowText      = Qt::black;
        ActiveColors.Base            = Qt::white;
        ActiveColors.AlternateBase   = QColor(245, 245, 245);
        ActiveColors.ToolTipBase     = Qt::white;
        ActiveColors.ToolTipText     = Qt::black;
        ActiveColors.Text            = Qt::black;
        ActiveColors.Button          = QColor(240, 240, 240);
        ActiveColors.ButtonText      = Qt::black;
        ActiveColors.BrightText      = Qt::red;
        ActiveColors.Link            = QColor(0, 0, 255);
        ActiveColors.Highlight       = QColor(0, 120, 215);
        ActiveColors.HighlightedText = Qt::white;
    }
    else if (ThemeType == "Windows Dark")
    {
        StyleName       = "windows";
        NeedHeaderStyle = true;
        ActiveColors.Window          = QColor(22, 32, 32);
        ActiveColors.WindowText      = Qt::white;
        ActiveColors.Base            = QColor(25, 25, 25);
        ActiveColors.AlternateBase   = QColor(42, 42, 42);
        ActiveColors.ToolTipBase     = QColor(53, 53, 53);
        ActiveColors.ToolTipText     = Qt::white;
        ActiveColors.Text            = Qt::white;
        ActiveColors.Button          = QColor(45, 45, 45);
        ActiveColors.ButtonText      = Qt::white;
        ActiveColors.BrightText      = Qt::red;
        ActiveColors.Link            = QColor(42, 130, 218);
        ActiveColors.Highlight       = QColor(42, 130, 218);
        ActiveColors.HighlightedText = Qt::white;
    }
    else if (ThemeType == "Windows White")
    {
        StyleName       = "windows";
        NeedHeaderStyle = true;
        ActiveColors.Window          = QColor(240, 240, 240);
        ActiveColors.WindowText      = Qt::black;
        ActiveColors.Base            = Qt::white;
        ActiveColors.AlternateBase   = QColor(233, 233, 233);
        ActiveColors.ToolTipBase     = QColor(255, 255, 220);
        ActiveColors.ToolTipText     = Qt::black;
        ActiveColors.Text            = Qt::black;
        ActiveColors.Button          = QColor(240, 240, 240);
        ActiveColors.ButtonText      = Qt::black;
        ActiveColors.BrightText      = Qt::red;
        ActiveColors.Link            = QColor(0, 0, 255);
        ActiveColors.Highlight       = QColor(0, 120, 215);
        ActiveColors.HighlightedText = Qt::white;
    }
    else if (ThemeType == "Windows 10 Dark")
    {
        StyleName       = "windows";
        NeedHeaderStyle = true;
        ActiveColors.Window          = QColor(28, 28, 28);
        ActiveColors.WindowText      = QColor(235, 235, 235);
        ActiveColors.Base            = QColor(32, 32, 32);
        ActiveColors.AlternateBase   = QColor(42, 42, 42);
        ActiveColors.ToolTipBase     = QColor(50, 50, 50);
        ActiveColors.ToolTipText     = QColor(235, 235, 235);
        ActiveColors.Text            = QColor(235, 235, 235);
        ActiveColors.Button          = QColor(40, 40, 40);
        ActiveColors.ButtonText      = QColor(235, 235, 235);
        ActiveColors.BrightText      = Qt::red;
        ActiveColors.Link            = QColor(40, 153, 224);
        ActiveColors.Highlight       = QColor(0, 120, 212);
        ActiveColors.HighlightedText = Qt::white;
    }
    else if (ThemeType == "VGUI")
    {
        StyleName       = "windows";
        NeedHeaderStyle = true;
        ActiveColors.Window          = QColor(48, 62, 48);
        ActiveColors.WindowText      = QColor(200, 220, 180);
        ActiveColors.Base            = QColor(35, 48, 38);
        ActiveColors.AlternateBase   = QColor(42, 55, 45);
        ActiveColors.ToolTipBase     = QColor(70, 90, 70);
        ActiveColors.ToolTipText     = QColor(220, 240, 200);
        ActiveColors.Text            = QColor(200, 220, 180);
        ActiveColors.Button          = QColor(58, 72, 58);
        ActiveColors.ButtonText      = QColor(200, 220, 180);
        ActiveColors.BrightText      = QColor(255, 240, 120);
        ActiveColors.Link            = QColor(130, 190, 130);
        ActiveColors.Highlight       = QColor(90, 140, 90);
        ActiveColors.HighlightedText = QColor(255, 255, 220);
    }
    else  // "Windows 10 White" or default/unknown
    {
        StyleName       = "windowsvista";
        NeedHeaderStyle = true;
        ActiveColors.Window          = QColor(243, 243, 243);
        ActiveColors.WindowText      = Qt::black;
        ActiveColors.Base            = Qt::white;
        ActiveColors.AlternateBase   = QColor(233, 233, 233);
        ActiveColors.ToolTipBase     = QColor(255, 255, 220);
        ActiveColors.ToolTipText     = Qt::black;
        ActiveColors.Text            = Qt::black;
        ActiveColors.Button          = QColor(243, 243, 243);
        ActiveColors.ButtonText      = Qt::black;
        ActiveColors.BrightText      = Qt::red;
        ActiveColors.Link            = QColor(0, 0, 255);
        ActiveColors.Highlight       = QColor(0, 120, 215);
        ActiveColors.HighlightedText = Qt::white;
    }

    QPalette Palette;
    ApplyColorGroup(Palette, QPalette::Active,   ActiveColors);
    ApplyColorGroup(Palette, QPalette::Inactive, MakeInactiveColors(ActiveColors));
    ApplyColorGroup(Palette, QPalette::Disabled, MakeDisabledColors(ActiveColors));

    // setStyle() internally calls oldStyle->unpolish(widget) + newStyle->polish(widget)
    // for every widget. The palette must be set AFTER setStyle() because setStyle()
    // may internally call style->polish(QPalette&) which can reset the application
    // palette to the style's defaults.
    QApplication::setStyle(StyleName);
    QApplication::setPalette(Palette);

    if (NeedHeaderStyle)
        ApplyHeaderPaletteStyle();

    // Force a repaint on all widgets. setPalette() already sends PaletteChange
    // to all top-level widgets (which cascade to children), but QDockWidget
    // internal layering can interrupt propagation in some Qt versions.
    for (QWidget* pWidget : QApplication::allWidgets())
        pWidget->update();
}
