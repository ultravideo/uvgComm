#include "sdpnegotiation.h"

#include "common.h"
#include "global.h"
#include "logger.h"

#include <QVariant>

SDPNegotiation::SDPNegotiation(std::shared_ptr<SDPMessageInfo> localSDP):
  localSDP_(nullptr),
  remoteSDP_(nullptr),
  negotiationState_(NEG_NO_STATE),
  peerAcceptsSDP_(false)
{
  // this makes it possible to send SDP as a signal parameter
  qRegisterMetaType<std::shared_ptr<SDPMessageInfo> >("std::shared_ptr<SDPMessageInfo>");

  localSDP_ = localSDP;
}


void SDPNegotiation::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Processing outgoing request");

  if (request.method == SIP_INVITE || request.method == SIP_OPTIONS)
  {
    addSDPAccept(request.message->accept);
  }

  // We could also add SDP to INVITE, but we choose to send offer
  // in INVITE OK response and ACK.
  if (request.method == SIP_ACK && negotiationState_ == NEG_OFFER_RECEIVED)
  {
    Logger::getLogger()->printNormal(this, "Adding SDP content to request");

    request.message->contentLength = 0;
    request.message->contentType = MT_APPLICATION_SDP;

    if (!localSDPToContent(content))
    {
      Logger::getLogger()->printError(this, "Failed to get SDP answer to request");
      return;
    }

    negotiationState_ = NEG_FINISHED;
  }

  emit outgoingRequest(request, content);
}


void SDPNegotiation::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  if (response.type == SIP_OK &&
      response.message->cSeq.method == SIP_INVITE)
  {
    addSDPAccept(response.message->accept);

    if (peerAcceptsSDP_)
    {
      if (negotiationState_ == NEG_NO_STATE || negotiationState_ == NEG_OFFER_RECEIVED)
      {
        Logger::getLogger()->printNormal(this, "Adding SDP to an OK response");
        response.message->contentLength = 0;
        response.message->contentType = MT_APPLICATION_SDP;

        if (!localSDPToContent(content))
        {
          Logger::getLogger()->printError(this, "Failed to get SDP answer to response");
          return;
        }

        if (negotiationState_ == NEG_NO_STATE)
        {
          negotiationState_ = NEG_OFFER_SENT;
        }
        else if (negotiationState_ == NEG_OFFER_RECEIVED)
        {
          negotiationState_ = NEG_FINISHED;
        }
      }
    }
  }

  emit outgoingResponse(response, content);
}


void SDPNegotiation::processIncomingRequest(SIPRequest& request, QVariant& content,
                                            SIPResponseStatus generatedResponse)
{
  Logger::getLogger()->printNormal(this, "Processing incoming request");


  if (request.method == SIP_INVITE)
  {
    peerAcceptsSDP_ = isSDPAccepted(request.message->accept);
  }

  if((request.method == SIP_INVITE || request.method == SIP_ACK) &&
     request.message->contentType == MT_APPLICATION_SDP &&
     peerAcceptsSDP_)
  {
    if (!processSDP(content))
    {
      Logger::getLogger()->printError(this, "Failed to process incoming SDP");
      // we send a DECLINE response
      generatedResponse = SIP_DECLINE;
    }
  }

  emit incomingRequest(request, content, generatedResponse);
}


void SDPNegotiation::processIncomingResponse(SIPResponse& response, QVariant& content,
                                             bool retryRequest)
{
  if(response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    peerAcceptsSDP_ = isSDPAccepted(response.message->accept);

    if(peerAcceptsSDP_ && response.message->contentType == MT_APPLICATION_SDP)
    {
      if (!processSDP(content))
      {
        Logger::getLogger()->printError(this, "Failed to process incoming SDP");
      }
    }
  }

  emit incomingResponse(response, content, retryRequest);
}


void SDPNegotiation::uninit()
{

  if (localSDP_ != nullptr)
  {
    /*for(auto& mediaStream : localSDP_->media)
    {
      // TODO: parameters_.makePortPairAvailable(mediaStream.receivePort);
    }*/
  }

  localSDP_ = nullptr;
  remoteSDP_ = nullptr;


  negotiationState_ = NEG_NO_STATE;
}


bool SDPNegotiation::localSDPToContent(QVariant& content)
{
  Q_ASSERT(localSDP_ != nullptr);
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "Adding local SDP to content");
  if(!localSDP_)
  {
    Logger::getLogger()->printWarning(this, "Failed to get local SDP!");
    return false;
  }

  SDPMessageInfo sdp = *localSDP_;
  content.setValue(sdp);
  return true;
}


