#include "ice.h"

#include "icesessiontester.h"
#include "logger.h"

#include <QNetworkInterface>
#include <QTime>
#include <QSettings>

#include <memory>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <math.h>       /* pow */

ICE::ICE(std::shared_ptr<NetworkCandidates> candidates, uint32_t sessionID):
  networkCandidates_(candidates),
  sessionID_(sessionID),
  pairs_(),
  connectionNominated_(false),
  peerSupportsICE_(false),
  components_(0),
  localSDP_(),
  remoteSDP_()
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


void ICE::processIncomingRequest(SIPRequest& request, QVariant& content,
                                 SIPResponseStatus generatedResponse)
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

  emit incomingRequest(request, content, generatedResponse);
}


void ICE::processIncomingResponse(SIPResponse& response, QVariant& content,
                                  bool retryRequest)
{
  if (response.message->cSeq.method == SIP_INVITE && response.type == SIP_OK)
  {
    peerSupportsICE_ = isICEToSupported(response.message->supported);
  }

  if (peerSupportsICE_ && response.message->contentType == MT_APPLICATION_SDP)
  {
    takeRemoteStartNomination(content);
  }

  emit incomingResponse(response, content, retryRequest);
}


void ICE::addLocalStartNomination(QVariant& content)
{
  localSDP_ = content.value<SDPMessageInfo>();

  components_ = localSDP_.media.count()*2;

  localSDP_.candidates = generateICECandidates(
        networkCandidates_->localCandidates(components_, sessionID_),
        networkCandidates_->globalCandidates(components_, sessionID_),
        networkCandidates_->stunCandidates(components_),
        networkCandidates_->stunBindings(components_, sessionID_),
        networkCandidates_->turnCandidates(components_, sessionID_),
        components_);

  content.setValue(localSDP_); // adds the candidates to outgoing message

  if (!remoteSDP_.candidates.empty())
  {
    if (!connectionNominated_)
    {
      // Start candiate nomination. This function won't block,
      // negotiation happens in the background
      startNomination(localSDP_.candidates, remoteSDP_.candidates, true);
    }
  }

}


void ICE::takeRemoteStartNomination(QVariant& content)
{
  remoteSDP_ = content.value<SDPMessageInfo>();

  if (!localSDP_.candidates.empty())
  {
    if (!connectionNominated_)
    {
      // spawn ICE controllee threads and start the candidate
      // exchange and nomination
      //
      // This will start the ICE nomination process. After it has finished,
      // it will send a signal which indicates its state and if successful, the call may start.
      startNomination(localSDP_.candidates, remoteSDP_.candidates, false);
    }
  }
}


int ICE::candidateTypePriority(CandidateType type, quint16 local, uint8_t component) const
{
  // see RFC 8445 section 5.1.2.1
  return ((int)pow(2, 24) * type) +
         ((int)pow(2, 8) * local) +
         256 - component;
}

int ICE::pairPriority(int controllerCandidatePriority, int controlleeCandidatePriority) const
{
  // see RFC 8445 section 6.1.2.3
  return ((int)pow(2, 32) * qMin(controllerCandidatePriority, controlleeCandidatePriority)) +
         ((int)2 * qMax(controllerCandidatePriority, controlleeCandidatePriority)) +
         controllerCandidatePriority > controlleeCandidatePriority ? 1 : 0;
}


QList<std::shared_ptr<ICEInfo>> ICE::generateICECandidates(
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
  addCandidates(globalCandidates, nullptr, foundation, HOST, 65534, iceCandidates, components);

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


void ICE::addCandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > addresses,
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


void ICE::printCandidates(QList<std::shared_ptr<ICEInfo>>& candidates)
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


QList<std::shared_ptr<ICEPair>> ICE::makeCandidatePairs(
    QList<std::shared_ptr<ICEInfo>>& local,
    QList<std::shared_ptr<ICEInfo>>& remote,
    bool controller
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

        if (controller)
        {
          pair->priority = pairPriority(local[i]->priority, remote[k]->priority);
        }
        else
        {
          pair->priority = pairPriority(remote[k]->priority, local[i]->priority);
        }

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

  /* Starts a SessionTester which is responsible for handling connectivity checks and nomination.
   * When testing is finished it is connected tonominationSucceeded/nominationFailed */

  agent_ = std::shared_ptr<IceSessionTester> (new IceSessionTester(controller));
  pairs_ = makeCandidatePairs(local, remote, controller);
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


  agent_->init(&pairs_, components_);
  agent_->start();
}


void ICE::handeICESuccess(QList<std::shared_ptr<ICEPair>> &streams)
{
  // check that results make sense. They should always.
  if (streams.at(0) == nullptr ||
      streams.at(1) == nullptr ||
      streams.size() != components_)
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

    Logger::getLogger()->printDebug(DEBUG_IMPORTANT, this, "ICE succeeded", names, values);

    // end other tests. We have a winner.
    agent_->quit();
    connectionNominated_ = true;
    pairs_.clear();
    pairs_.push_back(streams.at(0));
    pairs_.push_back(streams.at(1));
    pairs_.push_back(streams.at(2));
    pairs_.push_back(streams.at(3));

    emit nominationSucceeded(pairs_, sessionID_);
  }
}


void ICE::handleICEFailure()
{
  Logger::getLogger()->printDebug(DEBUG_ERROR, "ICE",  
                                  "Failed to nominate RTP/RTCP candidates!");

  agent_->quit();
  connectionNominated_ = false; // TODO: crash here with debugger
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
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
