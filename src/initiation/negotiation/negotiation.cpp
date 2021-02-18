#include "negotiation.h"

#include <QObject>

#include "common.h"
#include "global.h"



Negotiation::Negotiation(QString localAddress):
  localSDP_(nullptr),
  remoteSDP_(nullptr),
  negotiationState_(NEG_NO_STATE),
  negotiator_(),
  localAddress_(localAddress),
  peerAcceptsSDP_(false)
{
  // this makes it possible to send SDP as a signal parameter
  qRegisterMetaType<std::shared_ptr<SDPMessageInfo> >("std::shared_ptr<SDPMessageInfo>");
}


void Negotiation::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  printNormal(this, "Processing outgoing request");

  if (request.method == SIP_INVITE || request.method == SIP_OPTIONS)
  {
    addSDPAccept(request.message->accept);
  }

  // We could also add SDP to INVITE, but we choose to send offer
  // in INVITE OK response and ACK.
  if (request.method == SIP_ACK && negotiationState_ == NEG_ANSWER_GENERATED)
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

  emit outgoingRequest(request, content);
}


void Negotiation::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  if (response.type == SIP_OK &&
      response.message->cSeq.method == SIP_INVITE)
  {
    addSDPAccept(response.message->accept);

    if (peerAcceptsSDP_)
    {
      if (negotiationState_ == NEG_NO_STATE)
      {
        printNormal(this, "Adding SDP to an OK response");
        response.message->contentLength = 0;
        response.message->contentType = MT_APPLICATION_SDP;

        if (!SDPOfferToContent(content, localAddress_))
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
    }
  }

  emit outgoingResponse(response, content);
}


void Negotiation::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  printNormal(this, "Processing incoming request");


  if (request.method == SIP_INVITE)
  {
    peerAcceptsSDP_ = isSDPAccepted(request.message->accept);
  }

  if((request.method == SIP_INVITE || request.method == SIP_ACK) &&
     request.message->contentType == MT_APPLICATION_SDP &&
     peerAcceptsSDP_)
  {

    switch (negotiationState_)
    {
    case NEG_NO_STATE:
    {
      printDebug(DEBUG_NORMAL, this,
                 "Got first SDP offer.");
      if(!processOfferSDP(content, localAddress_))
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
      processOfferSDP(content, localAddress_);
      break;
    }
    case NEG_FINISHED:
    {
      printDebug(DEBUG_NORMAL, this,
                 "Got a new SDP offer in response.");
      processOfferSDP(content, localAddress_);
      break;
    }
    }
  }

  emit incomingRequest(request, content);
}


void Negotiation::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  if(response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    peerAcceptsSDP_ = isSDPAccepted(response.message->accept);

    if(peerAcceptsSDP_ && response.message->contentType == MT_APPLICATION_SDP)
    {
      switch (negotiationState_)
      {
      case NEG_NO_STATE:
      {
        printDebug(DEBUG_NORMAL, this,
                   "Got first SDP offer.");
        if(!processOfferSDP(content, localAddress_))
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
        processOfferSDP(content, localAddress_);
        break;
      }
      case NEG_FINISHED:
      {
        printDebug(DEBUG_NORMAL, this,
                   "Got a new SDP offer in response.");
        processOfferSDP(content, localAddress_);
        break;
      }
      }
    }
  }

  emit incomingResponse(response, content);
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

  // TODO: check that we dont already have an SDP for them in which case
  // we should deallocate those ports.

  // generate our SDP.
  std::shared_ptr<SDPMessageInfo> localSDP =
      negotiator_.negotiateSDP(remoteSDPOffer, localAddress);

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
}


void Negotiation::nominationSucceeded(QList<std::shared_ptr<ICEPair>>& streams,
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

  printNormal(this, "ICE nomination has succeeded", {"SessionID"},
              {QString::number(sessionID)});

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

  emit iceNominationSucceeded(sessionID, localSDP_, remoteSDP_);
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
   pointer = localSDP_;

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
  std::shared_ptr<SDPMessageInfo> pointer = localSDP_;
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


void Negotiation::addSDPAccept(std::shared_ptr<QList<SIPAccept>>& accepts)
{
  if (accepts == nullptr)
  {
    accepts = std::shared_ptr<QList<SIPAccept>> (new QList<SIPAccept>);
  }

  accepts->push_back({MT_APPLICATION_SDP, {}});
}


bool Negotiation::isSDPAccepted(std::shared_ptr<QList<SIPAccept>>& accepts)
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
