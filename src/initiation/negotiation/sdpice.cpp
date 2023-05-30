#include "sdpice.h"

#include "logger.h"

#include <QTime>

SDPICE::SDPICE(std::shared_ptr<NetworkCandidates> candidates, uint32_t sessionID, bool useICE):
  sessionID_(sessionID),
  networkCandidates_(candidates),
  peerSupportsICE_(true), // we assume that peer suppports ICE unless proven otherwise
  mediaLimit_(-1),
  useICE_(useICE)
{}

void SDPICE::uninit()
{
  networkCandidates_->cleanupSession(sessionID_);
}


void SDPICE::limitMediaCandidates(int limit)
{
  Logger::getLogger()->printNormal(this, "Limiting media", "Limit", QString::number(limit));
  mediaLimit_ = limit;
}


void SDPICE::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Processing outgoing request");

  // Add ice as supported module so the other one can anticipate need for ice
  if (request.method == SIP_INVITE || request.method == SIP_OPTIONS)
  {
    if (useICE_)
    {
      addICEToSupported(request.message->supported);
    }
  }

  if ((request.method == SIP_INVITE ||
       (peerSupportsICE_ && request.method == SIP_ACK))
      && request.message->contentType == MT_APPLICATION_SDP)
  {
    addLocalCandidatesToSDP(content);
  }

  emit outgoingRequest(request, content);
}


void SDPICE::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    if (useICE_)
    {
      addICEToSupported(response.message->supported);
    }
  }

  if (response.message->contentType == MT_APPLICATION_SDP)
  {
    addLocalCandidatesToSDP(content);
  }

  emit outgoingResponse(response, content);
}


void SDPICE::processIncomingRequest(SIPRequest& request, QVariant& content,
                                 SIPResponseStatus generatedResponse)
{
  Logger::getLogger()->printNormal(this, "Processing incoming request");

  if (request.method == SIP_INVITE || request.method == SIP_OPTIONS)
  {
    peerSupportsICE_ = isICEToSupported(request.message->supported);
  }

  if (peerSupportsICE_ && request.message->contentType == MT_APPLICATION_SDP)
  {
    Logger::getLogger()->printNormal(this, "Got remote ICE candidates");
  }

  emit incomingRequest(request, content, generatedResponse);
}


void SDPICE::processIncomingResponse(SIPResponse& response, QVariant& content,
                                  bool retryRequest)
{
  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    peerSupportsICE_ = isICEToSupported(response.message->supported);
  }

  if (peerSupportsICE_ && response.message->contentType == MT_APPLICATION_SDP)
  {
    Logger::getLogger()->printNormal(this, "Got remote ICE candidates");
  }

  emit incomingResponse(response, content, retryRequest);
}


void SDPICE::addLocalCandidatesToSDP(QVariant& content)
{
  SDPMessageInfo sdp = content.value<SDPMessageInfo>();
  for (unsigned int i = 0; i < sdp.media.size(); ++i)
  {
    if (sdp.media.at(i).candidates.empty() && sdp.media.at(i).connection_address == "")
    {
      if (mediaLimit_ > 0)
      {
        addLocalCandidatesToMedia(sdp.media[i], i%mediaLimit_);
      }
      else
      {
        addLocalCandidatesToMedia(sdp.media[i], i);
      }
    }
  }

  content.setValue(sdp); // adds the candidates to outgoing message
  std::shared_ptr<SDPMessageInfo> local = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  *local = sdp;

  // we must give our final SDP to rest of the program
  emit localSDPWithCandidates(sessionID_, local);
}


