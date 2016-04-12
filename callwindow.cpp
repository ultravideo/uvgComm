#include "callwindow.h"
#include "ui_callwindow.h"

#include <QCloseEvent>

CallWindow::CallWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::CallWindow)
{
  ui->setupUi(this);

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

void CallWindow::closeEvent(QCloseEvent *event)
{
  video_.stop();
  audio_.stop();
  QMainWindow::closeEvent(event);
}
