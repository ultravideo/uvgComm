#include "negotiation.h"

#include <QObject>

#include "common.h"
#include "global.h"



Negotiation::Negotiation(std::shared_ptr<NetworkCandidates> candidates,
                         uint32_t sessionID):
  ice_(std::make_unique<ICE>(candidates, sessionID)),
  localSDP_(nullptr),
  remoteSDP_(nullptr),
  negotiationState_(NEG_NO_STATE),
  negotiator_()
{
  QObject::connect(ice_.get(), &ICE::nominationSucceeded,
                   this,       &Negotiation::nominationSucceeded);

  QObject::connect(ice_.get(), &ICE::nominationFailed,
                   this,       &Negotiation::iceNominationFailed);
}


void Negotiation::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  if(request.method == SIP_ACK && negotiationState_ == NEG_ANSWER_GENERATED)
  {
    request.message->contentLength = 0;
    printNormal(this, "Adding SDP content to request");

    request.message->contentType = MT_APPLICATION_SDP;

    if (!SDPAnswerToContent(content))
    {
      printError(this, "Failed to get SDP answer to request");
      return;
    }
  }

  ice_->processOutgoingRequest(request, content);
}


void Negotiation::processOutgoingResponse(SIPResponse& response, QVariant& content,
                                          QString localAddress)
{
  if (response.type == SIP_OK
      && response.message->cSeq.method == SIP_INVITE
      && negotiationState_ == NEG_NO_STATE)
  {
    printNormal(this, "Adding SDP to an OK response");
    response.message->contentLength = 0;
    response.message->contentType = MT_APPLICATION_SDP;
    if (!SDPOfferToContent(content, localAddress))
    {
      return;
    }
  }
  // if they sent an offer in their INVITE
  else if (negotiationState_ == NEG_ANSWER_GENERATED)
  {
    printNormal(this, "Adding SDP to response since INVITE had an SDP.");

    response.message->contentLength = 0;
    response.message->contentType = MT_APPLICATION_SDP;
    if (!SDPAnswerToContent(content))
    {
      printError(this, "Failed to get SDP answer to response");
      return;
    }
  }

  ice_->processOutgoingResponse(response, content);
}

void Negotiation::processIncomingRequest(SIPRequest& request, QVariant& content,
                                         QString localAddress)
{
  ice_->processIncomingRequest(request, content);


  if((request.method == SIP_INVITE || request.method == SIP_ACK)
     && request.message->contentType == MT_APPLICATION_SDP)
  {
    switch (negotiationState_)
    {
    case NEG_NO_STATE:
    {
      printDebug(DEBUG_NORMAL, this,
                 "Got first SDP offer.");
      if(!processOfferSDP(content, localAddress))
      {
         printDebug(DEBUG_PROGRAM_ERROR, this,
                    "Failure to process SDP offer not implemented.");

         // TODO: sendResponse SIP_DECLINE
         return;
      }
      break;
    }
    case NEG_OFFER_GENERATED:
    {
      printDebug(DEBUG_NORMAL, this,
                 "Got an SDP answer.");
      processAnswerSDP(content);
      break;
    }
    case NEG_ANSWER_GENERATED: // TODO: Not sure if these make any sense
    {
      printDebug(DEBUG_NORMAL, this,
                 "They sent us another SDP offer.");
      processOfferSDP(content, localAddress);
      break;
    }
    case NEG_FINISHED:
    {
      printDebug(DEBUG_NORMAL, this,
                 "Got a new SDP offer in response.");
      processOfferSDP(content, localAddress);
      break;
    }
    }
  }
}


void Negotiation::processIncomingResponse(SIPResponse& response, QVariant& content,
                                          QString localAddress)
{
  ice_->processIncomingResponse(response, content);

  if(response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    if(response.message->contentType == MT_APPLICATION_SDP)
    {
      switch (negotiationState_)
      {
      case NEG_NO_STATE:
      {
        printDebug(DEBUG_NORMAL, this,
                   "Got first SDP offer.");
        if(!processOfferSDP(content, localAddress))
        {
           printDebug(DEBUG_PROGRAM_ERROR, this,
                      "Failure to process SDP offer not implemented.");

           //TODO: sendResponse SIP_DECLINE
           return;
        }
        break;
      }
      case NEG_OFFER_GENERATED:
      {
        printDebug(DEBUG_NORMAL, this,
                   "Got an SDP answer.");
        processAnswerSDP(content);
        break;
      }
      case NEG_ANSWER_GENERATED: // TODO: Not sure if these make any sense
      {
        printDebug(DEBUG_NORMAL, this,
                   "They sent us another SDP offer.");
        processOfferSDP(content, localAddress);
        break;
      }
      case NEG_FINISHED:
      {
        printDebug(DEBUG_NORMAL, this,
                   "Got a new SDP offer in response.");
        processOfferSDP(content, localAddress);
        break;
      }
      }
    }
  }
}


bool Negotiation::generateOfferSDP(QString localAddress)
{
  qDebug() << "Getting local SDP suggestion";
  std::shared_ptr<SDPMessageInfo> localSDP = negotiator_.generateLocalSDP(localAddress);
  // TODO: Set also media sdp parameters.

  if(localSDP != nullptr)
  {
    localSDP_ = localSDP;
    remoteSDP_ = nullptr;

    negotiationState_ = NEG_OFFER_GENERATED;
  }
  return localSDP != nullptr;
}