void SDPICE::addLocalCandidatesToMedia(MediaInfo& media, int mediaIndex)
{
  Logger::getLogger()->printNormal(this, "Media limit", "index", QString::number(mediaIndex));
  int neededComponents = 1;
  if (media.proto == "RTP/AVP")
  {
    neededComponents = 2; // RTP and RTCP
  }

  if (existingLocalCandidates_.size() <= mediaIndex)
  {
    existingLocalCandidates_.push_back(networkCandidates_->localCandidates(neededComponents, sessionID_));
  }
  if (existingGlobalCandidates_.size() <= mediaIndex)
  {
    existingGlobalCandidates_.push_back(networkCandidates_->globalCandidates(neededComponents, sessionID_));
  }
  if (existingStunCandidates_.size() <= mediaIndex)
  {
    existingStunCandidates_.push_back(networkCandidates_->stunCandidates(neededComponents));
  }
  if (existingStunBindings_.size() <= mediaIndex)
  {
    existingStunBindings_.push_back(networkCandidates_->stunBindings(neededComponents, sessionID_));
  }
  if (existingturnCandidates_.size() <= mediaIndex)
  {
    existingturnCandidates_.push_back(networkCandidates_->turnCandidates(neededComponents, sessionID_));
  }

  if (useICE_ && peerSupportsICE_)
  {
    // transform network addresses into ICE candidates
    media.candidates += generateICECandidates(
      existingLocalCandidates_[mediaIndex],
      existingGlobalCandidates_[mediaIndex],
      existingStunCandidates_[mediaIndex], existingStunBindings_[mediaIndex],
      existingturnCandidates_[mediaIndex], neededComponents);

    if (!media.candidates.empty()) {
      media.receivePort = media.candidates.first()->port;
    }
  }
  else
  {
    // TODO: Fix STUN bindings and change this order
    Logger::getLogger()->printNormal(this, "Settings connection addresses directly instead of ICE candidates");
    if (!existingLocalCandidates_[mediaIndex]->empty())
    {
      Logger::getLogger()->printNormal(this, "Using local IP address");
      setMediaAddress(existingLocalCandidates_, media, mediaIndex);
    }
    else if (!existingStunCandidates_[mediaIndex]->empty())
    {
      Logger::getLogger()->printNormal(this, "Using STUN address");
      setMediaAddress(existingStunCandidates_, media, mediaIndex);
    }
    else if (!existingGlobalCandidates_[mediaIndex]->empty())
    {
      Logger::getLogger()->printNormal(this, "Using Global IP address");
      setMediaAddress(existingGlobalCandidates_, media, mediaIndex);
    }
    else
    {
      Logger::getLogger()->printError(this, "No addresses were found!");
    }
  }
}

void SDPICE::setMediaAddress(const std::vector<std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>>> candidates,
                             MediaInfo& media, int mediaIndex)
{
  if (candidates.size() > mediaIndex) {
    media.connection_address = candidates[mediaIndex]->first().first.toString();
    media.receivePort = candidates[mediaIndex]->first().second;
    media.connection_nettype = "IN";

    if (candidates[mediaIndex]->first().first.protocol() == QAbstractSocket::IPv4Protocol)
    {
      media.connection_addrtype = "IP4";
    }
    else if (candidates[mediaIndex]->first().first.protocol() == QAbstractSocket::IPv6Protocol)
    {
      media.connection_addrtype = "IP6";
    }
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Not enough candidates available");
  }
}


QList<std::shared_ptr<ICEInfo>> SDPICE::generateICECandidates(
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > localCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > globalCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > stunCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > stunBindings,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > turnCandidates,
    int components)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Start Generating ICE candidates", {
               "Local", "Global", "STUN", "STUN relays", "TURN"},
            {QString::number(localCandidates->size()),
             QString::number(globalCandidates->size()),
             QString::number(stunCandidates->size()),
             QString::number(stunBindings->size()),
             QString::number(turnCandidates->size())});

  QTime time = QTime::currentTime();
  srand((uint)time.msec());

  QList<std::shared_ptr<ICEInfo>> iceCandidates;

  quint32 foundation = 1;

  addCandidates(localCandidates, nullptr, foundation, HOST, 65535, iceCandidates, components);
  addCandidates(globalCandidates, nullptr, foundation, HOST, 65535 - localCandidates->size()/components,
                iceCandidates, components);

  if (stunCandidates->size() == stunBindings->size())
  {
    addCandidates(stunCandidates, stunBindings, foundation, SERVER_REFLEXIVE,
                  65535, iceCandidates, components);
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "STUN bindings don't match");
  }
  addCandidates(turnCandidates, nullptr, foundation, RELAY, 0, iceCandidates, components);

  return iceCandidates;
}


