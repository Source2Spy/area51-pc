//==============================================================================
//
//  AboutDialog.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "AboutDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

AboutDialog::AboutDialog(const QString& GameType, const QString& ThemeType, QWidget* pParent)
    : QDialog(pParent)
    , m_pInfo(NULL)
{
    setWindowTitle("About");
    setMinimumSize(520, 340);

    QLabel* pBanner = new QLabel("XBMPViewer", this);
    QFont BannerFont = pBanner->font();
    BannerFont.setPointSize(BannerFont.pointSize() + 10);
    BannerFont.setBold(TRUE);
    pBanner->setFont(BannerFont);
    pBanner->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_pInfo = new QPlainTextEdit(this);
    m_pInfo->setReadOnly(TRUE);

    QString InfoText;
    InfoText += "Product Name: XBMPViewer\n";
    InfoText += QString("Build: %1 %2\n").arg(__DATE__).arg(__TIME__);
    InfoText += QString("Game: %1\n").arg(GameType);
    InfoText += QString("Theme: %1\n").arg(ThemeType);
    m_pInfo->setPlainText(InfoText);

    QDialogButtonBox* pButtons = new QDialogButtonBox(this);
    QPushButton* pCopy = pButtons->addButton("Copy Info", QDialogButtonBox::ActionRole);
    QPushButton* pOk = pButtons->addButton(QDialogButtonBox::Ok);
    connect(pCopy, &QPushButton::clicked, this, &AboutDialog::OnCopyInfo);
    connect(pOk, &QPushButton::clicked, this, &QDialog::accept);

    QVBoxLayout* pLayout = new QVBoxLayout(this);
    pLayout->addWidget(pBanner);
    pLayout->addWidget(m_pInfo);
    pLayout->addWidget(pButtons);
}

//==============================================================================

void AboutDialog::OnCopyInfo(void)
{
    if (!m_pInfo)
        return;
    QApplication::clipboard()->setText(m_pInfo->toPlainText());
}
