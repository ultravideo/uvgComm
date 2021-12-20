#include "ice.h"

#include "icesessiontester.h"
#include "common.h"
#include "logger.h"
#include "global.h"

#include <QNetworkInterface>
#include <QTime>
#include <QSettings>

#include <memory>


#include <math.h>       /* pow */

const uint32_t CONTROLLER_SESSION_TIMEOUT = 10000;
const uint32_t NONCONTROLLER_SESSION_TIMEOUT = 20000;


ICE::ICE(std::shared_ptr<NetworkCandidates> candidates, uint32_t sessionID):
  networkCandidates_(candidates),
  sessionID_(sessionID),
  peerSupportsICE_(false)
{}

ICE::~ICE()
{}


void ICE::processOutgoingRequest(SIPRequest& request, QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Processing outgoing request");

  // Add ice as supported module so the other one can anticipate need for ice
  if (request.method == SIP_INVITE || request.method == SIP_OPTIONS)
  {
    addICEToSupported(request.message->supported);
  }

  if (peerSupportsICE_ && request.message->contentType == MT_APPLICATION_SDP)
  {
    addLocalStartNomination(content);
  }

  emit outgoingRequest(request, content);
}


void ICE::processOutgoingResponse(SIPResponse& response, QVariant& content)
{
  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    addICEToSupported(response.message->supported);
  }

  if (peerSupportsICE_ && response.message->contentType == MT_APPLICATION_SDP)
  {
    addLocalStartNomination(content);
  }

  emit outgoingResponse(response, content);
}


void ICE::processIncomingRequest(SIPRequest& request, QVariant& content)
{
  Logger::getLogger()->printNormal(this, "Processing incoming request");

  if (request.method == SIP_INVITE || request.method == SIP_OPTIONS)
  {
    peerSupportsICE_ = isICEToSupported(request.message->supported);
  }

  if (peerSupportsICE_ && request.message->contentType == MT_APPLICATION_SDP)
  {
    takeRemoteStartNomination(content);
  }

  emit incomingRequest(request, content);
}


void ICE::processIncomingResponse(SIPResponse& response, QVariant& content)
{
  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    peerSupportsICE_ = isICEToSupported(response.message->supported);
  }

  if (peerSupportsICE_ && response.message->contentType == MT_APPLICATION_SDP)
  {
    takeRemoteStartNomination(content);
  }

  emit incomingResponse(response, content);
}


void ICE::addLocalStartNomination(QVariant& content)
{
  SDPMessageInfo localSDP = content.value<SDPMessageInfo>();

  localSDP.candidates = generateICECandidates(networkCandidates_->localCandidates(STREAM_COMPONENTS, sessionID_),
                                              networkCandidates_->globalCandidates(STREAM_COMPONENTS, sessionID_),
                                              networkCandidates_->stunCandidates(STREAM_COMPONENTS),
                                              networkCandidates_->stunBindings(STREAM_COMPONENTS, sessionID_),
                                              networkCandidates_->turnCandidates(STREAM_COMPONENTS, sessionID_));

  localCandidates_ = localSDP.candidates;

  content.setValue(localSDP);

  if (!remoteCandidates_.empty())
  {
    // Start candiate nomination. This function won't block,
    // negotiation happens in the background
    startNomination(localCandidates_, remoteCandidates_, true);
  }
}


void ICE::takeRemoteStartNomination(QVariant& content)
{
  remoteCandidates_ = content.value<SDPMessageInfo>().candidates;

  if (!localCandidates_.empty())
  {
    // spawn ICE controllee threads and start the candidate
    // exchange and nomination
    //
    // This will start the ICE nomination process. After it has finished,
    // it will send a signal which indicates its state and if successful, the call may start.
    startNomination(localCandidates_, remoteCandidates_, false);
  }
}


int ICE::calculatePriority(CandidateType type, quint16 local, uint8_t component)
{
  return ((int)pow(2, 24) * type) +
         ((int)pow(2, 8) * local) +
         256 - component;
}


