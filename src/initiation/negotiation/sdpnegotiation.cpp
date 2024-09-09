#include "sdpnegotiation.h"

#include "sdpdefault.h"

#include "initiation/negotiation/sdpmeshconference.h"

#include "common.h"
#include "logger.h"

#include <QVariant>
#include <random>

SDPNegotiation::SDPNegotiation(uint32_t sessionID, QString localAddress,
                               std::shared_ptr<SDPMessageInfo> localSDP,
                               std::shared_ptr<SDPMeshConference> sdpConf):
  sessionID_(sessionID),
  localbaseSDP_(nullptr),
  localSDP_(nullptr),
  remoteSDP_(nullptr),
  negotiationState_(NEG_NO_STATE),
  peerAcceptsSDP_(false),
  audioSSRC_(generateSSRC()),
  videoSSRC_(generateSSRC()),
  sdpConf_(sdpConf)
{
  // this makes it possible to send SDP as a signal parameter
  qRegisterMetaType<std::shared_ptr<SDPMessageInfo> >("std::shared_ptr<SDPMessageInfo>");

  localAddress_ = localAddress;
  setBaseSDP(localSDP);
}


void SDPNegotiation::setBaseSDP(std::shared_ptr<SDPMessageInfo> localSDP)
{
  setSDPAddress(localAddress_,
                localSDP->connection_address,
                localSDP->connection_nettype,
                localSDP->connection_addrtype);

  generateOrigin(localSDP, localAddress_, getLocalUsername());

  localbaseSDP_ = localSDP;
}


