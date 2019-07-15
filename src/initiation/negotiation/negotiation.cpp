#include "negotiation.h"
#include <QDateTime>

#include "common.h"

const uint16_t MIN_SIP_PORT   = 21500;
const uint16_t MAX_SIP_PORT   = 22000;

const uint16_t MAX_PORTS = 42;

Negotiation::Negotiation():
  localUsername_("")
{
  ice_ = std::make_unique<ICE>();
  parameters_.setPortRange(MIN_SIP_PORT, MAX_SIP_PORT, MAX_PORTS);
}

void Negotiation::setLocalInfo(QString username)
{
  localUsername_ = username;
}

bool Negotiation::generateOfferSDP(QHostAddress localAddress,
                                        uint32_t sessionID)
{
  Q_ASSERT(sessionID);

  qDebug() << "Getting local SDP suggestion";
  std::shared_ptr<SDPMessageInfo> localInfo = generateSDP(localAddress);

  if(localInfo != nullptr)
  {
    sdps_[sessionID].first = localInfo;
    sdps_[sessionID].second = nullptr;
  }
  return localInfo != nullptr;
}

std::shared_ptr<SDPMessageInfo>  Negotiation::generateSDP(QHostAddress localAddress)
{
  // TODO: This should ask media manager, what options it supports.
  qDebug() << "Generating new SDP message with our address as:" << localAddress;

  if(localAddress == QHostAddress::Null
     || localAddress == QHostAddress("0.0.0.0")
     || localUsername_ == "")
  {
    qWarning() << "WARNING: Necessary info not set for SDP generation:"
               << localAddress.toString()
               << localUsername_;
    return nullptr;
  }

  if(!parameters_.enoughFreePorts())
  {
    qDebug() << "Not enough free ports to create SDP";
    return nullptr;
  }

  // TODO: Get suitable SDP from media manager
  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;
  newInfo->originator_username = localUsername_;
  newInfo->sess_id = QDateTime::currentMSecsSinceEpoch();
  newInfo->sess_v = QDateTime::currentMSecsSinceEpoch();
  newInfo->host_nettype = "IN";
  newInfo->host_address = localAddress.toString();
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

  newInfo->sessionName = parameters_.sessionName();
  newInfo->sessionDescription = parameters_.sessionDescription();

  newInfo->connection_address = localAddress.toString();
  newInfo->connection_nettype = "IN";

  newInfo->timeDescriptions.push_back(TimeInfo{0,0, "", "", {}});

  MediaInfo audio;
  MediaInfo video;

  if(!generateAudioMedia(audio) || !generateVideoMedia(video))
  {
    return nullptr;
  }

  newInfo->media = {audio, video};
  newInfo->candidates = ice_->generateICECandidates();

  return newInfo;
}


bool Negotiation::generateAudioMedia(MediaInfo &audio)
{
  // we ignore nettype, addrtype and address, because we use a global c=
  audio = {"audio", parameters_.nextAvailablePortPair(), "RTP/AVP", {},
           "", "", "", "", {},"", parameters_.audioCodecs(),{A_SENDRECV},{}};

  if(audio.receivePort == 0)
  {
    parameters_.makePortPairAvailable(audio.receivePort);
    qWarning() << "WARNING: Ran out of ports to assign to audio media in SDP. "
                  "Should be checked earlier.";
    return false;
  }

  // add all the dynamic numbers first because we want to favor dynamic type codecs.
  for(RTPMap codec : audio.codecs)
  {
    audio.rtpNums.push_back(codec.rtpNum);
  }

  audio.rtpNums += parameters_.audioPayloadTypes();
  return true;
}


