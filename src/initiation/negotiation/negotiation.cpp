#include "negotiation.h"
#include <QDateTime>
#include <QObject>

#include "mediacapabilities.h"

#include "common.h"

// Port ranges used for media port allocation.
const uint16_t MIN_SIP_PORT   = 21500;
const uint16_t MAX_SIP_PORT   = 22000;

const uint16_t MAX_PORTS = 42;

Negotiation::Negotiation():
  localUsername_("")
{
  ice_ = std::make_unique<ICE>();

  QObject::connect(
    ice_.get(),
    &ICE::nominationSucceeded,
    this,
    &Negotiation::nominationSucceeded
  );

  QObject::connect(
    ice_.get(),
    &ICE::nominationFailed,
    this,
    &Negotiation::iceNominationFailed
  );
}


void Negotiation::setLocalInfo(QString username)
{
  localUsername_ = username;
}


bool Negotiation::generateOfferSDP(QString localAddress,
                                        uint32_t sessionID)
{
  Q_ASSERT(sessionID);

  qDebug() << "Getting local SDP suggestion";
  std::shared_ptr<SDPMessageInfo> localInfo = generateLocalSDP(localAddress);

  if(localInfo != nullptr)
  {
    sdps_[sessionID].localSDP = localInfo;
    sdps_[sessionID].remoteSDP = nullptr;

    negotiationStates_[sessionID] = NEG_OFFER_GENERATED;
  }
  return localInfo != nullptr;
}


bool Negotiation::generateAnswerSDP(SDPMessageInfo &remoteSDPOffer,
                                    QString localAddress,
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
    printDebug(DEBUG_PROGRAM_ERROR, "Negotiation", 
               "Failed to generate our answer to their offer."
               "Suitability should be detected earlier in checkOffer.");
    return false;
  }

  std::shared_ptr<SDPMessageInfo> remoteSDP
      = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *remoteSDP = remoteSDPOffer;

  sdps_[sessionID].localSDP = localSDP;
  sdps_[sessionID].remoteSDP = remoteSDP;

  negotiationStates_[sessionID] = NEG_ANSWER_GENERATED;
  return true;
}


bool Negotiation::processAnswerSDP(SDPMessageInfo &remoteSDPAnswer, uint32_t sessionID)
{
  printDebug(DEBUG_NORMAL, "Negotiation",  "Starting to process answer SDP.");
  if (!checkSessionValidity(sessionID, false))
  {
    return false;
  }

  if (getState(sessionID) == NEG_NO_STATE)
  {
    printDebug(DEBUG_WARNING, "Negotiation",  "Processing SDP answer without hacing sent an offer!");
    return false;
  }

  // this function blocks until a candidate is nominated or all candidates are considered
  // invalid in which case it returns false to indicate error
  if (checkSDPOffer(remoteSDPAnswer))
  {
    std::shared_ptr<SDPMessageInfo> remoteSDP
        = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
    *remoteSDP = remoteSDPAnswer;
    sdps_[sessionID].remoteSDP = remoteSDP;

    negotiationStates_[sessionID] = NEG_FINISHED;
    return true;
  }

  return false;
}


std::shared_ptr<SDPMessageInfo>  Negotiation::generateLocalSDP(QString localAddress)
{
  // TODO: This should ask media manager, what options it supports.
  qDebug() << "Generating new SDP message with our address as:" << localAddress;

  if(localAddress == ""
     || localAddress == "0.0.0.0"
     || localUsername_ == "")
  {
    qWarning() << "WARNING: Necessary info not set for SDP generation:"
               << localAddress
               << localUsername_;
    return nullptr;
  }

  // TODO: Get suitable SDP from media manager
  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;
  generateOrigin(newInfo, localAddress);
  setConnectionAddress(newInfo, localAddress);

  newInfo->sessionName = SESSION_NAME;
  newInfo->sessionDescription = SESSION_DESCRIPTION;

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
                                                          QString localAddress)
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
  for (auto& remoteMedia : remoteSDPOffer.media)
  {
    MediaInfo ourMedia;
    ourMedia.type = remoteMedia.type;
    ourMedia.receivePort = 0; // TODO: ICE Should set this to one of its candidates
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
      QList<uint8_t> supportedNums = PREDEFINED_AUDIO_CODECS;
      QList<RTPMap> supportedCodecs = DYNAMIC_AUDIO_CODECS;

      selectBestCodec(remoteMedia.rtpNums, remoteMedia.codecs,
                      supportedNums, supportedCodecs,
                      ourMedia.rtpNums, ourMedia.codecs);

    }
    else if (remoteMedia.type == "video")
    {
      QList<uint8_t> supportedNums = PREDEFINED_VIDEO_CODECS;
      QList<RTPMap> supportedCodecs = DYNAMIC_VIDEO_CODECS;

      selectBestCodec(remoteMedia.rtpNums, remoteMedia.codecs,
                      supportedNums, supportedCodecs,
                      ourMedia.rtpNums, ourMedia.codecs);
    }
    newInfo->media.append(ourMedia);
  }

  // TODO: Set also media sdp parameters.
  newInfo->candidates = ice_->generateICECandidates();
  return newInfo;
}


