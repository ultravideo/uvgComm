#pragma once

#include <QCamera>

#include <QMainWindow>

#include "filtergraph.h"

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

    FilterGraph video_;
    FilterGraph audio_;
};
