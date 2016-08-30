#include "callwindow.h"
#include "ui_callwindow.h"

#include "statisticswindow.h"

#include <QCloseEvent>
#include <QHostAddress>
#include <QtEndian>

#include <QTimer>

CallWindow::CallWindow(QWidget *parent, uint16_t width, uint16_t height) :
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  fg_(),
  participants_(0)
{
  ui_->setupUi(this);
  QSize resolution(width, height);
  fg_.init(ui_->SelfView, resolution);

  timer_ = new QTimer(this);
  timer_->setInterval(10);
  timer_->setSingleShot(false);
  connect(timer_, SIGNAL(timeout()), this, SLOT(update()));
  connect(ui_->stats, SIGNAL(clicked()), this, SLOT(openStatistics()));
  timer_->start();

  stats_ = new StatisticsWindow(this);
}

CallWindow::~CallWindow()
{
  fg_.stop();

  fg_.uninit();

  timer_->stop();
  delete timer_;

  delete stats_;
  delete ui_->videoCall; // TODO delete rest of the video views?
  delete ui_;
}

void CallWindow::addParticipant()
{
  qDebug() << "User wants to add participant to phone call. Num:" << participants_;
  QString ip_str = ui_->ip->toPlainText();
  QString port_str = ui_->port->toPlainText();

  uint16_t nextIp = 0;

  nextIp = port_str.toInt();

  nextIp += 2;

  ui_->port->setText(QString::number(nextIp));

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

  if(stats_)
    stats_->addParticipant(ip_str, port_str);
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  fg_.stop();
  QMainWindow::closeEvent(event);
}

void CallWindow::openStatistics()
{

  stats_->show();
}

