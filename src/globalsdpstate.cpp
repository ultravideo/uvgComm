#include "globalsdpstate.h"

#include <QDateTime>

#include <QUdpSocket>

GlobalSDPState::GlobalSDPState():
  localUsername_(""),
  remainingPorts_(0)
{}

void GlobalSDPState::setLocalInfo(QString username)
{
  localUsername_ = username;
}

void GlobalSDPState::setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections)
{
  for(unsigned int i = minport; i < maxport; i += 2)
  {
    makePortPairAvailable(i);
  }

  remainingPorts_ = maxRTPConnections;
}

std::shared_ptr<SDPMessageInfo> GlobalSDPState::localSDPSuggestion(QHostAddress localAddress)
{
  return generateSDP(localAddress);
}

std::shared_ptr<SDPMessageInfo> GlobalSDPState::generateSDP(QHostAddress localAddress)
{
  // TODO: This should ask media manager, what options it supports.

  if(localAddress == QHostAddress::Null
     || localAddress ==QHostAddress("0.0.0.0")
     || localUsername_ == "")
  {
    qWarning() << "WARNING: Necessary info not set for SDP generation:" << localAddress.toString()
               << localUsername_;
    return NULL;
  }

  if(availablePorts_.size() <= 2)
  {
    qDebug() << "Not enough free ports to create SDP:" << availablePorts_.size();
    return NULL;
  }

  // TODO: Get suitable SDP from media manager
  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;
  newInfo->originator_username = localUsername_;
  newInfo->sess_id = QDateTime::currentMSecsSinceEpoch();
  newInfo->sess_v = QDateTime::currentMSecsSinceEpoch();
  newInfo->host_nettype = "IN";
  if(localAddress.protocol() == QAbstractSocket::IPv6Protocol)
  {
    newInfo->host_addrtype = "IP6";
    newInfo->connection_addrtype = "IP6";
  }
  else
  {
    newInfo->host_addrtype = "IP4";
    newInfo->connection_addrtype = "IP4";
  }

  newInfo->host_address = localAddress.toString();

  newInfo->sessionName = " ";
  newInfo->sessionDescription = "A Kvazzup Video Conference";

  newInfo->connection_address = localAddress.toString();
  newInfo->connection_nettype = "IN";

  MediaInfo audio;
  MediaInfo video;
  audio.type = "audio";
  video.type = "video";
  audio.receivePort = nextAvailablePortPair();
  video.receivePort = nextAvailablePortPair();
  if(audio.receivePort == 0 || video.receivePort == 0)
  {
    makePortPairAvailable(audio.receivePort);
    makePortPairAvailable(video.receivePort);
    qDebug() << "WARNING: Ran out of ports to assign to SDP. SHould be checked earlier.";
    return NULL;
  }
  audio.proto = "RTP/AVP";
  video.proto = "RTP/AVP";
  audio.rtpNum = 96;
  video.rtpNum = 97;
  // we ignore nettype, addrtype and address, because we have a global c=

  RTPMap a_rtp;
  RTPMap v_rtp;
  a_rtp.rtpNum = 96;
  v_rtp.rtpNum = 97;
  a_rtp.clockFrequency = 48000;
  v_rtp.clockFrequency = 90000;
  a_rtp.codec = "opus";
  v_rtp.codec = "h265";

  audio.codecs.push_back(a_rtp);
  video.codecs.push_back(v_rtp);
  audio.activity = A_SENDRECV;
  video.activity = A_SENDRECV;

  newInfo->media.push_back(audio);
  newInfo->media.push_back(video);

  qDebug() << "SDP generated";

  return newInfo;
}
// returns NULL if suitable could not be found
std::shared_ptr<SDPMessageInfo> GlobalSDPState::localFinalSDP(SDPMessageInfo &remoteSDP,
                                                              QHostAddress localAddress)
{
  // check if suitable.
  if(!checkSDPOffer(remoteSDP))
  {
    qDebug() << "Incoming SDP did not have Opus and H265 in their offer.";
    return NULL;
  }

  // Generate response
  return generateSDP(localAddress); // TODO: The final should be modified from remote
}

bool GlobalSDPState::remoteFinalSDP(SDPMessageInfo &remoteInviteSDP)
{
  return checkSDPOffer(remoteInviteSDP);
}

bool GlobalSDPState::checkSDPOffer(SDPMessageInfo &offer)
{
  bool hasOpus = false;
  bool hasH265 = false;

  if(offer.connection_address == "0.0.0.0")
  {
    qDebug() << "Got bad global address from SDP:" << offer.connection_address;
    return false;
  }

  for(MediaInfo media : offer.media)
  {
    for(RTPMap rtp : media.codecs)
    {
      if(rtp.codec == "opus")
      {
        qDebug() << "Found Opus";
        hasOpus = true;
      }
      else if(rtp.codec == "h265")
      {
        qDebug() << "Found H265";
        hasH265 = true;
      }
    }
  }

  return hasOpus && hasH265;
}

uint16_t GlobalSDPState::nextAvailablePortPair()
{
  // TODO: I'm suspecting this may sometimes hang Kvazzup at the start

  uint16_t newLowerPort = 0;

  portLock_.lock();
  if(availablePorts_.size() >= 1 && remainingPorts_ >= 2)
  {
    QUdpSocket test_port1;
    QUdpSocket test_port2;
    do
    {
      newLowerPort = availablePorts_.at(0);
      availablePorts_.pop_front();
      remainingPorts_ -= 2;
      qDebug() << "Trying to bind ports:" << newLowerPort << "and" << newLowerPort + 1;

    } while(!test_port1.bind(newLowerPort) && !test_port2.bind(newLowerPort + 1));
    test_port1.abort();
    test_port2.abort();
  }
  else
  {
    qDebug() << "Could not reserve ports. Remaining ports:" << remainingPorts_
             << "deque size:" << availablePorts_.size();
  }
  portLock_.unlock();

  return newLowerPort;
}

void GlobalSDPState::makePortPairAvailable(uint16_t lowerPort)
{
  if(lowerPort != 0)
  {
    portLock_.lock();
    availablePorts_.push_back(lowerPort);
    remainingPorts_ += 2;
    portLock_.unlock();
  }
}
