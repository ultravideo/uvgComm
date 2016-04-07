#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "callwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui_(new Ui::MainWindow),
    call_(NULL)
{
    ui_->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui_;
    delete call_;
}

void MainWindow::startCall()
{
    if(call_)
    {
        delete call_;
        call_ = NULL;
    }
    call_ = new CallWindow();
    call_->show();
}
