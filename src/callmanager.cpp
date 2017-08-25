#include "callmanager.h"

#include "callnegotiation.h"
#include "statisticsinterface.h"

#include <QHostAddress>
#include <QtEndian>
#include <QSettings>

CallManager::CallManager():
    media_(),
    callNeg_(),
    window_(NULL),
    portsOpen_(0)
{}

void CallManager::init()
{
  window_.init(this);
  window_.show();
  VideoWidget* selfview = window_.getSelfDisplay();

  // TODO move these closer to useagepoint
  QSettings settings;
  QString localName = settings.value("local/Name").toString();
  QString localUsername = settings.value("local/Username").toString();

  callNeg_.init(localName, localUsername);

  QObject::connect(&callNeg_, SIGNAL(incomingINVITE(QString, QString)),
                   this, SLOT(incomingCall(QString, QString)));

  QObject::connect(&callNeg_, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callOurselves(QString, std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                              std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                      std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(ringing(QString)),
                   this, SLOT(callRinging(QString)));

  QObject::connect(&callNeg_, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>,
                                                               std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                      std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(ourCallRejected(QString)),
                   this, SLOT(callRejected(QString)));

  QObject::connect(&callNeg_, SIGNAL(callEnded(QString, QString)),
                   this, SLOT(callEnded(QString, QString)));

  QObject::connect(&window_, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
  QObject::connect(&window_, SIGNAL(micStateSwitch()), this, SLOT(micState()));
  QObject::connect(&window_, SIGNAL(cameraStateSwitch()), this, SLOT(cameraState()));
  QObject::connect(&window_, SIGNAL(endCall()), this, SLOT(endTheCall()));
  QObject::connect(&window_, SIGNAL(closed()), this, SLOT(windowClosed()));

  QObject::connect(&window_, SIGNAL(callAccepted(QString)), this, SLOT(acceptCall(QString)));
  QObject::connect(&window_, SIGNAL(callRejected(QString)), this, SLOT(rejectCall(QString)));

  stats_ = window_.createStatsWindow();

  media_.init(selfview, stats_);
}

void CallManager::uninit()
{
  endTheCall();
  callNeg_.uninit();
  media_.uninit();
}

void CallManager::windowClosed()
{
  uninit();
}

void CallManager::callToParticipant(QString name, QString username, QString ip)
{
  if(roomForMoreParticipants())
  {
    QString ip_str = ip;

    Contact con;
    con.contactAddress = ip_str;
    con.name = name;
    con.username = username;

    QList<Contact> list;
    list.append(con);

    //start negotiations for this connection

    QList<QString> callIDs = callNeg_.startCall(list);

    for(auto callID : callIDs)
    {
      window_.displayOutgoingCall(callID, name);
    }
  }
  else
  {
    qDebug() << "No room for more participants.";
  }
}

void CallManager::callOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info)
{
  if(roomForMoreParticipants())
  {
    qDebug() << "Calling ourselves, how boring.";
    VideoWidget* view = window_.addVideoStream(callID);
    createParticipant(callID, info, info, view, stats_);
  }
  else
  {
    qDebug() << "WARNING: No Room when calling ourselves, but this should be checked earlier.";
  }
}

void CallManager::chatWithParticipant(QString name, QString username, QString ip)
{
  qDebug("Chat not implemented yet");
}

void CallManager::incomingCall(QString callID, QString caller)
{
  if(!roomForMoreParticipants())
  {
    qWarning() << "WARNING: Could not fit more participants to this call";
    //rejectCall(); // TODO: send a not possible message instead of reject.
    return;
  }

  window_.displayIncomingCall(callID, caller);
}

void CallManager::updateSettings()
{
  // TODO call update settings on everything?
  media_.updateSettings();
}

bool CallManager::roomForMoreParticipants() const
{
  return portsOpen_ + media_.portsPerParticipant() <= media_.maxOpenPorts();
}

void CallManager::createParticipant(QString& callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                                    const std::shared_ptr<SDPMessageInfo> localInfo,
                                    VideoWidget* videoWidget,
                                    StatisticsInterface* stats)
{
  qDebug() << "User wants to add participant. Ports required:"
           << portsOpen_ + media_.portsPerParticipant() << "/" << media_.maxOpenPorts();
  Q_ASSERT(roomForMoreParticipants());

  if(videoWidget == NULL)
  {
    qWarning() << "WARNING: NULL widget got from";
    return;
  }

  portsOpen_ +=media_.portsPerParticipant();

  QHostAddress address;
  address.setAddress(peerInfo->globalAddress);

  in_addr ip;
  ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

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

  if(stats)
    stats->addParticipant(peerInfo->globalAddress,
                          QString::number(sendAudioPort),
                          QString::number(sendVideoPort));

  qDebug() << "Sending mediastreams to:" << peerInfo->globalAddress << "audioPort:" << sendAudioPort
           << "VideoPort:" << sendVideoPort;
  media_.addParticipant(callID, ip, sendAudioPort, recvAudioPort, sendVideoPort, recvVideoPort, videoWidget);
}

void CallManager::callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                                std::shared_ptr<SDPMessageInfo> localInfo)
{
  qDebug() << "Our call has been accepted.";

  qDebug() << "Sending media to IP:" << peerInfo->globalAddress
           << "to port:" << peerInfo->media.first().port;

  VideoWidget* view = window_.addVideoStream(callID);

  // TODO check the SDP info and do ports and rtp numbers properly
  createParticipant(callID, peerInfo, localInfo, view, stats_);
}

void CallManager::acceptCall(QString callID)
{
  callNeg_.acceptCall(callID);
}

void CallManager::rejectCall(QString callID)
{
  callNeg_.rejectCall(callID);
}

void CallManager::endTheCall()
{
  qDebug() << "Ending all calls";
  media_.endAllCalls();
  window_.clearConferenceView();
  portsOpen_ = 0;
}

void CallManager::micState()
{
  window_.setMicState(media_.toggleMic());
}

void CallManager::cameraState()
{
  window_.setCameraState(media_.toggleCamera());
}

void CallManager::callRinging(QString callID)
{
  qDebug() << "Our call is ringing!";
  window_.displayRinging(callID);
}

void CallManager::callRejected(QString callID)
{
  qDebug() << "Our call has been rejected!";
  window_.removeParticipant(callID);
}

void CallManager::callEnded(QString callID, QString ip)
{
  qDebug() << "They have left the call. Ports in use:" << portsOpen_
           << "->" << portsOpen_ - media_.portsForCallID(callID);

  media_.removeParticipant(callID);
  window_.removeParticipant(callID);
  stats_->removeParticipant(ip);

  Q_ASSERT(portsOpen_ >= media_.portsForCallID(callID));
  portsOpen_ -= media_.portsForCallID(callID);
}