QList<std::shared_ptr<ICEInfo>> ICE::generateICECandidates(
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > localCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > globalCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > stunCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > stunBindings,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > turnCandidates)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Start Generating ICE candidates", {
               "Local", "Global", "STUN", "STUN relays", "TURN"},
            {QString::number(localCandidates->size()),
             QString::number(globalCandidates->size()),
             QString::number(stunCandidates->size()),
             QString::number(stunBindings->size()),
             QString::number(turnCandidates->size())});


  QTime time = QTime::currentTime();
  qsrand((uint)time.msec());

  QList<std::shared_ptr<ICEInfo>> iceCandidates;

  quint32 foundation = 1;

  addCandidates(localCandidates, nullptr, foundation, HOST, 65535, iceCandidates);
  addCandidates(globalCandidates, nullptr, foundation, HOST, 65534, iceCandidates);

  if (stunCandidates->size() == stunBindings->size())
  {
    addCandidates(stunCandidates, stunBindings, foundation, SERVER_REFLEXIVE,
                  65535, iceCandidates);
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "STUN bindings don't match");
  }
  addCandidates(turnCandidates, nullptr, foundation, RELAY, 0, iceCandidates);

  return iceCandidates;
}


void ICE::addCandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > addresses,
                        std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > relayAddresses,
                        quint32& foundation, CandidateType type, quint16 localPriority,
                        QList<std::shared_ptr<ICEInfo>>& candidates)
{
  bool includeRelayAddress = relayAddresses != nullptr && addresses->size() == relayAddresses->size();

  if (!includeRelayAddress && type != HOST && !addresses->empty())
  {
    Logger::getLogger()->printProgramError(this, "Bindings not given for non host cadidate!");
    return;
  }

  // got through sets of STREAMS addresses
  for (int i = 0; i + STREAM_COMPONENTS <= addresses->size(); i += STREAM_COMPONENTS)
  {
    // make a candidate set
    // j is the index in addresses
    for (int j = i; j < i + STREAM_COMPONENTS; ++j)
    {

      QHostAddress relayAddress = QHostAddress("");
      quint16 relayPort = 0;

      if (includeRelayAddress)
      {
        relayAddress = relayAddresses->at(j).first;
        relayPort = relayAddresses->at(j).second;
      }
      uint8_t component = j - i + 1;

      candidates.push_back(makeCandidate(foundation, type, component,
                                         addresses->at(j).first,
                                         addresses->at(j).second,
                                         relayAddress, relayPort, localPriority));
    }

    ++foundation;

  }
}


