#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "callwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    call(NULL)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::startCall()
{
    call = new CallWindow();
    call->show();
}
