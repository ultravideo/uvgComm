#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "callwindow.h"


#include <QHostAddress>
#include <QtEndian>


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
  ui_ = 0;
  delete call_;
  call_ = 0;
}

void MainWindow::startCall()
{
  if(call_)
  {
    delete call_;
    call_ = NULL;
  }

  QString ip_str = ui_->ip->toPlainText();
  QString port_str = ui_->port->toPlainText();

  QHostAddress address;

  address.setAddress(ip_str);

  in_addr ip;
  ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  call_ = new CallWindow(this,  ip, port_str.toInt());
  call_->show();
}