bool Negotiation::selectBestCodec(QList<uint8_t>& remoteNums,       QList<RTPMap> &remoteCodecs,
                                  QList<uint8_t>& supportedNums,    QList<RTPMap> &supportedCodecs,
                                  QList<uint8_t>& outMatchingNums,  QList<RTPMap> &outMatchingCodecs)
{
  for (auto& remoteCodec : remoteCodecs)
  {
    for (auto& supportedCodec : supportedCodecs)
    {
      if(remoteCodec.codec == supportedCodec.codec)
      {
        outMatchingCodecs.append(remoteCodec);
        printDebug(DEBUG_NORMAL, "Negotiation",  "Found suitable codec.");

        outMatchingNums.push_back(remoteCodec.rtpNum);

        return true;
      }
    }
  }

  for (auto& rtpNumber : remoteNums)
  {
    for (auto& supportedNum : supportedNums)
    {
      if(rtpNumber == supportedNum)
      {
        outMatchingNums.append(rtpNumber);
        printDebug(DEBUG_NORMAL, "Negotiation",  "Found suitable RTP number.");
        return true;
      }
    }
  }

  printDebug(DEBUG_ERROR, "Negotiation", 
             "Could not find suitable codec or RTP number for media.");

  return false;
}


void Negotiation::generateOrigin(std::shared_ptr<SDPMessageInfo> sdp,
                                 QString localAddress)
{
  sdp->originator_username = localUsername_;
  sdp->sess_id = QDateTime::currentMSecsSinceEpoch();
  sdp->sess_v = QDateTime::currentMSecsSinceEpoch();
  sdp->host_nettype = "IN";
  sdp->host_address = localAddress;
  if (localAddress.front() == "[")
  {
    sdp->host_address = localAddress.mid(1, localAddress.size() - 2);
    sdp->host_addrtype = "IP6";
  }
  else {
    sdp->host_addrtype = "IP4";
  }
}


