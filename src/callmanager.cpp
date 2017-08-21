#include "callmanager.h"

#include "callnegotiation.h"
#include "statisticsinterface.h"

#include <QHostAddress>
#include <QtEndian>

const uint16_t MAXOPENPORTS = 42;
const uint16_t PORTSPERPARTICIPANT = 4;

CallManager::CallManager(StatisticsInterface *stats):
    media_(stats),
    portsOpen_(0)
{}


void CallManager::init(VideoWidget* selfview)
{
  media_.init(selfview);
}

void CallManager::uninit()
{
  media_.uninit();
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

void CallManager::endCall()
{
  media_.endAllCalls();
  portsOpen_ = 0;
}

void CallManager::micState()
{
  bool state = media_.toggleMic();
/*
  if(state)
  {
    ui_->mic->setText("Mic off");
  }
  else
  {
    ui_->mic->setText("Mic on");
  }
  */
}

void CallManager::cameraState()
{
  bool state = media_.toggleCamera();
  /*
  if(state)
  {
    ui_->camera->setText("Camera off");
  }
  else
  {
    ui_->camera->setText("Camera on");
  }
  */
}