bool Negotiation::generateVideoMedia(MediaInfo& video)
{
  // we ignore nettype, addrtype and address, because we use a global c=
  video = {"video", parameters_.nextAvailablePortPair(), "RTP/AVP", {},
           "", "", "", "", {},"", parameters_.videoCodecs(),{A_SENDRECV},{}};

  if(video.receivePort == 0)
  {
    parameters_.makePortPairAvailable(video.receivePort);
    qWarning() << "WARNING: Ran out of ports to assign to video media in SDP. "
                  "Should be checked earlier.";
    return false;
  }

  for(RTPMap codec : video.codecs)
  {
    video.rtpNums.push_back(codec.rtpNum);
  }

  // just for completeness, we will probably never support any of the pre-set video types.
  video.rtpNums += parameters_.videoPayloadTypes();
  return true;
}


bool Negotiation::generateAnswerSDP(SDPMessageInfo &remoteSDPOffer,
                                        QHostAddress localAddress,
                                        uint32_t sessionID)
{
  Q_ASSERT(sessionID);

  // check if suitable.
  if(!checkSDPOffer(remoteSDPOffer))
  {
    qDebug() << "Incoming SDP did not have Opus and H265 in their offer.";
    return false;
  }

  // generate our SDP.
  // TODO: generate our SDP based on their offer.
  std::shared_ptr<SDPMessageInfo> localSDP = generateSDP(localAddress);

  std::shared_ptr<SDPMessageInfo> remoteSDP
      = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *remoteSDP = remoteSDPOffer;

  // TODO: modify the their SDP to match our accepted configuration
  sdps_[sessionID].first = localSDP;
  sdps_[sessionID].second = remoteSDP;

  localSDP->sessionName = remoteSDP->sessionName;
  localSDP->sess_v = remoteSDP->sess_v + 1;

  // spawn ICE controller/controllee threads and start the candidate
  // exchange and nomination
  ice_->respondToNominations(localSDP->candidates, remoteSDP->candidates, sessionID);

  // wait until the nomination has finished (or failed)
  //
  // The call won't start until ICE has finished its job
  if (!ice_->callerConnectionNominated(sessionID))
  {
    qDebug() << "ERROR: Failed to nominate ICE candidates!";
    return false;
  }

  ICEMediaInfo nominated = ice_->getNominated(sessionID);

  // first is RTP, second is RTCP
  if (nominated.audio.first != nullptr && nominated.audio.second != nullptr)
  {
    setMediaPair(localSDP->media[0],      nominated.audio.first->local);
    setMediaPair(remoteSDP->media[0], nominated.audio.first->remote);
  }

  if (nominated.video.first != nullptr && nominated.video.second != nullptr)
  {
    setMediaPair(localSDP->media[1],      nominated.video.first->local);
    setMediaPair(remoteSDP->media[1], nominated.video.first->remote);
  }

  return true;
}

bool Negotiation::processAnswerSDP(SDPMessageInfo &remoteSDPAnswer, uint32_t sessionID)
{
  if (!checkSessionValidity(sessionID, false))
  {
    return false;
  }

  // this function blocks until a candidate is nominated or all candidates are considered
  // invalid in which case it returns false to indicate error
  if (!ice_->calleeConnectionNominated(sessionID))
  {
    qDebug() << "ERROR: Failed to nominate ICE candidates!";
    return false;
  }

  if (checkSDPOffer(remoteSDPAnswer))
  {
    std::shared_ptr<SDPMessageInfo> remoteSDP
        = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
    *remoteSDP = remoteSDPAnswer;
    sdps_[sessionID].second = remoteSDP;

    setICEPorts(sessionID);
    return true;
  }

  return false;
}


std::shared_ptr<SDPMessageInfo> Negotiation::getLocalSDP(uint32_t sessionID) const
{
  if(!checkSessionValidity(sessionID, false))
  {
    return nullptr;
  }
  return sdps_.at(sessionID).first;
}

std::shared_ptr<SDPMessageInfo> Negotiation::getRemoteSDP(uint32_t sessionID) const
{
  if(!checkSessionValidity(sessionID, true))
  {
    return nullptr;
  }

  return sdps_.at(sessionID).second;
}