bool SDPNegotiation::processSDP(QVariant& content)
{
  switch (negotiationState_)
  {
    case NEG_NO_STATE:
    case NEG_OFFER_RECEIVED:
    case NEG_FINISHED:
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                      "Got an SDP offer");
      if(!processOfferSDP(content))
      {
         Logger::getLogger()->printError(this,
                                         "Failed to process SDP offer");

         return false;
      }
      break;
    }
    case NEG_OFFER_SENT:
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                      "Got an SDP answer.");
      processAnswerSDP(content);
      break;
    }
  }

  return true;
}


bool SDPNegotiation::processOfferSDP(QVariant& content)
{
  if(!content.isValid())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "The SDP content is not valid at processing. "
                                    "Should be detected earlier.");
    return false;
  }

  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();

  std::shared_ptr<SDPMessageInfo> answerSDP = negotiateSDP(*localSDP_.get(), retrieved);

  if(answerSDP == nullptr)
  {
    Logger::getLogger()->printWarning(this, "Incoming SDP was not suitable");
    uninit();
    return false;
  }

  std::shared_ptr<SDPMessageInfo> remoteSDP
      = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *remoteSDP = retrieved;
  remoteSDP_ = remoteSDP;

  negotiationState_ = NEG_OFFER_RECEIVED;

  return true;
}


bool SDPNegotiation::processAnswerSDP(QVariant &content)
{
  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
  if (!content.isValid())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Content is not valid when processing SDP. "
                                    "Should be detected earlier.");
    return false;
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "Negotiation",
                                  "Starting to process answer SDP.");
  if (!checkSessionValidity(false) || negotiationState_ == NEG_NO_STATE)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "Negotiation",
                                    "Processing SDP answer without having sent an offer!");
    return false;
  }

  // this function blocks until a candidate is nominated or all candidates are considered
  // invalid in which case it returns false to indicate error
  if (checkSDPOffer(retrieved))
  {
    std::shared_ptr<SDPMessageInfo> remoteSDP
        = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
    *remoteSDP = retrieved;
    remoteSDP_ = remoteSDP;

    negotiationState_ = NEG_FINISHED;

    return true;
  }

  return false;
}


void SDPNegotiation::nominationSucceeded(QList<std::shared_ptr<ICEPair>>& streams,
                                      quint32 sessionID)
{
  if (!checkSessionValidity(true))
  {
    return;
  }

  if (streams.size() != STREAM_COMPONENTS)
  {
    return;
  }

  Logger::getLogger()->printNormal(this, "ICE nomination has succeeded", {"SessionID"},
                                   {QString::number(sessionID)});

  // Video. 0 is RTP, 1 is RTCP
  if (streams.at(0) != nullptr && streams.at(1) != nullptr)
  {
    setMediaPair(localSDP_->media[1],  streams.at(0)->local, true);
    setMediaPair(remoteSDP_->media[1], streams.at(0)->remote, false);
  }

  // Audio. 2 is RTP, 3 is RTCP
  if (streams.at(2) != nullptr && streams.at(3) != nullptr)
  {
    setMediaPair(localSDP_->media[0],  streams.at(2)->local, true);
    setMediaPair(remoteSDP_->media[0], streams.at(2)->remote, false);
  }

  emit iceNominationSucceeded(sessionID, localSDP_, remoteSDP_);
}


bool SDPNegotiation::checkSessionValidity(bool checkRemote) const
{
  Q_ASSERT(localSDP_ != nullptr);
  Q_ASSERT(remoteSDP_ != nullptr || !checkRemote);

  if(localSDP_ == nullptr ||
     (remoteSDP_ == nullptr && checkRemote))
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "Negotiation",
                                    "SDP not set correctly");
    return false;
  }
  return true;
}




void SDPNegotiation::addSDPAccept(std::shared_ptr<QList<SIPAccept>>& accepts)
{
  if (accepts == nullptr)
  {
    accepts = std::shared_ptr<QList<SIPAccept>> (new QList<SIPAccept>);
  }

  accepts->push_back({MT_APPLICATION_SDP, {}});
}


bool SDPNegotiation::isSDPAccepted(std::shared_ptr<QList<SIPAccept>>& accepts)
{
  // sdp is accepted on default
  if (accepts == nullptr)
  {
    return true;
  }

  for (auto& accept : *accepts)
  {
    if (accept.type == MT_APPLICATION_SDP)
    {
      return true;
    }
  }

  return false;
}


