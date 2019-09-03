#include "negotiation.h"
#include <QDateTime>

#include "common.h"

// Port ranges used for media port allocation.
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
  std::shared_ptr<SDPMessageInfo> localInfo = generateLocalSDP(localAddress);

  if(localInfo != nullptr)
  {
    sdps_[sessionID].first = localInfo;
    sdps_[sessionID].second = nullptr;

    negotiationStates_[sessionID] = NEG_OFFER_GENERATED;
  }
  return localInfo != nullptr;
}

// Includes the medias for all the participants for conference
bool Negotiation::initialConferenceOfferSDP(uint32_t sessionID)
{
  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo = sdps_.at(sessionID).first;

  // go through every media and include it in our conference offer so we get a
  // response port for each media in conference for them.
  for (auto callSession : sdps_)
  {
    // don't include their own address
    if (callSession.first != sessionID)
    {
      for (auto media : callSession.second.second->media)
      {
        media.connection_address = callSession.second.second->connection_address;
        media.connection_nettype = callSession.second.second->connection_nettype;
        media.connection_addrtype = callSession.second.second->connection_addrtype;

        newInfo->media.append(media);

        // we use sendonly because we don't know the receive port of other conference
        // participants.
        newInfo->media.back().flagAttributes = {A_SENDONLY};
      }
    }
  }

  recvConferenceSdps_[sessionID] = newInfo;
  negotiationStates_[sessionID] = NEG_OFFER_GENERATED;

  return true;
}


// Includes the medias for all the participants for conference
bool Negotiation::finalConferenceOfferSDP(uint32_t sessionID)
{
  if (!checkSessionValidity(sessionID, true))
  {
    return false;
  }

  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  *newInfo = *(recvConferenceSdps_.at(sessionID));

  for (int i = 0; i < newInfo->media.size(); ++i)
  {
    newInfo->media[i].flagAttributes.clear();
    newInfo->media[i].flagAttributes.append(A_SENDRECV);
  }

  // include receive port for every media

  QString remoteAddress = sdps_.at(sessionID).second->connection_address;

  for (auto sdp : recvConferenceSdps_)
  {
    for (auto media : sdp.second->media)
    {

      // TODO: SHould this be unequal?
      if (media.connection_address == remoteAddress)
      {
        newInfo->media.append(media);
        newInfo->media.back().flagAttributes = {A_SENDRECV};
      }
    }
  }

  finalConferenceSdps_[sessionID] = newInfo;
  negotiationStates_[sessionID] = NEG_OFFER_GENERATED;
  return true;
}


std::shared_ptr<SDPMessageInfo> Negotiation::getInitialConferenceOffer(uint32_t sessionID) const
{
  Q_ASSERT(sessionID);
  Q_ASSERT(recvConferenceSdps_.find(sessionID) != recvConferenceSdps_.end());

  return recvConferenceSdps_.at(sessionID);
}


std::shared_ptr<SDPMessageInfo> Negotiation::getFinalConferenceOffer(uint32_t sessionID) const
{
  Q_ASSERT(sessionID);
  Q_ASSERT(finalConferenceSdps_.find(sessionID) != finalConferenceSdps_.end());

  return finalConferenceSdps_.at(sessionID);
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

  // TODO: check that we dont already have an SDP for them in which case we should deallocate those ports.

  // generate our SDP.
  std::shared_ptr<SDPMessageInfo> localSDP = negotiateSDP(remoteSDPOffer, localAddress);

  if (localSDP == nullptr)
  {
    printDebug(DEBUG_PROGRAM_ERROR, "Negotiation", DC_NEGOTIATING,
               "Failed to generate our answer to their offer."
               "Suitability should be detected earlier in checkOffer.");
    return false;
  }

  std::shared_ptr<SDPMessageInfo> remoteSDP
      = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *remoteSDP = remoteSDPOffer;

  sdps_[sessionID].first = localSDP;
  sdps_[sessionID].second = remoteSDP;

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

  setICEPorts(sessionID);
  negotiationStates_[sessionID] = NEG_ANSWER_GENERATED;
  return true;
}


