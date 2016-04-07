#include "callwindow.h"
#include "ui_callwindow.h"

CallWindow::CallWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CallWindow)
{
    ui->setupUi(this);
}

CallWindow::~CallWindow()
{
    delete ui;
}

