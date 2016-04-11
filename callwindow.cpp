#include "callwindow.h"
#include "ui_callwindow.h"


#include <QCameraViewfinder>
#include <QCameraInfo>

#include "cameraframegrabber.h"


CallWindow::CallWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CallWindow)
{
    ui->setupUi(this);

    camera = new QCamera(QCameraInfo::defaultCamera());
    cameraFrameGrabber_ = new CameraFrameGrabber();
//    camera->setViewfinder(ui->viewFinder);
    camera->setViewfinder(cameraFrameGrabber_);

    connect(cameraFrameGrabber_, SIGNAL(frameAvailable(QImage)), this, SLOT(handleFrame(QImage)));

    camera->start();

    video_.constructVideoGraph();
    audio_.constructAudioGraph();

    video_.run();
    audio_.run();

}

CallWindow::~CallWindow()
{
    video_.stop();
    audio_.stop();

    video_.deconstruct();
    audio_.deconstruct();
    delete ui;
}

void CallWindow::handleFrame(QImage image)
{

}