std::shared_ptr<ICEInfo> ICE::makeCandidate(uint32_t foundation,
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
  candidate->priority  = calculatePriority(type, localPriority, component);

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


void ICE::printCandidates(QList<std::shared_ptr<ICEInfo>>& candidates)
{
  QStringList candidateNames;
  QStringList candidateStrings;
  for (auto& candidate : candidates)
  {
    candidateNames.push_back(candidate->address + ":");
    candidateStrings.push_back("Foundation: " + candidate->foundation +
                               " Priority: " + candidate->priority);
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Generated the following ICE candidates", 
                                  candidateNames, candidateStrings);
}


QList<std::shared_ptr<ICEPair>> ICE::makeCandidatePairs(
    QList<std::shared_ptr<ICEInfo>>& local,
    QList<std::shared_ptr<ICEInfo>>& remote
)
{
  QList<std::shared_ptr<ICEPair>> pairs;

  // match all host candidates with remote (remote does the same)
  for (int i = 0; i < local.size(); ++i)
  {
    for (int k = 0; k < remote.size(); ++k)
    {
      // component has to match
      if (local[i]->component == remote[k]->component)
      {
        std::shared_ptr<ICEPair> pair = std::make_shared<ICEPair>();

        // we copy local because we modify it later with stun bindings and
        // we don't want to modify our sent candidates
        pair->local = std::shared_ptr<ICEInfo> (new ICEInfo);
        *(pair->local)    = *local[i];

        pair->remote   = remote[k];
        pair->priority = qMin(local[i]->priority, remote[k]->priority); // TODO spec
        pair->state    = PAIR_FROZEN;

        pairs.push_back(pair);
      }
    }
  }

  Logger::getLogger()->printNormal(this, "Created " + QString::number(pairs.size()) + 
                                         " candidate pairs");
  return pairs;
}


void ICE::startNomination(QList<std::shared_ptr<ICEInfo>>& local,
                          QList<std::shared_ptr<ICEInfo>>& remote, bool controller)
{
  Logger::getLogger()->printImportant(this, "Starting ICE nomination");

  // Starts a SessionTester which is responsible for handling
  // connectivity checks and nomination.
  // When testing is finished it is connected tonominationSucceeded/nominationFailed

  // nomination-related memory is released by cleanupSession

  uint32_t timeout = 0;
  if (controller)
  {
    timeout = CONTROLLER_SESSION_TIMEOUT;
  }
  else
  {
    timeout = NONCONTROLLER_SESSION_TIMEOUT;
  }

  agent_ = std::shared_ptr<IceSessionTester> (new IceSessionTester(controller, timeout));
  pairs_ = makeCandidatePairs(local, remote);
  connectionNominated_ = false;

  QObject::connect(agent_.get(),
                   &IceSessionTester::iceSuccess,
                   this,
                   &ICE::handeICESuccess,
                   Qt::DirectConnection);
  QObject::connect(agent_.get(),
                   &IceSessionTester::iceFailure,
                   this,
                   &ICE::handleICEFailure,
                   Qt::DirectConnection);


  agent_->init(&pairs_, STREAM_COMPONENTS);
  agent_->start();
}


void ICE::handeICESuccess(QList<std::shared_ptr<ICEPair>> &streams)
{
  // check that results make sense. They should always.
  if (streams.at(0) == nullptr ||
      streams.at(1) == nullptr ||
      streams.size() != STREAM_COMPONENTS)
  {
    Logger::getLogger()->printProgramError(this,  "The ICE results don't make " 
                                                  "sense even though they should");
    handleICEFailure();
  }
  else 
  {
    QStringList names;
    QStringList values;
    for(auto& component : streams)
    {
      names.append("Component " + QString::number(component->local->component));
      values.append(component->local->address + ":" + QString::number(component->local->port)
                    + " <-> " +
                    component->remote->address + ":" + QString::number(component->remote->port));
    }

    Logger::getLogger()->printDebug(DEBUG_IMPORTANT, this, "ICE finished.", names, values);

    // end other tests. We have a winner.
    agent_->quit();
    connectionNominated_ = true;
    QList<std::shared_ptr<ICEPair>> pairs;
    pairs.push_back(streams.at(0));
    pairs.push_back(streams.at(1));
    pairs.push_back(streams.at(2));
    pairs.push_back(streams.at(3));

    emit nominationSucceeded(pairs, sessionID_);
  }
}


void ICE::handleICEFailure()
{
  Logger::getLogger()->printDebug(DEBUG_ERROR, "ICE",  
                                  "Failed to nominate RTP/RTCP candidates!");

  agent_->quit();
  connectionNominated_ = false;
  emit nominationFailed(sessionID_);
}


void ICE::uninit()
{
  if (agent_ != nullptr)
  {
    agent_->exit(0);
    uint8_t waits = 0;
    while (agent_->isRunning() && waits <= 10)
    {
      qSleep(10);
      ++waits;
    }
  }

  agent_ = nullptr;
  pairs_.clear();
  connectionNominated_ = false;

  networkCandidates_->cleanupSession(sessionID_);
}


void ICE::addICEToSupported(std::shared_ptr<QStringList>& supported)
{
  if (supported == nullptr)
  {
    supported = std::shared_ptr<QStringList> (new QStringList);
  }

  supported->append("ice");
}

bool ICE::isICEToSupported(std::shared_ptr<QStringList> supported)
{
  return supported != nullptr && supported->contains("ice");
}
