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
  filterIniated_(false),
  timer_(new QTimer(this)),
  row_(0),
  column_(0)
{
  ui_->setupUi(this);
  currentResolution_ = QSize(width, height);

  // GUI updates are handled solely by timer
  timer_->setInterval(10);
  timer_->setSingleShot(false);
  connect(timer_, SIGNAL(timeout()), this, SLOT(update()));
  connect(timer_, SIGNAL(timeout()), stats_, SLOT(update()));
  timer_->start();

  connect(ui_->stats, SIGNAL(clicked()), this, SLOT(openStatistics()));
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

void CallWindow::startStream()
{
  if(!filterIniated_)
  {
    fg_.init(ui_->SelfView, currentResolution_);
    filterIniated_ = true;
  }
}

void CallWindow::addParticipant()
{
  startStream();

  QString ip_str = ui_->ip->toPlainText();
  QString port_str = ui_->port->toPlainText();

  uint16_t nextIp = 0;

  nextIp = port_str.toInt();
  nextIp += 2; // increase port for convencience

  ui_->port->setText(QString::number(nextIp));

  QHostAddress address;
  address.setAddress(ip_str);

  in_addr ip;
  ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  VideoWidget* view = new VideoWidget;
  ui_->participantLayout->addWidget(view, row_, column_);

  // TODO improve this algorithm
  ++column_;
  if(column_ == 3)
  {
    column_ = 0;
    ++row_;
  }

  fg_.addParticipant(ip, port_str.toInt(), view, true, true, true, true);

  if(stats_)
    stats_->addParticipant(ip_str, port_str);
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  fg_.stop();
  stats_->hide();
  stats_->finished(0);
  QMainWindow::closeEvent(event);
}

void CallWindow::openStatistics()
{
  stats_->show();
}

