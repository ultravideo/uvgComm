#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include <QCamera>

#include <QMainWindow>

namespace Ui {
class CallWindow;
}

class CameraFrameGrabber;

class CallWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CallWindow(QWidget *parent = 0);
    ~CallWindow();

    void startStream();



private slots:
    void handleFrame(QImage image);


private:
    Ui::CallWindow *ui;
    QCamera *camera;
    CameraFrameGrabber *cameraFrameGrabber_;
};

#endif // CALLWINDOW_H