void Negotiation::setConnectionAddress(std::shared_ptr<SDPMessageInfo> sdp,
                                       QString localAddress)
{
  sdp->connection_address = localAddress;
  sdp->connection_nettype = "IN";
  if (localAddress.front() == "[")
  {
    sdp->connection_address = localAddress.mid(1, localAddress.size() - 2);
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
  audio = {"audio", 0, "RTP/AVP", {},
           "", "", "", "", {},"", DYNAMIC_AUDIO_CODECS, {A_SENDRECV}, {}};

  // add all the dynamic numbers first because we want to favor dynamic type codecs.
  for(RTPMap codec : audio.codecs)
  {
    audio.rtpNums.push_back(codec.rtpNum);
  }

  audio.rtpNums += PREDEFINED_AUDIO_CODECS;
  return true;
}


bool Negotiation::generateVideoMedia(MediaInfo& video)
{
  // we ignore nettype, addrtype and address, because we use a global c=
  video = {"video", 0, "RTP/AVP", {},
           "", "", "", "", {},"", DYNAMIC_VIDEO_CODECS, {A_SENDRECV}, {}};

  for(RTPMap codec : video.codecs)
  {
    video.rtpNums.push_back(codec.rtpNum);
  }

  // just for completeness, we will probably never support any of the pre-set video types.
  video.rtpNums += PREDEFINED_VIDEO_CODECS;
  return true;
}


std::shared_ptr<SDPMessageInfo> Negotiation::getLocalSDP(uint32_t sessionID) const
{
  if(!checkSessionValidity(sessionID, false))
  {
    return nullptr;
  }
  return sdps_.at(sessionID).localSDP;
}

std::shared_ptr<SDPMessageInfo> Negotiation::getRemoteSDP(uint32_t sessionID) const
{
  if(!checkSessionValidity(sessionID, true))
  {
    return nullptr;
  }

  return sdps_.at(sessionID).remoteSDP;
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

  printDebug(DEBUG_NORMAL, "Negotiation", 
             "Found following codecs in SDP", {"Codecs"}, debugCodecsFound);

  if (offer.timeDescriptions.size() >= 1)
  {
    if (offer.timeDescriptions.at(0).startTime != 0 ||
        offer.timeDescriptions.at(0).stopTime != 0)
    {
      printDebug(DEBUG_ERROR, "Negotiation", 
                 "They offered us a session with limits. Unsupported.");
      return false;
    }
  }
  else {
    printDebug(DEBUG_PROGRAM_ERROR, "Negotiation", 
               "they included wrong number of Time Descriptions. Should be detected earlier.");
    return false;
  }


  return hasOpus && hasH265;
}


void Negotiation::endSession(uint32_t sessionID)
{
  if(sdps_.find(sessionID) != sdps_.end())
  {
    if (sdps_.at(sessionID).localSDP != nullptr)
    {
      std::shared_ptr<SDPMessageInfo> localSDP = sdps_.at(sessionID).localSDP;
      for(auto& mediaStream : localSDP->media)
      {
        // TODO: parameters_.makePortPairAvailable(mediaStream.receivePort);
      }
    }
    sdps_.erase(sessionID);
  }

  if (negotiationStates_.find(sessionID) != negotiationStates_.end())
  {
    negotiationStates_.erase(sessionID);
  }

  ice_->cleanupSession(sessionID);
}


void Negotiation::endAllSessions()
{
  QList<uint32_t> sessions;

  for (auto& i : negotiationStates_)
  {
    sessions.push_back(i.first);

  }

  for (auto& i : sessions)
  {
    endSession(i);
  }
}

void Negotiation::respondToICECandidateNominations(uint32_t sessionID)
{
  if (!checkSessionValidity(sessionID, true))
  {
    return;
  }

  std::shared_ptr<SDPMessageInfo> localSDP = sdps_.at(sessionID).localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP = sdps_.at(sessionID).remoteSDP;

  ice_->startNomination(localSDP->candidates, remoteSDP->candidates, sessionID, false);
}

void Negotiation::startICECandidateNegotiation(uint32_t sessionID)
{
  if(!checkSessionValidity(sessionID, true))
  {
    return;
  }

  std::shared_ptr<SDPMessageInfo> localSDP = sdps_.at(sessionID).localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP = sdps_.at(sessionID).remoteSDP;

  ice_->startNomination(localSDP->candidates, remoteSDP->candidates, sessionID, true);
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

void Negotiation::nominationSucceeded(quint32 sessionID)
{
  if (!checkSessionValidity(sessionID, true))
  {
    return;
  }

  printNormal(this, "ICE nomination has succeeded", {"SessionID"}, {QString::number(sessionID)});

  ICEMediaInfo nominated = ice_->getNominated(sessionID);

  std::shared_ptr<SDPMessageInfo> localSDP = sdps_.at(sessionID).localSDP;
  std::shared_ptr<SDPMessageInfo> remoteSDP = sdps_.at(sessionID).remoteSDP;

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

  emit iceNominationSucceeded(sessionID);
}

bool Negotiation::checkSessionValidity(uint32_t sessionID, bool checkRemote) const
{
  Q_ASSERT(sessionID);

  Q_ASSERT(sdps_.find(sessionID) != sdps_.end());
  Q_ASSERT(sdps_.at(sessionID).localSDP != nullptr);
  Q_ASSERT(sdps_.at(sessionID).remoteSDP != nullptr || !checkRemote);

  if(sessionID == 0 ||
     sdps_.find(sessionID) == sdps_.end() ||
     sdps_.at(sessionID).localSDP == nullptr ||
     (sdps_.at(sessionID).remoteSDP == nullptr && checkRemote))
  {
    printDebug(DEBUG_PROGRAM_ERROR, "GlobalSDPState", 
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
