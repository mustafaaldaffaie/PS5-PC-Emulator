#pragma once

#include <QMainWindow>

class QAction;
class QLabel;
class QMenu;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void BuildUi();
    void RetranslateUi();
    void SetLanguage(const QString& localeName);

    QLabel* titleLabel_ = nullptr;
    QMenu* languageMenu_ = nullptr;
    QAction* englishAction_ = nullptr;
    QAction* arabicAction_ = nullptr;
};
