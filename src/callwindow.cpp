#include "callwindow.h"
#include "ui_callwindow.h"

#include <QCloseEvent>

CallWindow::CallWindow(QWidget *parent,
                       in_addr ip, uint16_t port) :
  QMainWindow(parent),
  ui(new Ui::CallWindow)
{
  ui->setupUi(this);

  video_.constructVideoGraph(ui->videoCall, ip,  port);
  //audio_.constructAudioGraph();

  video_.run();
  //audio_.run();
}

CallWindow::~CallWindow()
{
  video_.stop();
  //audio_.stop();

  video_.deconstruct();
  //audio_.deconstruct();

  delete ui->videoCall;
  delete ui;
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  video_.stop();
  //audio_.stop();
  QMainWindow::closeEvent(event);
}