bool Negotiation::processAnswerSDP(SDPMessageInfo &remoteSDPAnswer, uint32_t sessionID)
{
  printDebug(DEBUG_NORMAL, "Negotiation", DC_NEGOTIATING, "Starting to process answer SDP.");
  if (!checkSessionValidity(sessionID, false))
  {
    return false;
  }

  if (getState(sessionID) == NEG_NO_STATE)
  {
    printDebug(DEBUG_WARNING, "Negotiation", DC_NEGOTIATING, "Processing SDP answer without hacing sent an offer!");
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
    negotiationStates_[sessionID] = NEG_FINISHED;
    return true;
  }

  return false;
}


std::shared_ptr<SDPMessageInfo>  Negotiation::generateLocalSDP(QHostAddress localAddress)
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
  generateOrigin(newInfo, localAddress);
  setConnectionAddress(newInfo, localAddress);

  newInfo->sessionName = parameters_.callSessionName();
  newInfo->sessionDescription = parameters_.sessionDescription();

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


std::shared_ptr<SDPMessageInfo> Negotiation::negotiateSDP(SDPMessageInfo& remoteSDPOffer,
                                                          QHostAddress localAddress)
{
  // At this point we should have checked if their offer is acceptable.
  // Now we just have to generate our answer.

  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);

  // we assume our answer will be different so new origin
  generateOrigin(newInfo, localAddress);
  setConnectionAddress(newInfo, localAddress);

  newInfo->version = 0;
  newInfo->sessionName = remoteSDPOffer.sessionName;
  newInfo->sessionDescription = remoteSDPOffer.sessionDescription;
  newInfo->timeDescriptions = remoteSDPOffer.timeDescriptions;

  // Now the hard part. Select best codecs and set our corresponding media ports.
  for (auto remoteMedia : remoteSDPOffer.media)
  {
    MediaInfo ourMedia;
    ourMedia.type = remoteMedia.type;
    ourMedia.receivePort = parameters_.nextAvailablePortPair();
    ourMedia.proto = remoteMedia.proto;
    ourMedia.title = remoteMedia.title;
    if (remoteMedia.flagAttributes.empty())
    {
      ourMedia.flagAttributes = {A_SENDRECV};
    }
    else if (remoteMedia.flagAttributes.back() == A_SENDONLY)
    {
      ourMedia.flagAttributes = {A_RECVONLY};
    }
    else if (remoteMedia.flagAttributes.back() == A_RECVONLY)
    {
      ourMedia.flagAttributes = {A_SENDONLY};
    }
    else {
      ourMedia.flagAttributes = remoteMedia.flagAttributes;
    }

    // set our bitrate, not implemented
    // set our encryptionKey, not implemented

    if (remoteMedia.type == "audio")
    {
      QList<uint8_t> supportedNums = parameters_.audioPayloadTypes();
      QList<RTPMap> supportedCodecs = parameters_.audioCodecs();

      selectBestCodec(remoteMedia.rtpNums, remoteMedia.codecs,
                      supportedNums, supportedCodecs,
                      ourMedia.rtpNums, ourMedia.codecs);

    }
    else if (remoteMedia.type == "video")
    {
      QList<uint8_t> supportedNums = parameters_.videoPayloadTypes();
      QList<RTPMap> supportedCodecs = parameters_.videoCodecs();

      selectBestCodec(remoteMedia.rtpNums, remoteMedia.codecs,
                      supportedNums, supportedCodecs,
                      ourMedia.rtpNums, ourMedia.codecs);
    }
    newInfo->media.append(ourMedia);
  }

  return newInfo;
}


bool Negotiation::selectBestCodec(QList<uint8_t>& remoteNums,       QList<RTPMap> &remoteCodecs,
                                  QList<uint8_t>& supportedNums,    QList<RTPMap> &supportedCodecs,
                                  QList<uint8_t>& outMatchingNums,  QList<RTPMap> &outMatchingCodecs)
{
  for (auto remoteCodec : remoteCodecs)
  {
    for (auto supportedCodec : supportedCodecs)
    {
      if(remoteCodec.codec == supportedCodec.codec)
      {
        outMatchingCodecs.append(remoteCodec);
        printDebug(DEBUG_NORMAL, "Negotiation", DC_NEGOTIATING, "Found suitable codec.");

        outMatchingNums.push_back(remoteCodec.rtpNum);

        return true;
      }
    }
  }

  for (auto rtpNumber : remoteNums)
  {
    for (auto supportedNum : supportedNums)
    {
      if(rtpNumber == supportedNum)
      {
        outMatchingNums.append(rtpNumber);
        printDebug(DEBUG_NORMAL, "Negotiation", DC_NEGOTIATING, "Found suitable RTP number.");
        return true;
      }
    }
  }

  printDebug(DEBUG_ERROR, "Negotiation", DC_NEGOTIATING,
             "Could not find suitable codec or RTP number for media.");

  return false;
}


