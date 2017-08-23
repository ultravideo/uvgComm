#include "callmanager.h"

#include "callnegotiation.h"
#include "statisticsinterface.h"

#include <QHostAddress>
#include <QtEndian>
#include <QSettings>

const uint16_t MAXOPENPORTS = 42;
const uint16_t PORTSPERPARTICIPANT = 4;

CallManager::CallManager():
    media_(),
    callNeg_(),
    window_(NULL),
    portsOpen_(0)
{}


void CallManager::init()
{
  window_.init();
  window_.registerGUIEndpoints(this);
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
                   this, SLOT(ringing(QString)));

  QObject::connect(&callNeg_, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>,
                                                               std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                      std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(ourCallRejected(QString)),
                   this, SLOT(ourCallRejected(QString)));

  QObject::connect(&callNeg_, SIGNAL(callEnded(QString, QString)),
                   this, SLOT(endCall(QString, QString)));

  QObject::connect(&window_, SIGNAL(closed()), this, SLOT(windowClosed()));

  stats_ = window_.createStatsWindow();

  media_.init(selfview, stats_);
}

void CallManager::uninit()
{
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
    con.name = "Anonymous";
    con.username = "unknown";

    QList<Contact> list;
    list.append(con);

    //start negotiations for this connection

    QList<QString> callIDs = callNeg_.startCall(list);

    for(auto callID : callIDs)
    {
      window_.callingTo(callID);
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

  window_.incomingCallNotification(callID, caller);
}

void CallManager::recordChangedSettings()
{
  // TODO call update settings on everything?
  media_.updateSettings();
}

bool CallManager::roomForMoreParticipants() const
{
  return portsOpen_ + PORTSPERPARTICIPANT <= MAXOPENPORTS;
}

void CallManager::createParticipant(QString& callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                                    const std::shared_ptr<SDPMessageInfo> localInfo,
                                    VideoWidget* videoWidget,
                                    StatisticsInterface* stats)
{
  qDebug() << "User wants to add participant. Ports required:"
           << portsOpen_ + PORTSPERPARTICIPANT << "/" << MAXOPENPORTS;
  Q_ASSERT(roomForMoreParticipants());

  if(videoWidget == NULL)
  {
    qWarning() << "WARNING: NULL widget got from";
    return;
  }

  portsOpen_ +=PORTSPERPARTICIPANT;

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

void CallManager::removeParticipant(QString callID)
{
  media_.removeParticipant(callID); // must be ended first because of the view.
  portsOpen_ -= PORTSPERPARTICIPANT;
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

void CallManager::endCall()
{
  media_.endAllCalls();
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
