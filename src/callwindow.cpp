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
  stats_(new StatisticsWindow(this)),
  fg_(stats_),
  participants_(0),
  timer_(new QTimer(this)),
  row_(0),
  column_(0)
{
  ui_->setupUi(this);
  currentResolution_ = QSize(width, height);
  fg_.init(ui_->SelfView, currentResolution_);

  timer_->setInterval(10);
  timer_->setSingleShot(false);
  connect(timer_, SIGNAL(timeout()), this, SLOT(update()));
  connect(ui_->stats, SIGNAL(clicked()), this, SLOT(openStatistics()));
  timer_->start();


}

CallWindow::~CallWindow()
{
  fg_.stop();

  fg_.uninit();

  timer_->stop();
  delete timer_;
  stats_->close();
  delete stats_;
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

  VideoWidget* view = new VideoWidget;
  ui_->participantLayout->addWidget(view, row_, column_);

  ++column_;
  if(column_ == 3)
  {
    column_ = 0;
    ++row_;
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

