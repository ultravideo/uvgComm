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
  stats_(new StatisticsWindow(this)),
  conference_(this),
  callNeg_(),
  media_(stats_),
  timer_(new QTimer(this)),
  currentResolution_(),
  portsOpen_(0),
  name_(name)
{
  ui_->setupUi(this);

  currentResolution_ = QSize(width, height);

  // GUI updates are handled solely by timer
  timer_->setInterval(30);
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
  Ui::CallerWidget *ui_widget = new Ui::CallerWidget;
  QWidget* holderWidget = new QWidget;
  ui_widget->setupUi(holderWidget);
  connect(ui_widget->AcceptButton, SIGNAL(clicked()), this, SLOT(acceptCall()));
  connect(ui_widget->DeclineButton, SIGNAL(clicked()), this, SLOT(rejectCall()));

  callNeg_.init(name_);

  QObject::connect(&callNeg_, SIGNAL(incomingINVITE(QString, QString)),
                   this, SLOT(incomingCall(QString, QString)));

  QObject::connect(&callNeg_, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callOurselves(QString, std::shared_ptr<SDPMessageInfo>)));

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

  QObject::connect(&callNeg_, SIGNAL(callEnded(QString, QString)),
                   this, SLOT(endCall(QString, QString)));

  conferenceMutex_.lock();
  conference_.init(ui_->participantLayout, ui_->participants, ui_widget, holderWidget);
  conferenceMutex_.unlock();

  media_.init();
  media_.startCall(ui_->SelfView, currentResolution_);
}


void CallWindow::addParticipant()
{

  if(portsOpen_ <= MAXOPENPORTS)
  {
    QString ip_str = ui_->ip->toPlainText();

    Contact con;
    con.contactAddress = ip_str;
    con.name = "Anonymous";
    if(!name_.isEmpty())
    {
      con.name = name_;
    }
    con.username = "unknown";

    QList<Contact> list;
    list.append(con);

    //start negotiations for this connection

    QList<QString> callIDs = callNeg_.startCall(list);

    for(auto callID : callIDs)
    {
      conferenceMutex_.lock();
      conference_.callingTo(callID, con.name);
      conferenceMutex_.unlock();
    }
  }
}

void CallWindow::createParticipant(QString& callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                                   const std::shared_ptr<SDPMessageInfo> localInfo)
{
  qDebug() << "Adding participant to conference.";

  QHostAddress address;
  address.setAddress(peerInfo->globalAddress);

  in_addr ip;
  ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  conferenceMutex_.lock();
  VideoWidget* view = conference_.addVideoStream(callID);
  conferenceMutex_.unlock();

  if(view == NULL)
  {
    qWarning() << "WARNING: NULL widget got from";
    return;
  }

  // TODO: have these be arrays and send video to all of them in case SDP describes it so
  uint16_t sendAudioPort = 0;
  uint16_t recvAudioPort = 0;
  uint16_t sendVideoPort = 0;
  uint16_t recvVideoPort = 0;

  for(auto media : localInfo->media)
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

  for(auto media : peerInfo->media)
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

  // TODO send all the media if more than one are specified and if required to more than one participant.
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
  media_.addParticipant(callID, ip, sendAudioPort, recvAudioPort, sendVideoPort, recvVideoPort, view);
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
  callNeg_.endAllCalls();

  callNeg_.uninit();

  media_.uninit();

  stats_->hide();
  stats_->finished(0);

  conferenceMutex_.lock();
  conference_.close();
  conferenceMutex_.unlock();

  QMainWindow::closeEvent(event);
}

void CallWindow::incomingCall(QString callID, QString caller)
{
  portsOpen_ +=PORTSPERPARTICIPANT;

  if(portsOpen_ > MAXOPENPORTS)
  {
    qWarning() << "WARNING: Ran out of ports:" << portsOpen_ << "/" << MAXOPENPORTS;
    rejectCall(); // TODO: send a not possible message instead of reject.
    return;
  }

  conferenceMutex_.lock();
  conference_.incomingCall(callID, caller);
  conferenceMutex_.unlock();
}

void CallWindow::callOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info)
{
  portsOpen_ +=PORTSPERPARTICIPANT;

  if(portsOpen_ <= MAXOPENPORTS)
  {
    qDebug() << "Calling ourselves, how boring.";
    createParticipant(callID, info, info);
  }
}

void CallWindow::acceptCall()
{
  qDebug() << "We accepted";
  conferenceMutex_.lock();
  QString callID = conference_.acceptNewest();
  conferenceMutex_.unlock();
  callNeg_.acceptCall(callID);
}

void CallWindow::rejectCall()
{
  qDebug() << "We rejected";
  conferenceMutex_.lock();
  QString callID = conference_.rejectNewest();
  conferenceMutex_.unlock();
  callNeg_.rejectCall(callID);
}

void CallWindow::ringing(QString callID)
{
  qDebug() << "call is ringing. TODO: display it to user";
}

void CallWindow::ourCallRejected(QString callID)
{
  qDebug() << "Our Call was rejected. TODO: display it to user";
  conferenceMutex_.lock();
  conference_.removeCaller(callID);
  conferenceMutex_.unlock();
}

void CallWindow::callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                                std::shared_ptr<SDPMessageInfo> localInfo)
{
  qDebug() << "Our call has been accepted.";

  qDebug() << "Sending media to IP:" << peerInfo->globalAddress
           << "to port:" << peerInfo->media.first().port;

  // TODO check the SDP info and do ports and rtp numbers properly
  createParticipant(callID, peerInfo, localInfo);
}

void CallWindow::endCall(QString callID, QString ip)
{
  qDebug() << "Received the end of call message";

  media_.endCall(callID); // must be ended first because of the view.

  conferenceMutex_.lock();
  conference_.removeCaller(callID);
  conferenceMutex_.unlock();

  stats_->removeParticipant(ip);
}
