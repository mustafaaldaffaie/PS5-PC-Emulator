#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QTranslator>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    BuildUi();

    QSettings settings;
    SetLanguage(settings.value("ui/language", "en").toString());
}

void MainWindow::BuildUi() {
    titleLabel_ = new QLabel(this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    setCentralWidget(titleLabel_);

    languageMenu_ = menuBar()->addMenu(QString());

    englishAction_ = languageMenu_->addAction(QString());
    arabicAction_ = languageMenu_->addAction(QString());

    connect(englishAction_, &QAction::triggered, this, [this]() {
        SetLanguage("en");
    });

    connect(arabicAction_, &QAction::triggered, this, [this]() {
        SetLanguage("ar");
    });

    RetranslateUi();
}

void MainWindow::RetranslateUi() {
    setWindowTitle(tr("PS5-PC-Emulator"));
    titleLabel_->setText(tr("Game Library"));
    languageMenu_->setTitle(tr("Language"));
    englishAction_->setText(tr("English"));
    arabicAction_->setText(tr("Arabic"));
}

void MainWindow::SetLanguage(const QString& localeName) {
    static QTranslator translator;
    qApp->removeTranslator(&translator);

    const QString resourceName = QString(":/i18n/ps5emu_%1.qm").arg(localeName);
    translator.load(resourceName);
    qApp->installTranslator(&translator);

    const bool isArabic = localeName.startsWith("ar", Qt::CaseInsensitive);
    qApp->setLayoutDirection(isArabic ? Qt::RightToLeft : Qt::LeftToRight);

    QSettings settings;
    settings.setValue("ui/language", localeName);

    RetranslateUi();
}