void SDPNegotiation::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Processing outgoing request");

  if (request.method == SIP_INVITE || request.method == SIP_OPTIONS)
  {
    addSDPAccept(request.message->accept);
  }

  if (request.method == SIP_INVITE)
  {
    negotiationState_ = NEG_NO_STATE;
  }

  // We could also add SDP to INVITE, but we choose to send offer
  // in INVITE OK response and ACK.
  if (request.method == SIP_INVITE || (request.method == SIP_ACK && negotiationState_ == NEG_OFFER_RECEIVED))
  {
    Logger::getLogger()->printNormal(this, "Adding SDP content to request");

    request.message->contentLength = 0;
    request.message->contentType = MT_APPLICATION_SDP;

    if (!sdpToContent(content))
    {
      Logger::getLogger()->printError(this, "Failed to get SDP answer to request");
      return;
    }
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

        if (!sdpToContent(content))
        {
          Logger::getLogger()->printError(this, "Failed to get SDP answer to response");
          return;
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
    negotiationState_ = NEG_NO_STATE; // reset state so we can negotiate again
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
  localSDP_ = nullptr;
  remoteSDP_ = nullptr;

  negotiationState_ = NEG_NO_STATE;
}


bool SDPNegotiation::sdpToContent(QVariant& content)
{
  std::shared_ptr<SDPMessageInfo> ourSDP = localbaseSDP_;

  if (negotiationState_ == NEG_OFFER_RECEIVED)
  {
    ourSDP = localSDP_;
    negotiationState_ = NEG_FINISHED;
  }
  else if (negotiationState_ == NEG_NO_STATE)
  {
    negotiationState_ = NEG_OFFER_SENT;

    for (unsigned int i = 0; i < ourSDP->media.size(); ++i)
    {
      setSSRC(i, ourSDP->media[i]);
      setMID(i, ourSDP->media[i]);
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "SDP negotiation in wrong state when including SDP");
    return false;
  }


  ourSDP = sdpConf_->getMeshSDP(sessionID_, ourSDP);

  Q_ASSERT(ourSDP != nullptr);
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "Adding local SDP to content");
  if(!ourSDP)
  {
    Logger::getLogger()->printWarning(this, "Failed to get local SDP!");
    return false;
  }
  content.setValue(*ourSDP);

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
    case NEG_FAILED:
    {
      return false;
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

  sdpConf_->addRemoteSDP(sessionID_, retrieved);

  // get our final SDP, which is later sent to them
  localSDP_ = findCommonSDP(*localbaseSDP_.get(), retrieved);

  // get their final SDP based on what is acceptable to us
  remoteSDP_ = findCommonSDP(retrieved, *localSDP_.get());

  if (localSDP_ == nullptr || remoteSDP_ == nullptr)
  {
    Logger::getLogger()->printWarning(this, "Incoming SDP was not suitable");
    negotiationState_ = NEG_FAILED;
    uninit();
    return false;
  }

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

  sdpConf_->addRemoteSDP(sessionID_, retrieved);

  /* Get our final SDP based on their answer, should succeed if they did everything correctly,
   * but good to check */
  localSDP_ = findCommonSDP(*localbaseSDP_.get(), retrieved);

  // this function blocks until a candidate is nominated or all candidates are considered
  // invalid in which case it returns false to indicate error
  if (localSDP_ != nullptr)
  {
    std::shared_ptr<SDPMessageInfo> remoteSDP
        = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
    *remoteSDP = retrieved;
    remoteSDP_ = remoteSDP;

    negotiationState_ = NEG_FINISHED;

    return true;
  }

  negotiationState_ = NEG_FAILED;

  return false;
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


std::shared_ptr<SDPMessageInfo> SDPNegotiation::findCommonSDP(const SDPMessageInfo& baseSDP,
                                                              const SDPMessageInfo& comparedSDP)
{
  // At this point we should have checked if their offer is acceptable.
  // Now we just have to generate our answer.

  std::vector<int> matches;

  if (!matchMedia(matches, comparedSDP, baseSDP) || matches.empty())
  {
    return nullptr;
  }

  std::shared_ptr<SDPMessageInfo> newInfo =
      std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);

  *newInfo.get() = baseSDP; // use the base as default for all values

  // since compared is usually the more processed or the initial SDP, we use its descriptions

  newInfo->version = 0;
  newInfo->sessionName        = comparedSDP.sessionName;
  newInfo->sessionDescription = comparedSDP.sessionDescription;
  newInfo->timeDescriptions   = comparedSDP.timeDescriptions;

  newInfo->media.clear(); // contruct media based on remote offer

  for (int i = 0; i < comparedSDP.media.size(); ++i)
  {
    if (matches.at(i) != -1)
    {
      MediaInfo resultMedia;

      resultMedia.type = comparedSDP.media.at(i).type;
      resultMedia.receivePort = 0; // setting this is handled later outside this class
      resultMedia.title = comparedSDP.media.at(i).title;

      resultMedia.proto = baseSDP.media.at(matches.at(i)).proto;

      if (matches.at(i) == -1)
      {
        // no match was found, this media is unacceptable

        // compared to INACTIVE, 0 makes it so that no RTCP is sent
        resultMedia.receivePort = 0;
        resultMedia.flagAttributes = {A_INACTIVE};
      }
      else
      {
        // here we determine which side is ready to send and/or receive this media
        SDPAttributeType ourAttribute   = findStatusAttribute(baseSDP.media.at(matches.at(i)).flagAttributes);
        SDPAttributeType theirAttribute = findStatusAttribute(comparedSDP.media.at(i).flagAttributes);

        resultMedia.flagAttributes.clear(); // TODO: Copy non-directional attributes

        // important to know here that having no attribute means same as sendrecv (default) in SDP.
        // These ifs go through possible flags one by one, some checks are omitted thanks to previous cases
        if (ourAttribute   == A_INACTIVE ||
            theirAttribute == A_INACTIVE ||
            (ourAttribute == A_SENDONLY && theirAttribute == A_SENDONLY) || // both want to send
            (ourAttribute == A_RECVONLY && theirAttribute == A_RECVONLY))   // both want to receive
        {
          // no possible media connection found, but this stream can be activated at any time
          // RTCP should be remain active while the stream itself is inactive
          resultMedia.flagAttributes.push_back(A_INACTIVE);
        }
        else if ((ourAttribute   == A_SENDRECV || ourAttribute   == A_NO_ATTRIBUTE) &&
                 (theirAttribute == A_SENDRECV || theirAttribute == A_NO_ATTRIBUTE))
        {
          // media will flow in both directions
          resultMedia.flagAttributes.push_back(ourAttribute); // sendrecv or no attribute
        }
        else if ((ourAttribute   == A_SENDONLY || ourAttribute   == A_SENDRECV ||  ourAttribute   == A_NO_ATTRIBUTE ) &&
                 (theirAttribute == A_RECVONLY || theirAttribute == A_SENDRECV ||  theirAttribute == A_NO_ATTRIBUTE))
        {
          // we will send media, but not receive it
          resultMedia.flagAttributes.push_back(A_SENDONLY);
        }
        else if ((ourAttribute   == A_RECVONLY || ourAttribute   == A_SENDRECV ||  ourAttribute   == A_NO_ATTRIBUTE ) &&
                 (theirAttribute == A_SENDONLY || theirAttribute == A_SENDRECV ||  theirAttribute == A_NO_ATTRIBUTE))
        {
          // we will not send media, but will receive it
          resultMedia.flagAttributes.push_back(A_RECVONLY);
        }
        else
        {
          Logger::getLogger()->printProgramError(this, "Couldn't determine attribute correctly");
          return nullptr;
        }

        selectBestCodec(comparedSDP.media.at(i).rtpNums,         comparedSDP.media.at(i).rtpMaps,
                        baseSDP.media.at(matches.at(i)).rtpNums, baseSDP.media.at(matches.at(i)).rtpMaps,
                        resultMedia.rtpNums,                     resultMedia.rtpMaps);
      }

      newInfo->media.append(resultMedia);
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Did not find match for media, rejecting this one media");
    }
  }

  for (unsigned int i = 0; i < newInfo->media.size(); ++i)
  {
    setSSRC(i, newInfo->media[i]);
    setMID(i, newInfo->media[i]);
  }

  return newInfo;
}


bool SDPNegotiation::selectBestCodec(const QList<uint8_t>& comparedNums, const QList<RTPMap> &comparedCodecs,
                                     const QList<uint8_t>& baseNums,     const QList<RTPMap> &baseCodecs,
                                           QList<uint8_t>& resultNums,         QList<RTPMap> &resultCodecs)
{
  for (auto& remoteCodec : comparedCodecs)
  {
    for (auto& supportedCodec : baseCodecs)
    {
      if(remoteCodec.codec == supportedCodec.codec)
      {
        resultCodecs.append(remoteCodec);
        Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPNegotiationHelper",  "Found suitable codec");

        resultNums.push_back(remoteCodec.rtpNum);

        return true;
      }
    }
  }

  for (auto& rtpNumber : comparedNums)
  {
    for (auto& supportedNum : baseNums)
    {
      if(rtpNumber == supportedNum)
      {
        resultNums.append(rtpNumber);
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


bool SDPNegotiation::matchMedia(std::vector<int> &matches,
                                const SDPMessageInfo &firstSDP,
                                const SDPMessageInfo &secondSDP)
{
  /* Currently, the matching tries to match all medias, but the
   * best approach would be to set the ports of non-matched media streams
   * to 0 which would only reject them instead of the whole call.
   * We would also have to omit our media that was not in the offer
   */
  for (auto& media : firstSDP.media)
  {
    bool foundMatch = false;

    // see which remote media best matches our media
    for (int j = 0; j < secondSDP.media.size(); ++j)
    {
      // TODO: Improve matching?
      bool isMatch = media.type == secondSDP.media.at(j).type;

      if (isMatch)
      {
        matches.push_back(j);

        foundMatch = true;
        break;
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

void SDPNegotiation::setSSRC(unsigned int mediaIndex, MediaInfo& media)
{
  for (unsigned int j = 0; j < media.valueAttributes.size(); ++j)
  {
    if (media.valueAttributes.at(j).type == A_SSRC)
    {
      return;
    }
  }

  if (media.type == "audio")
  {
    media.valueAttributes.push_back({A_SSRC, QString::number(audioSSRC_)});
  }
  else if (media.type == "video")
  {
    media.valueAttributes.push_back({A_SSRC, QString::number(videoSSRC_)});
  }
}


void SDPNegotiation::setMID(unsigned int mediaIndex, MediaInfo& media)
{
  for (unsigned int j = 0; j < media.valueAttributes.size(); ++j)
  {
    if (media.valueAttributes.at(j).type == A_MID)
    {
      return;
    }
  }
  media.valueAttributes.push_back({A_MID, QString::number(mediaIndex + 1)});
}


uint32_t SDPNegotiation::generateSSRC()
{
  // TODO: Detect collisions for SSRC
  unsigned int now = static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  Logger::getLogger()->printNormal(this, QString::number(now));
  std::mt19937 rng{std::random_device{}() + now};
  std::uniform_int_distribution<uint32_t> gen32_dist{1, UINT32_MAX};

  return gen32_dist(rng);
}
