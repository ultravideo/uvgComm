#include "callwindow.h"
#include "ui_callwindow.h"


#include <QCameraViewfinder>
#include <QCameraInfo>



CallWindow::CallWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CallWindow)
{
    ui->setupUi(this);

    camera = new QCamera(QCameraInfo::defaultCamera());

    camera->setViewfinder(ui->viewFinder);

    camera->start();
}

CallWindow::~CallWindow()
{
    delete ui;
}

