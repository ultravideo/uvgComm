#include "callmanager.h"

#include "statisticsinterface.h"

#include "globalsdpstate.h"
#include "sip/siptypes.h"

#include <QHostAddress>
#include <QtEndian>

CallManager::CallManager():
    media_(),
    sip_(),
    window_(NULL)
{}

void CallManager::init()
{
  window_.init(this);
  window_.show();
  VideoWidget* selfview = window_.getSelfDisplay();

  sip_.init(this);

  // register the GUI signals indicating GUI changes to be handled approrietly in a system wide manner
  QObject::connect(&window_, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
  QObject::connect(&window_, SIGNAL(micStateSwitch()), this, SLOT(micState()));
  QObject::connect(&window_, SIGNAL(cameraStateSwitch()), this, SLOT(cameraState()));
  QObject::connect(&window_, SIGNAL(endCall()), this, SLOT(endTheCall()));
  QObject::connect(&window_, SIGNAL(closed()), this, SLOT(windowClosed()));

  QObject::connect(&window_, &CallWindow::callAccepted, this, &CallManager::acceptCall);
  QObject::connect(&window_, &CallWindow::callRejected, this, &CallManager::rejectCall);

  stats_ = window_.createStatsWindow();

  media_.init(selfview, stats_);

  QObject::connect(&stun_, SIGNAL(addressReceived(QHostAddress)), this, SLOT(stunAddress(QHostAddress)));
  QObject::connect(&stun_, SIGNAL(error()), this, SLOT(noStunAddress()));
  //stun_.wantAddress("stun.l.google.com");
}

void CallManager::stunAddress(QHostAddress address)
{
  qDebug() << "Our stun address:" << address;
}

void CallManager::noStunAddress()
{
  qDebug() << "Could not get STUN address";
}

void CallManager::uninit()
{
  endTheCall();
  sip_.uninit();
  media_.uninit();
}

void CallManager::windowClosed()
{
  uninit();
}

void CallManager::callToParticipant(QString name, QString username, QString ip)
{
  QString ip_str = ip;

  Contact con;
  con.remoteAddress = ip_str;
  con.realName = name;
  con.username = username;
  con.proxyConnection = false;

  QList<Contact> list;
  list.append(con);

  //start negotiations for this connection

  QList<uint32_t> sessionIDs = sip_.startCall(list);

  for(auto sessionID : sessionIDs)
  {
    window_.displayOutgoingCall(sessionID, name);
  }

}

void CallManager::chatWithParticipant(QString name, QString username, QString ip)
{
  qDebug() << "Chatting with:" << name << '(' << username << ") at ip:" << ip << ": Chat not implemented yet";
}

void CallManager::incomingCall(uint32_t sessionID, QString caller)
{
  window_.displayIncomingCall(sessionID, caller);
}

void CallManager::callRinging(uint32_t sessionID)
{
  qDebug() << "Our call is ringing!";
  window_.displayRinging(sessionID);
}

void CallManager::callRejected(uint32_t sessionID)
{
  qDebug() << "Our call has been rejected!";
  window_.removeParticipant(sessionID);
}

void CallManager::callNegotiated(uint32_t sessionID)
{
  qDebug() << "Call has been agreed upon with peer:" << sessionID;

  VideoWidget* view = window_.addVideoStream(sessionID);

  std::shared_ptr<SDPMessageInfo> localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP;

  sip_.getSDPs(sessionID,
               localSDP,
               remoteSDP);

  if(localSDP == NULL || remoteSDP == NULL)
  {
    return;
  }

  // TODO check the SDP info and do ports and rt numbers properly
  createParticipant(sessionID, remoteSDP, localSDP, view, stats_);
}

void CallManager::callNegotiationFailed(uint32_t sessionID)
{
  // TODO: display a proper error for the user
  callRejected(sessionID);
}

void CallManager::cancelIncomingCall(uint32_t sessionID)
{
  // TODO: display a proper message to the user
  window_.removeParticipant(sessionID);
}

void CallManager::endCall(uint32_t sessionID)
{
  media_.removeParticipant(sessionID);
  window_.removeParticipant(sessionID);
}

void CallManager::registeredToServer()
{
  qDebug() << "Got info, that we have been registered to SIP server.";
  // TODO: indicate to user in some small detail
}

void CallManager::registeringFailed()
{
  qDebug() << "Failed to register";
  // TODO: indicate error to user
}

void CallManager::updateSettings()
{
  media_.updateSettings();
}

void CallManager::createParticipant(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> peerInfo,
                                    const std::shared_ptr<SDPMessageInfo> localInfo,
                                    VideoWidget* videoWidget,
                                    StatisticsInterface* stats)
{

  if(videoWidget == NULL)
  {
    qWarning() << "WARNING: NULL widget got from";
    return;
  }

  QHostAddress address;
  address.setAddress(peerInfo->connection_address);

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
      recvAudioPort = media.receivePort;
    }
    else if(media.type == "video" && recvVideoPort == 0)
    {
      recvVideoPort = media.receivePort;
    }
  }

  for(auto media : peerInfo->media)
  {
    if(media.type == "audio" && sendAudioPort == 0)
    {
      sendAudioPort = media.receivePort;
    }
    else if(media.type == "video" && sendVideoPort == 0)
    {
      sendVideoPort = media.receivePort;
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
    stats->addParticipant(peerInfo->connection_address,
                          QString::number(sendAudioPort),
                          QString::number(sendVideoPort));

  qDebug() << "Sending mediastreams to:" << peerInfo->connection_address << "audioPort:" << sendAudioPort
           << "VideoPort:" << sendVideoPort;
  media_.addParticipant(sessionID, ip, sendAudioPort, recvAudioPort, sendVideoPort, recvVideoPort, videoWidget);
}


void CallManager::acceptCall(uint32_t sessionID)
{
  qDebug() << "Sending accept";
  sip_.acceptCall(sessionID);
}

void CallManager::rejectCall(uint32_t sessionID)
{
  qDebug() << "We have rejected their call";
  sip_.rejectCall(sessionID);
  window_.removeParticipant(sessionID);
}

void CallManager::endTheCall()
{
  qDebug() << "Ending all calls";
  sip_.endAllCalls();
  media_.endAllCalls();
  window_.clearConferenceView();
}

void CallManager::micState()
{
  window_.setMicState(media_.toggleMic());
}

void CallManager::cameraState()
{
  window_.setCameraState(media_.toggleCamera());
}
