#include "callwindow.h"
#include "ui_callwindow.h"

#include <QCloseEvent>

CallWindow::CallWindow(QWidget *parent,
                       in_addr ip, uint16_t port) :
  QMainWindow(parent),
  ui(new Ui::CallWindow)
{
  ui->setupUi(this);

  fg_.constructVideoGraph(ui->videoCall, ip,  port);
  //fg_.constructAudioGraph();

  fg_.run();
}

CallWindow::~CallWindow()
{
  fg_.stop();

  fg_.deconstruct();

  delete ui->videoCall;
  delete ui;
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  fg_.stop();
  QMainWindow::closeEvent(event);
}