void SDPICE::addCandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > addresses,
                        std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > relayAddresses,
                        quint32& foundation, CandidateType type, quint16 localPriority,
                        QList<std::shared_ptr<ICEInfo>>& candidates,
                        int components)
{
  bool includeRelayAddress = relayAddresses != nullptr && addresses->size() == relayAddresses->size();

  if (!includeRelayAddress && type != HOST && !addresses->empty())
  {
    Logger::getLogger()->printProgramError(this, "Bindings not given for non host cadidate!");
    return;
  }

  // map sorts candidates by priority if we add them here
  std::map<int, std::shared_ptr<ICEInfo>> sorted;

  // got through sets of STREAMS addresses
  for (int i = 0; i + components <= addresses->size(); i += components)
  {
    // make a candidate set
    // j is the index in addresses
    for (int j = i; j < i + components; ++j)
    {

      QHostAddress relayAddress = QHostAddress("");
      quint16 relayPort = 0;

      if (includeRelayAddress)
      {
        relayAddress = relayAddresses->at(j).first;
        relayPort = relayAddresses->at(j).second;
      }
      uint8_t component = j - i + 1;

      std::shared_ptr<ICEInfo> candidate = makeCandidate(foundation, type, component,
                                                         addresses->at(j).first,
                                                         addresses->at(j).second,
                                                         relayAddress, relayPort, localPriority - i/components);
      sorted[candidate->priority] = candidate;
    }

    ++foundation;
  }

  // add candidates from largest to smallest priority
  for (auto candidate = sorted.rbegin(); candidate != sorted.rend(); candidate++)
  {
    candidates.push_back(candidate->second);
  }
}


std::shared_ptr<ICEInfo> SDPICE::makeCandidate(uint32_t foundation,
                                            CandidateType type,
                                            uint8_t component,
                                            const QHostAddress address,
                                            quint16 port,
                                            const QHostAddress relayAddress,
                                            quint16 relayPort,
                                            quint16 localPriority)
{
  std::shared_ptr<ICEInfo> candidate  = std::make_shared<ICEInfo>();

  candidate->address  = address.toString();
  candidate->port  = port;
  candidate->foundation  = QString::number(foundation);
  candidate->transport  = "UDP";
  candidate->component  = component;
  candidate->priority  = candidateTypePriority(type, localPriority, component);

  QString typeString = "";
  candidate->rel_address = "";
  candidate->rel_port = 0;

  if (type != HOST && !relayAddress.isNull() && relayPort != 0)
  {
    candidate->rel_address = relayAddress.toString();
    candidate->rel_port = relayPort;
  }

  if (type == HOST)
  {
    typeString = "host";
  }
  else if (type == SERVER_REFLEXIVE)
  {
    typeString = "srflx";
  }
  else if (type == RELAY)
  {
    typeString = "relay";
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "Peer reflexive candidates not "
                                                 "possible at this point");
    return nullptr;
  }

  candidate->type = typeString;

  return candidate;
}


void SDPICE::printCandidates(QList<std::shared_ptr<ICEInfo>>& candidates)
{
  QStringList candidateNames;
  QStringList candidateStrings;
  for (auto& candidate : candidates)
  {
    candidateNames.push_back(candidate->address + ":");
    candidateStrings.push_back("Foundation: " + candidate->foundation +
                               " Priority: " + QString::number(candidate->priority));
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Generated the following ICE candidates",
                                  candidateNames, candidateStrings);
}


void SDPICE::addICEToSupported(std::shared_ptr<QStringList>& supported)
{
  if (supported == nullptr)
  {
    supported = std::shared_ptr<QStringList> (new QStringList);
  }

  supported->append("ice");
}


bool SDPICE::isICEToSupported(std::shared_ptr<QStringList> supported)
{
  return supported != nullptr && supported->contains("ice");
}


int SDPICE::candidateTypePriority(CandidateType type, quint16 local, uint8_t component) const
{
  // see RFC 8445 section 5.1.2.1
  return ((int)pow(2, 24) * type) +
         ((int)pow(2, 8) * local) +
         256 - component;
}

