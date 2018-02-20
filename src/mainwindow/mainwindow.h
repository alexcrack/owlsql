#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include "session_manager/sessionmanager.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void closeEvent(QCloseEvent *event);

private slots:
    void on_actionSession_manager_triggered();

private:
    Ui::MainWindow *ui;

    QSettings settings;
    SessionManager sessionManager;

    void saveWindowParameters();
    void loadWindowParameters();

    void makeToolbarsAndMenues();
    void makeConnectionButtons();
    void makeRefreshButtons();
    void makeRunButtons();
};

#endif // MAINWINDOW_H