std::shared_ptr<SDPMessageInfo> SDPNegotiation::negotiateSDP(const SDPMessageInfo& localSDP,
                                                             const SDPMessageInfo& remoteSDPOffer)
{
  // At this point we should have checked if their offer is acceptable.
  // Now we just have to generate our answer.

  // TODO: This should also probably modify remoteSDPOffer according to what we select

  std::vector<int> matches;

  if (!matchMedia(matches, remoteSDPOffer, localSDP) || matches.empty())
  {
    return nullptr;
  }

  std::shared_ptr<SDPMessageInfo> newInfo =
      std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);

  *newInfo.get() = localSDP; // use the local as default for all values

  newInfo->version = 0;
  newInfo->sessionName        = remoteSDPOffer.sessionName;
  newInfo->sessionDescription = remoteSDPOffer.sessionDescription;
  newInfo->timeDescriptions   = remoteSDPOffer.timeDescriptions;

  newInfo->media.clear(); // contruct media based on remote offer

  for (int i = 0; i < remoteSDPOffer.media.size(); ++i)
  {
    MediaInfo answerMedia;

    answerMedia.type = remoteSDPOffer.media.at(i).type;
    answerMedia.receivePort = 0; // TODO: ICE Should set this to one of its candidates, 0 means it is rejected
    answerMedia.title = remoteSDPOffer.media.at(i).title;

    answerMedia.proto = localSDP.media.at(matches.at(i)).proto;

    if (matches.at(i) == -1)
    {
      // no match was found, this media is unacceptable

      // compared to INACTIVE, 0 makes it so that no RTCP is sent
      answerMedia.receivePort = 0;
      answerMedia.flagAttributes = {A_INACTIVE};
    }
    else
    {
      SDPAttributeType ourAttribute   = findStatusAttribute(localSDP.media.at(matches.at(i)).flagAttributes);
      SDPAttributeType theirAttribute = findStatusAttribute(remoteSDPOffer.media.at(i).flagAttributes);

      answerMedia.flagAttributes.clear(); // TODO: Copy non-directional attributes

      // important to know here that having no attribute means same as sendrecv (default) in SDP.
      // These ifs go through possible flags one by one, some checks are omitted thanks to previous cases
      if (ourAttribute   == A_INACTIVE ||
          theirAttribute == A_INACTIVE ||
          (ourAttribute == A_SENDONLY && theirAttribute == A_SENDONLY) || // both want to send
          (ourAttribute == A_RECVONLY && theirAttribute == A_RECVONLY))   // both want to receive
      {
        // no possible media connection found, but this stream can be activated at any time
        // RTCP should be remain active while the stream itself is inactive
        answerMedia.flagAttributes.push_back(A_INACTIVE);
      }
      else if ((ourAttribute   == A_SENDRECV || ourAttribute   == A_NO_ATTRIBUTE) &&
               (theirAttribute == A_SENDRECV || theirAttribute == A_NO_ATTRIBUTE))
      {
        // media will flow in both directions
        answerMedia.flagAttributes.push_back(ourAttribute); // sendrecv or no attribute
      }
      else if ((ourAttribute   == A_SENDONLY || ourAttribute   == A_SENDRECV ||  ourAttribute   == A_NO_ATTRIBUTE ) &&
               (theirAttribute == A_RECVONLY || theirAttribute == A_SENDRECV ||  theirAttribute == A_NO_ATTRIBUTE))
      {
        // we will send media, but not receive it
        answerMedia.flagAttributes.push_back(A_SENDONLY);
      }
      else if ((ourAttribute   == A_RECVONLY || ourAttribute   == A_SENDRECV ||  ourAttribute   == A_NO_ATTRIBUTE ) &&
               (theirAttribute == A_SENDONLY || theirAttribute == A_SENDRECV ||  theirAttribute == A_NO_ATTRIBUTE))
      {
        // we will not send media, but will receive it
        answerMedia.flagAttributes.push_back(A_RECVONLY);
      }
      else
      {
        Logger::getLogger()->printProgramError(this, "Couldn't determine attribute correctly");
        return nullptr;
      }

      selectBestCodec(remoteSDPOffer.media.at(i).rtpNums,       remoteSDPOffer.media.at(i).codecs,
                      localSDP.media.at(matches.at(i)).rtpNums, localSDP.media.at(matches.at(i)).codecs,
                      answerMedia.rtpNums,                      answerMedia.codecs);
    }

    newInfo->media.append(answerMedia);
  }

  return newInfo;
}


