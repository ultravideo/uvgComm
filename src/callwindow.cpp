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
  ui_widget_(new Ui::CallerWidget),
  stats_(new StatisticsWindow(this)),
  callingWidget_(new QWidget),
  call_neg_(),
  call_(stats_),
  timer_(new QTimer(this)),
  row_(0),
  column_(0),
  currentResolution_(),
  portsOpen_(0),
  ringing_(false)
{
  ui_->setupUi(this);
  ui_widget_->setupUi(callingWidget_);

  currentResolution_ = QSize(width, height);
  connect(ui_widget_->AcceptButton, SIGNAL(clicked()), this, SLOT(acceptCall()));
  connect(ui_widget_->DeclineButton, SIGNAL(clicked()), this, SLOT(rejectCall()));

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

  call_.uninit();
}

void CallWindow::startStream()
{
  call_neg_.init();

  QObject::connect(&call_neg_, SIGNAL(incomingINVITE(QString, QString)),
                   this, SLOT(incomingCall(QString, QString)));

  QObject::connect(&call_neg_, SIGNAL(callingOurselves(std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callOurselves(std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&call_neg_, SIGNAL(callNegotiated(std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callOurselves()));

  QObject::connect(&call_neg_, SIGNAL(ringing(QString)),
                   this, SLOT(ringing(QString)));

  QObject::connect(&call_neg_, SIGNAL(ourCallAccepted(std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(ourCallAccepted(std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&call_neg_, SIGNAL(ourCallRejected(QString CallID)),
                   this, SLOT(ourCallRejected(QString CallID)));

  QObject::connect(&call_neg_, SIGNAL(callEnded()),
                   this, SLOT(endCall()));

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

    con.contactAddress = ip_str;
    con.name = "Unknown";
    con.username = "unknown";

    QList<Contact> list;
    list.append(con);

    //start negotiations for this connection

    QString sdp = "";
    call_neg_.startCall(list, sdp);

    QLabel* label = new QLabel(this);
    label->setText("Calling...");

    QFont font = QFont("Times", 16);
    label->setFont(font);
    label->setAlignment(Qt::AlignHCenter);
    ui_->participantLayout->addWidget(label, row_, column_);

  }
}

void CallWindow::hideLabel()
{
  QLayoutItem* label = ui_->participantLayout->itemAtPosition(row_,column_);
  if(label)
    label->widget()->hide();
}

void CallWindow::createParticipant(QString ip_str, uint16_t port)
{
  qDebug() << "Adding participant to conference.";
/*
  uint16_t nextIp = 0;

  nextIp = port_str.toInt();
  nextIp += 2; // increase port for convencience

  ui_->port->setText(QString::number(nextIp));
*/
  QHostAddress address;
  address.setAddress(ip_str);

  in_addr ip;
  ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  hideLabel();

  VideoWidget* view = new VideoWidget;
  ui_->participantLayout->addWidget(view, row_, column_);

  // TODO improve this algorithm for more optimized layout
  ++column_;
  if(column_ == 3)
  {
    column_ = 0;
    ++row_;
  }

  call_.addParticipant(ip, port, view);

  if(stats_)
    stats_->addParticipant(ip_str, QString(port));
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

  callingWidget_->hide();

  QMainWindow::closeEvent(event);
}

void CallWindow::incomingCall(QString callID, QString caller)
{
  callWaitingMutex_.lock();
  waitingCalls_.append({callID,caller});
  callWaitingMutex_.unlock();

  if(!ringing_)
  {
    qDebug() << "Displaying pop-up for somebody calling";
    ui_widget_->CallerLabel->setText(caller + " is calling..");
    callingWidget_->show();
  }
}

void CallWindow::callOurselves(std::shared_ptr<SDPMessageInfo> info)
{
  qDebug() << "Calling ourselves, how boring.";
  createParticipant(info->globalAddress, info->media.first().port);
}

void CallWindow::acceptCall()
{
  callingWidget_->hide();

  callWaitingMutex_.lock();
  call_neg_.acceptCall(waitingCalls_.first().callID);
  callWaitingMutex_.unlock();

  processNextWaitingCall();
}

void CallWindow::rejectCall()
{
  callingWidget_->hide();
  hideLabel();

  callWaitingMutex_.lock();
  call_neg_.rejectCall(waitingCalls_.first().callID);
  callWaitingMutex_.unlock();

  processNextWaitingCall();
}

void CallWindow::processNextWaitingCall()
{
  if(!ringing_)
  {
    callWaitingMutex_.lock();

    if(waitingCalls_.size() > 0)
    {
      waitingCalls_.removeFirst();

      if(waitingCalls_.size() > 0)
      {
        ui_widget_->CallerLabel->setText(waitingCalls_.first().name + " is calling..");
        callingWidget_->show();
        ringing_ = true;
      }
      else
      {
        callingWidget_->hide();
        ringing_ = false;
      }
    }
    callWaitingMutex_.unlock();
  }
}

void CallWindow::ringing(QString callID)
{}

void CallWindow::ourCallAccepted(std::shared_ptr<SDPMessageInfo> info)
{
  qDebug() << "Our call has been accepted.";

  qDebug() << "Sending media to IP:" << info->globalAddress
           << "to port:" << info->media.first().port;

  // TODO check the SDP info and do ports and rtp numbers properly
  createParticipant(info->globalAddress, info->media.first().port);
}

void CallWindow::ourCallRejected(QString callID)
{
  qDebug() << "Our Call was rejected. TODO: display it to user";
}

void CallWindow::theirCallNegotiated(std::shared_ptr<SDPMessageInfo> info)
{
  qDebug() << "Their call has been negotiated.";

  qDebug() << "Sending media to IP:" << info->globalAddress
           << "to port:" << info->media.first().port;

  // TODO check the SDP info and do ports and rtp numbers properly
  createParticipant(info->globalAddress, info->media.first().port);
}

void CallWindow::endCall(QString callID)
{
  qDebug() << "End call";
}
