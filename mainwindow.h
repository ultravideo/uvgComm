#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


//private slots:
//    void call();

private:
    Ui::MainWindow *ui;
//    Call *call;
};

#endif // MAINWINDOW_H
