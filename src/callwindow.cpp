#include "callwindow.h"
#include "ui_callwindow.h"

#include <QCloseEvent>
#include <QHostAddress>
#include <QtEndian>

#include <QTimer>

CallWindow::CallWindow(QWidget *parent) :
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  fg_(),
  participants_(0)
{
  ui_->setupUi(this);
  QSize resolution(320, 240);
  fg_.init(ui_->SelfView, resolution);

  timer_ = new QTimer(this);
  timer_->setInterval(10);
  timer_->setSingleShot(false);
  connect(timer_, SIGNAL(timeout()), this, SLOT(update()));
  timer_->start();
}

CallWindow::~CallWindow()
{
  fg_.stop();

  fg_.uninit();

  timer_->stop();
  delete timer_;
  delete ui_->videoCall;
  delete ui_;
}

void CallWindow::addParticipant()
{
  qDebug() << "User wants to add participant to phone call. Num:" << participants_;
  QString ip_str = ui_->ip->toPlainText();
  QString port_str = ui_->port->toPlainText();

  QHostAddress address;

  address.setAddress(ip_str);

  in_addr ip;
  ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  VideoWidget * view = NULL;
  switch(participants_)
  {
  case 0:
    view = ui_->videoCall;
    break;
  case 1:
    view = ui_->videoCall_2;
    break;
  case 2:
    view = ui_->videoCall_3;
    break;
  case 3:
    view = ui_->videoCall_4;
    break;
  case 4:
    view = ui_->videoCall_5;
    break;
  case 5:
    view = ui_->videoCall_6;
    break;
  case 6:
    view = ui_->videoCall_7;
    break;
  case 7:
    view = ui_->videoCall_8;
    break;
  default:
    view = NULL;
  }

  fg_.addParticipant(ip, port_str.toInt(), view);

  qDebug() << "Participant" << participants_ << "added";
  ++participants_;

}

void CallWindow::closeEvent(QCloseEvent *event)
{
  fg_.stop();
  QMainWindow::closeEvent(event);
}