bool Negotiation::checkSDPOffer(SDPMessageInfo &offer)
{
  // TODO: check everything.

  bool hasOpus = false;
  bool hasH265 = false;

  if(offer.connection_address == "0.0.0.0")
  {
    qDebug() << "Got bad global address from SDP:" << offer.connection_address;
    return false;
  }

  if(offer.version != 0)
  {
    qDebug() << "Their offer had not 0 version:" << offer.version;
    return false;
  }

  for(MediaInfo media : offer.media)
  {
    for(RTPMap rtp : media.codecs)
    {
      if(rtp.codec == "opus")
      {
        qDebug() << "Found Opus in SDP offer.";
        hasOpus = true;
      }
      else if(rtp.codec == "h265")
      {
        qDebug() << "Found H265 in SDP offer.";
        hasH265 = true;
      }
    }
  }

  return hasOpus && hasH265;
}

// frees the ports when they are not needed in rest of the program
void Negotiation::endSession(uint32_t sessionID)
{
  if(sdps_.find(sessionID) != sdps_.end())
  {
    if (sdps_.at(sessionID).first != nullptr)
    {
      std::shared_ptr<SDPMessageInfo> localSDP = sdps_.at(sessionID).first;
      for(auto mediaStream : localSDP->media)
      {
        parameters_.makePortPairAvailable(mediaStream.receivePort);
      }
    }
    sdps_.erase(sessionID);
  }

  ice_->cleanupSession(sessionID);
}


void Negotiation::startICECandidateNegotiation(uint32_t sessionID)
{
  if(!checkSessionValidity(sessionID, true))
  {
    return;
  }
  std::shared_ptr<SDPMessageInfo> localSDP = sdps_.at(sessionID).first;
  std::shared_ptr<SDPMessageInfo> remoteSDP = sdps_.at(sessionID).second;

  ice_->startNomination(localSDP->candidates, remoteSDP->candidates, sessionID);
}


void Negotiation::setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo)
{
  if (mediaInfo == nullptr)
  {
    return;
  }

  media.connection_address = mediaInfo->address;
  media.receivePort        = mediaInfo->port;
}


void Negotiation::setICEPorts(uint32_t sessionID)
{
  if(!checkSessionValidity(sessionID, true))
  {
    return;
  }

  ICEMediaInfo nominated = ice_->getNominated(sessionID);

  std::shared_ptr<SDPMessageInfo> localSDP = sdps_.at(sessionID).first;
  std::shared_ptr<SDPMessageInfo> remoteSDP = sdps_.at(sessionID).second;

  // first is RTP, second is RTCP
  if (nominated.audio.first != nullptr && nominated.audio.second != nullptr)
  {
    setMediaPair(localSDP->media[0],  nominated.audio.first->local);
    setMediaPair(remoteSDP->media[0], nominated.audio.first->remote);
  }

  if (nominated.video.first != nullptr && nominated.video.second != nullptr)
  {
    setMediaPair(localSDP->media[1],  nominated.video.first->local);
    setMediaPair(remoteSDP->media[1], nominated.video.first->remote);
  }
}


bool Negotiation::checkSessionValidity(uint32_t sessionID, bool remotePresent) const
{
  Q_ASSERT(sessionID);

  Q_ASSERT(sdps_.find(sessionID) != sdps_.end());
  Q_ASSERT(sdps_.at(sessionID).first != nullptr);
  Q_ASSERT(sdps_.at(sessionID).second != nullptr || !remotePresent);

  if(sessionID == 0 ||
     sdps_.find(sessionID) == sdps_.end() ||
     sdps_.at(sessionID).first == nullptr ||
     (sdps_.at(sessionID).second == nullptr && remotePresent))
  {
    printDebug(DEBUG_PROGRAM_ERROR, "GlobalSDPState", DC_NEGOTIATING,
               "Attempting to use GlobalSDPState without setting SessionID correctly",
              {"sessionID"},{QString::number(sessionID)});
    return false;
  }
  return true;
}