bool Negotiation::generateAnswerSDP(SDPMessageInfo &remoteSDPOffer,
                                    QString localAddress)
{
  // check if suitable.
  if(!negotiator_.checkSDPOffer(remoteSDPOffer))
  {
    qDebug() << "Incoming SDP did not have Opus and H265 in their offer.";
    return false;
  }

  // TODO: check that we dont already have an SDP for them in which case we should deallocate those ports.

  // generate our SDP.
  std::shared_ptr<SDPMessageInfo> localSDP = negotiator_.negotiateSDP(remoteSDPOffer, localAddress);

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

  localSDP_ = localSDP;
  remoteSDP_ = remoteSDP;

  negotiationState_ = NEG_ANSWER_GENERATED;

  return true;
}


bool Negotiation::processAnswerSDP(SDPMessageInfo &remoteSDPAnswer)
{
  printDebug(DEBUG_NORMAL, "Negotiation",  "Starting to process answer SDP.");
  if (!checkSessionValidity(false))
  {
    return false;
  }

  if (negotiationState_ == NEG_NO_STATE)
  {
    printDebug(DEBUG_WARNING, "Negotiation",  "Processing SDP answer without hacing sent an offer!");
    return false;
  }

  // this function blocks until a candidate is nominated or all candidates are considered
  // invalid in which case it returns false to indicate error
  if (negotiator_.checkSDPOffer(remoteSDPAnswer))
  {
    std::shared_ptr<SDPMessageInfo> remoteSDP
        = std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
    *remoteSDP = remoteSDPAnswer;
    remoteSDP_ = remoteSDP;

    negotiationState_ = NEG_FINISHED;

    return true;
  }

  return false;
}


std::shared_ptr<SDPMessageInfo> Negotiation::getLocalSDP() const
{
  if(!checkSessionValidity(false))
  {
    return nullptr;
  }
  return localSDP_;
}


std::shared_ptr<SDPMessageInfo> Negotiation::getRemoteSDP() const
{
  if(!checkSessionValidity(true))
  {
    return nullptr;
  }

  return remoteSDP_;
}


void Negotiation::uninit()
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

  ice_->uninit();
}


void Negotiation::nominationSucceeded(quint32 sessionID)
{
  if (!checkSessionValidity(true))
  {
    return;
  }

  QList<std::shared_ptr<ICEPair>> streams = ice_->getNominated();

  if (streams.size() != 4)
  {
    return;
  }

  printNormal(this, "ICE nomination has succeeded", {"SessionID"}, {QString::number(sessionID)});

  // Video. 0 is RTP, 1 is RTCP
  if (streams.at(0) != nullptr && streams.at(1) != nullptr)
  {
    negotiator_.setMediaPair(localSDP_->media[1],  streams.at(0)->local, true);
    negotiator_.setMediaPair(remoteSDP_->media[1], streams.at(0)->remote, false);
  }

  // Audio. 2 is RTP, 3 is RTCP
  if (streams.at(2) != nullptr && streams.at(3) != nullptr)
  {
    negotiator_.setMediaPair(localSDP_->media[0],  streams.at(2)->local, true);
    negotiator_.setMediaPair(remoteSDP_->media[0], streams.at(2)->remote, false);
  }

  emit iceNominationSucceeded(sessionID);
}


bool Negotiation::checkSessionValidity(bool checkRemote) const
{
  Q_ASSERT(localSDP_ != nullptr);
  Q_ASSERT(remoteSDP_ != nullptr || !checkRemote);

  if(localSDP_ == nullptr ||
     (remoteSDP_ == nullptr && checkRemote))
  {
    printDebug(DEBUG_PROGRAM_ERROR, "Negotiation",
               "SDP not set correctly");
    return false;
  }
  return true;
}

bool Negotiation::SDPOfferToContent(QVariant& content, QString localAddress)
{
  std::shared_ptr<SDPMessageInfo> pointer;

  printDebug(DEBUG_NORMAL, this,  "Adding one-to-one SDP.");
  if(!generateOfferSDP(localAddress))
  {
    printWarning(this, "Failed to generate local SDP when sending offer.");
    return false;
  }
   pointer = getLocalSDP();

  Q_ASSERT(pointer != nullptr);

  SDPMessageInfo sdp = *pointer;
  content.setValue(sdp);
  return true;
}


bool Negotiation::processOfferSDP(QVariant& content,
                                  QString localAddress)
{
  if(!content.isValid())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this,
                     "The SDP content is not valid at processing. "
                     "Should be detected earlier.");
    return false;
  }

  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
  if(!generateAnswerSDP(retrieved, localAddress))
  {
    printWarning(this, "Remote SDP not suitable or we have no ports to assign");
    uninit();
    return false;
  }

  return true;
}


bool Negotiation::SDPAnswerToContent(QVariant &content)
{
  SDPMessageInfo sdp;
  std::shared_ptr<SDPMessageInfo> pointer = getLocalSDP();
  if (pointer == nullptr)
  {
    return false;
  }
  sdp = *pointer;
  content.setValue(sdp);
  return true;
}


bool Negotiation::processAnswerSDP(QVariant &content)
{
  SDPMessageInfo retrieved = content.value<SDPMessageInfo>();
  if (!content.isValid())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this,
               "Content is not valid when processing SDP. "
               "Should be detected earlier.");
    return false;
  }

  if(!processAnswerSDP(retrieved))
  {
    return false;
  }

  return true;
}
