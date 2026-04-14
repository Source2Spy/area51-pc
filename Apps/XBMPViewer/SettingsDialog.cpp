//==============================================================================
//
//  SettingsDialog.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

SettingsDialog::SettingsDialog(QWidget* pParent)
    : QDialog(pParent)
    , m_pGameType(NULL)
    , m_pThemeType(NULL)
{
    setWindowTitle("Settings");

    m_pGameType = new QComboBox(this);
    m_pGameType->addItem("Area-51");
    m_pGameType->addItem("The Hobbit");
    m_pGameType->addItem("Tribes:AA");

    m_pThemeType = new QComboBox(this);
    //m_pThemeType->addItem("Fusion White");
    //m_pThemeType->addItem("Fusion Dark");
    m_pThemeType->addItem("Windows White");
    m_pThemeType->addItem("Windows Dark");
    m_pThemeType->addItem("Windows 10 White");
    m_pThemeType->addItem("Windows 10 Dark");
    m_pThemeType->addItem("VGUI");

    QFormLayout* pFormLayout = new QFormLayout();
    pFormLayout->addRow("Game", m_pGameType);
    pFormLayout->addRow("Theme", m_pThemeType);

    QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* pLayout = new QVBoxLayout(this);
    pLayout->addLayout(pFormLayout);
    pLayout->addWidget(pButtons);
}

//==============================================================================

void SettingsDialog::SetGameType(const QString& GameType)
{
    const s32 Index = m_pGameType->findText(GameType);
    if (Index >= 0)
        m_pGameType->setCurrentIndex(Index);
}

//==============================================================================

void SettingsDialog::SetThemeType(const QString& ThemeType)
{
    const s32 Index = m_pThemeType->findText(ThemeType);
    if (Index >= 0)
        m_pThemeType->setCurrentIndex(Index);
}

//==============================================================================

QString SettingsDialog::GetGameType(void) const
{
    return m_pGameType->currentText();
}

//==============================================================================

QString SettingsDialog::GetThemeType(void) const
{
    return m_pThemeType->currentText();
}