void Negotiation::generateOrigin(std::shared_ptr<SDPMessageInfo> sdp,
                                 QHostAddress localAddress)
{
  sdp->originator_username = localUsername_;
  sdp->sess_id = QDateTime::currentMSecsSinceEpoch();
  sdp->sess_v = QDateTime::currentMSecsSinceEpoch();
  sdp->host_nettype = "IN";
  sdp->host_address = localAddress.toString();
  if(localAddress.protocol() == QAbstractSocket::IPv6Protocol)
  {
    sdp->host_addrtype = "IP6";
  }
  else {
    sdp->host_addrtype = "IP4";
  }
}


void Negotiation::setConnectionAddress(std::shared_ptr<SDPMessageInfo> sdp,
                                       QHostAddress localAddress)
{
  sdp->connection_address = localAddress.toString();
  sdp->connection_nettype = "IN";
  if(localAddress.protocol() == QAbstractSocket::IPv6Protocol)
  {
    sdp->connection_addrtype = "IP6";
  }
  else
  {
    sdp->connection_addrtype = "IP4";
  }
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


std::shared_ptr<SDPMessageInfo> Negotiation::getRemoteConferenceSDP(uint32_t sessionID) const
{
  return finalConferenceSdps_.at(sessionID);
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

  QStringList debugCodecsFound = {};
  for(MediaInfo media : offer.media)
  {
    for(RTPMap rtp : media.codecs)
    {
      if(rtp.codec == "opus")
      {
        debugCodecsFound << "opus";
        hasOpus = true;
      }
      else if(rtp.codec == "h265")
      {
        debugCodecsFound << "h265";
        hasH265 = true;
      }
    }
  }

  printDebug(DEBUG_NORMAL, "Negotiation", DC_NEGOTIATING,
             "Found following codecs in SDP", {"Codecs"}, debugCodecsFound);

  if (offer.timeDescriptions.size() >= 1)
  {
    if (offer.timeDescriptions.at(0).startTime != 0 ||
        offer.timeDescriptions.at(0).stopTime != 0)
    {
      printDebug(DEBUG_ERROR, "Negotiation", DC_NEGOTIATING,
                 "They offered us a session with limits. Unsupported.");
      return false;
    }
  }
  else {
    printDebug(DEBUG_PROGRAM_ERROR, "Negotiation", DC_NEGOTIATING,
               "they included wrong number of Time Descriptions. Should be detected earlier.");
    return false;
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


bool Negotiation::checkSessionValidity(uint32_t sessionID, bool checkRemote) const
{
  Q_ASSERT(sessionID);

  Q_ASSERT(sdps_.find(sessionID) != sdps_.end());
  Q_ASSERT(sdps_.at(sessionID).first != nullptr);
  Q_ASSERT(sdps_.at(sessionID).second != nullptr || !checkRemote);

  if(sessionID == 0 ||
     sdps_.find(sessionID) == sdps_.end() ||
     sdps_.at(sessionID).first == nullptr ||
     (sdps_.at(sessionID).second == nullptr && checkRemote))
  {
    printDebug(DEBUG_PROGRAM_ERROR, "GlobalSDPState", DC_NEGOTIATING,
               "Attempting to use GlobalSDPState without setting SessionID correctly",
              {"sessionID"},{QString::number(sessionID)});
    return false;
  }
  return true;
}


NegotiationState Negotiation::getState(uint32_t sessionID)
{
  if (negotiationStates_.find(sessionID) == negotiationStates_.end())
  {
    return NEG_NO_STATE;
  }

  return negotiationStates_[sessionID];
}
