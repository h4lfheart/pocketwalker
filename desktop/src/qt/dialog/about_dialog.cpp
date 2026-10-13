#include "about_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QString>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("About PocketWalker");
    setMinimumWidth(400);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    auto* logo = new QLabel(this);
    const QPixmap icon(":/images/logo.png");
    logo->setPixmap(icon.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setFixedSize(96, 96);
    logo->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("PocketWalker", this);
    QFont title_font = title->font();
    title_font.setPointSize(24);
    title_font.setBold(true);
    title->setFont(title_font);

    auto* version = new QLabel(QStringLiteral("%1").arg(POCKETWALKER_VERSION), this);

    auto* authors = new QLabel(
        R"(By <a href="https://github.com/h4lfheart">Half</a> and <a href="https://github.com/h4lfheart/pocketwalker/graphs/contributors">contributors</a>.)", this);
    authors->setOpenExternalLinks(true);

    auto* legal = new QLabel(
        "PocketWalker is not affiliated with or endorsed by Nintendo.", this);
    legal->setWordWrap(true);
    legal->setEnabled(false);
    QFont legal_font = legal->font();
    legal_font.setPointSize(9);
    legal->setFont(legal_font);

    auto* info_layout = new QVBoxLayout();
    info_layout->setSpacing(8);
    info_layout->addWidget(title, 0, Qt::AlignTop);
    info_layout->addWidget(version);
    info_layout->addWidget(authors);
    info_layout->addStretch();
    info_layout->addWidget(legal, 0, Qt::AlignBottom);

    auto* top_layout = new QHBoxLayout();
    top_layout->setSpacing(24);
    top_layout->addWidget(logo, 0, Qt::AlignTop);
    top_layout->addLayout(info_layout);

    auto* github_btn = new QPushButton("GitHub", this);
    github_btn->setFlat(true);
    connect(github_btn, &QPushButton::clicked, this, []
    {
        QDesktopServices::openUrl(QUrl("https://github.com/h4lfheart/pocketwalker"));
    });

    auto* close_btn = new QPushButton("Close", this);
    close_btn->setDefault(true);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);

    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(12);
    btn_layout->addStretch();
    btn_layout->addWidget(github_btn);
    btn_layout->addWidget(close_btn);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->addLayout(top_layout);
    layout->addSpacing(12);
    layout->addLayout(btn_layout);
}