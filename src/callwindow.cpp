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

CallWindow::CallWindow(QWidget *parent, uint16_t width, uint16_t height, QString name) :
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  ui_widget_(new Ui::CallerWidget),
  stats_(new StatisticsWindow(this)),
  callingWidget_(new QWidget),
  callNeg_(),
  media_(stats_),
  timer_(new QTimer(this)),
  row_(0),
  column_(0),
  currentResolution_(),
  portsOpen_(0),
  name_(name),
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

  media_.uninit();
}

void CallWindow::startStream()
{
  callNeg_.init(name_);

  QObject::connect(&callNeg_, SIGNAL(incomingINVITE(QString, QString)),
                   this, SLOT(incomingCall(QString, QString)));

  QObject::connect(&callNeg_, SIGNAL(callingOurselves(std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callOurselves(std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                              std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                      std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(ringing(QString)),
                   this, SLOT(ringing(QString)));

  QObject::connect(&callNeg_, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>,
                                                               std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                      std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(ourCallRejected(QString)),
                   this, SLOT(ourCallRejected(QString)));

  QObject::connect(&callNeg_, SIGNAL(callEnded(QString)),
                   this, SLOT(endCall(QString)));

  media_.init();
  media_.startCall(ui_->SelfView, currentResolution_);
}


void CallWindow::addParticipant()
{
  portsOpen_ +=PORTSPERPARTICIPANT;

  if(portsOpen_ <= MAXOPENPORTS)
  {

    QString ip_str = ui_->ip->toPlainText();

    Contact con;

    con.contactAddress = ip_str;
    con.name = "Unknown";
    if(!name_.isEmpty())
    {
      con.name = name_;
    }
    con.username = "unknown";

    QList<Contact> list;
    list.append(con);

    //start negotiations for this connection

    callNeg_.startCall(list);

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

void CallWindow::createParticipant(std::shared_ptr<SDPMessageInfo> peerInfo,
                                   const std::shared_ptr<SDPMessageInfo> localInfo)
{
  qDebug() << "Adding participant to conference.";

  QHostAddress address;
  address.setAddress(peerInfo->globalAddress);

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

  // TODO: have these be arrays and send video to all of them in case SDP describes it so
  uint16_t sendAudioPort = 0;
  uint16_t recvAudioPort = 0;
  uint16_t sendVideoPort = 0;
  uint16_t recvVideoPort = 0;

  for(auto media : peerInfo->media)
  {
    if(media.type == "audio" && recvAudioPort == 0)
    {
      recvAudioPort = media.port;
    }
    else if(media.type == "video" && recvVideoPort == 0)
    {
      recvVideoPort = media.port;
    }
  }

  for(auto media : localInfo->media)
  {
    if(media.type == "audio" && sendAudioPort == 0)
    {
      sendAudioPort = media.port;
    }
    else if(media.type == "video" && sendVideoPort == 0)
    {
      sendVideoPort = media.port;
    }
  }

  if(sendAudioPort == 0 || recvAudioPort == 0 || sendVideoPort == 0 || recvVideoPort == 0)
  {
    qWarning() << "WARNING: All media ports were not found in given SDP. SendAudio:" << sendAudioPort
               << "recvAudio:" << recvAudioPort << "SendVideo:" << sendVideoPort << "RecvVideo:" << recvVideoPort;
    return;
  }

  if(stats_)
    stats_->addParticipant(peerInfo->globalAddress,
                           QString::number(sendAudioPort),
                           QString::number(sendVideoPort));

  qDebug() << "Sending mediastreams to:" << peerInfo->globalAddress << "audioPort:" << sendAudioPort
           << "VideoPort:" << sendVideoPort;
  media_.addParticipant(ip, sendAudioPort, recvAudioPort, sendVideoPort, recvVideoPort, view);
}

void CallWindow::openStatistics()
{
  stats_->show();
}


void CallWindow::micState()
{
  bool state = media_.toggleMic();

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
  bool state = media_.toggleCamera();
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

  callNeg_.uninit();

  media_.uninit();

  stats_->hide();
  stats_->finished(0);

  callingWidget_->hide();

  QMainWindow::closeEvent(event);
}

void CallWindow::incomingCall(QString callID, QString caller)
{
  portsOpen_ +=PORTSPERPARTICIPANT;

  if(portsOpen_ <= MAXOPENPORTS)
  {
    qWarning() << "WARNING: Ran out of ports!!";
    rejectCall(); // TODO: send a not possible message instead of reject.
  }

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
  createParticipant(info, info);
}

void CallWindow::acceptCall()
{
  callingWidget_->hide();

  callWaitingMutex_.lock();
  callNeg_.acceptCall(waitingCalls_.first().callID);
  callWaitingMutex_.unlock();

  processNextWaitingCall();
}

void CallWindow::rejectCall()
{
  callingWidget_->hide();
  hideLabel();

  callWaitingMutex_.lock();
  callNeg_.rejectCall(waitingCalls_.first().callID);
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
      currentCalls_.push_back(waitingCalls_.first());
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
{
  qDebug() << "call is ringing. TODO: display it to user";
}

void CallWindow::ourCallRejected(QString callID)
{
  qDebug() << "Our Call was rejected. TODO: display it to user";
}

void CallWindow::callNegotiated(QString CallID, std::shared_ptr<SDPMessageInfo> peerInfo,
                    std::shared_ptr<SDPMessageInfo> localInfo)
{
  qDebug() << "Our call has been accepted.";

  qDebug() << "Sending media to IP:" << peerInfo->globalAddress
           << "to port:" << peerInfo->media.first().port;

  // TODO check the SDP info and do ports and rtp numbers properly
  createParticipant(peerInfo, localInfo);
}

void CallWindow::endCall(QString callID)
{
  qDebug() << "End call";

  callNeg_.endCall(callID);
}