bool SDPNegotiation::selectBestCodec(const QList<uint8_t>& remoteNums,      const QList<RTPMap> &remoteCodecs,
                                     const QList<uint8_t>& supportedNums,   const QList<RTPMap> &supportedCodecs,
                                           QList<uint8_t>& outMatchingNums,       QList<RTPMap> &outMatchingCodecs)
{
  for (auto& remoteCodec : remoteCodecs)
  {
    for (auto& supportedCodec : supportedCodecs)
    {
      if(remoteCodec.codec == supportedCodec.codec)
      {
        outMatchingCodecs.append(remoteCodec);
        Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPNegotiationHelper",  "Found suitable codec");

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
        Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPNegotiationHelper",
                                        "Found suitable RTP number");
        return true;
      }
    }
  }

  Logger::getLogger()->printDebug(DEBUG_ERROR, "SDPNegotiationHelper",
                                  "Could not find suitable codec or RTP number for media.");

  return false;
}


bool SDPNegotiation::checkSDPOffer(SDPMessageInfo &offer)
{
  // TODO: use local SDP to verify offer

  bool hasAudio = false;
  bool hasH265 = false;

  if(offer.version != 0)
  {
    Logger::getLogger()->printPeerError("SDPNegotiationHelper",
                                        "Their offer had non-0 version",
                                        "Version",
                                        QString::number(offer.version));
    return false;
  }

  QStringList debugCodecsFound = {};
  for(MediaInfo& media : offer.media)
  {
    if(!media.rtpNums.empty() && media.rtpNums.first() == 0)
    {
      debugCodecsFound << "PCMU";
      hasAudio = true;
    }

    for(RTPMap& rtp : media.codecs)
    {
      if(rtp.codec == "opus")
      {
        debugCodecsFound << "opus";
        hasAudio = true;
      }
      else if(rtp.codec == "H265")
      {
        debugCodecsFound << "H265";
        hasH265 = true;
      }
    }
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPNegotiationHelper",
             "Found following codecs in SDP", {"Codecs"}, debugCodecsFound);

  if (offer.timeDescriptions.size() >= 1)
  {
    if (offer.timeDescriptions.at(0).startTime != 0 ||
        offer.timeDescriptions.at(0).stopTime != 0)
    {
      Logger::getLogger()->printDebug(DEBUG_ERROR, "SDPNegotiationHelper",
                 "They offered us a session with limits. Unsupported.");
      return false;
    }
  }
  else {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SDPNegotiationHelper",
               "they included wrong number of Time Descriptions. Should be detected earlier.");
    return false;
  }

  return hasAudio && hasH265;
}


void SDPNegotiation::setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo, bool local)
{
  if (mediaInfo == nullptr)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SDPNegotiationHelper",
                                    "Null mediainfo in setMediaPair");
    return;
  }

  // for local address, we bind to our rel-address if using non-host connection type
  if (local &&
      mediaInfo->type != "host" &&
      mediaInfo->rel_address != "" && mediaInfo->rel_port != 0)
  {
    media.connection_address = mediaInfo->rel_address;
    media.receivePort        = mediaInfo->rel_port;
  }
  else
  {
    media.connection_address = mediaInfo->address;
    media.receivePort        = mediaInfo->port;
  }
}


bool SDPNegotiation::matchMedia(std::vector<int> &matches,
                                const SDPMessageInfo &firstSDP,
                                const SDPMessageInfo &secondSDP)
{
  /* Currently, the matching tries to match all medias, but the
   * best approach would be to set the ports of non-matched media streams
   * to 0 which would only reject them instead of the whole call.
   * We would also have to omit our media that was not in the offer
   */
  std::vector<bool> reserved = std::vector<bool>(secondSDP.media.size(), false);
  for (auto& media : firstSDP.media)
  {
    bool foundMatch = false;

    // see which remote media best matches our media
    for (int j = 0; j < secondSDP.media.size(); ++j)
    {
      if (!reserved.at(j))
      {
        // TODO: Improve matching?
        bool isMatch = media.type == secondSDP.media.at(j).type;

        if (isMatch)
        {
          matches.push_back(j);
          reserved.at(j) = true;
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch)
    {
      matches.push_back(-1);
    }
  }

  // TODO: Support partial matches
  return matches.size() == firstSDP.media.size();
}


SDPAttributeType SDPNegotiation::findStatusAttribute(const QList<SDPAttributeType>& attributes) const
{
  for (auto& attribute : attributes)
  {
    if (attribute == A_SENDRECV ||
        attribute == A_RECVONLY ||
        attribute == A_SENDONLY ||
        attribute == A_INACTIVE)
    {
      return attribute;
    }
  }

  return A_NO_ATTRIBUTE;
}
