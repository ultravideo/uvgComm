#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include <QCamera>

#include <QMainWindow>

namespace Ui {
class CallWindow;
}

class CallWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CallWindow(QWidget *parent = 0);
    ~CallWindow();

    void startStream();


private:
    Ui::CallWindow *ui;
    QCamera *camera;
};

#endif // CALLWINDOW_H
