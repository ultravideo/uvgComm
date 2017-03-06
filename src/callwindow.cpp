#include "callwindow.h"
#include "ui_callwindow.h"

#include "ui_callingwidget.h"

#include "statisticswindow.h"

#include <QCloseEvent>
#include <QHostAddress>
#include <QtEndian>
#include <QTimer>


const uint16_t MAXOPENPORTS = 42;
const uint16_t PORTSPERPARTICIPANT = 4;


CallWindow::CallWindow(QWidget *parent, uint16_t width, uint16_t height) :
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  widget_(new Ui::CallerWidget),
  stats_(new StatisticsWindow(this)),
  call_(stats_),
  call_neg_(),
  callingWidget_(new QWidget),
  timer_(new QTimer(this)),
  row_(0),
  column_(0),
  currentResolution_(),
  portsOpen_(0)
{
  ui_->setupUi(this);
  widget_->setupUi(callingWidget_);

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
  timer_->stop();
  delete timer_;
  stats_->close();
  delete stats_;
  delete ui_;
}

void CallWindow::startStream()
{
  call_neg_.init();

  QObject::connect(&call_neg_, SIGNAL(incomingINVITE(QString)), this, SLOT(incomingCall(QString)));

  call_.init();
  call_.startCall(ui_->SelfView, currentResolution_);
}

void CallWindow::addParticipant()
{
  portsOpen_ +=PORTSPERPARTICIPANT;

  if(portsOpen_ <= MAXOPENPORTS)
  {

    QString ip_str = ui_->ip->toPlainText();
    QString port_str = ui_->port->toPlainText();

    Contact con;

    con.address = QString(ip_str);
    con.name = "somebody";

    QList<Contact> list;
    list.append(con);

    call_neg_.startCall(list, "");

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

    // TODO improve this algorithm for more optimized layout
    ++column_;
    if(column_ == 3)
    {
      column_ = 0;
      ++row_;
    }

    call_.addParticipant(ip, port_str.toInt(), view);

    if(stats_)
      stats_->addParticipant(ip_str, port_str);
  }
}

void CallWindow::openStatistics()
{
  stats_->show();
}


void CallWindow::micState()
{
  bool state = call_.toggleMic();

  if(state)
  {
    ui_->mic->setText("Turn mic off");
  }
  else
  {
    ui_->mic->setText("Turn mic on");
  }
}

void CallWindow::cameraState()
{
  bool state = call_.toggleCamera();
  if(state)
  {
    ui_->camera->setText("Turn camera off");
  }
  else
  {
    ui_->camera->setText("Turn camera on");
  }
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  call_.uninit();

  stats_->hide();
  stats_->finished(0);
  //delete stats_;
  //stats_ = 0;
  QMainWindow::closeEvent(event);
}


void CallWindow::incomingCall(QString caller)
{
  callingWidget_->show();
}
