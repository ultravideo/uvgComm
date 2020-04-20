#include "ice.h"

#include "icesessiontester.h"
#include "common.h"
#include "global.h"

#include <QNetworkInterface>
#include <QTime>
#include <QSettings>

#include <memory>


#include <math.h>       /* pow */


ICE::ICE()
{}

ICE::~ICE()
{}

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
  printDebug(DEBUG_NORMAL, this, "Start Generating ICE candidates", {
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
    printProgramError(this, "STUN bindings don't match");
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
    printProgramError(this, "Bindings not given for non host cadidate!");
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
    printProgramError(this, "Peer reflexive candidates not possible at this point");
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
    candidateStrings.push_back("Foundation: " + candidate->foundation + " Priority: " + candidate->priority);
  }

  printDebug(DEBUG_NORMAL, this, "Generated the following ICE candidates", candidateNames, candidateStrings);
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

  printNormal(this, "Created " + QString::number(pairs.size()) + " candidate pairs");
  return pairs;
}

// callee (flow controller)
// 
// this function spawns a control thread and exist right away so the 200 OK
// response can be set as fast as possible and the remote can start respoding to our requests
//
// Thread spawned by startNomination() must keep track of which candidates failed and which succeeded
void ICE::startNomination(QList<std::shared_ptr<ICEInfo>>& local,
    QList<std::shared_ptr<ICEInfo>>& remote,
    uint32_t sessionID, bool flowController)
{
  // Spawns a FlowAgent thread which is responsible for handling
  // connectivity checks and nomination.
  // When FlowAgent has finished (succeed or failed), it sends a ready() signal
  // which is caught by handleCalleeEndOfNomination slot

  printImportant(this, "Starting ICE nomination");

  // nomination-related memory is released when handleEndOfNomination() is called
  if (flowController)
  {
    nominationInfo_[sessionID].agent = new IceSessionTester(true, 10000);
  }
  else
  {
    nominationInfo_[sessionID].agent = new IceSessionTester(false, 20000);
  }

  nominationInfo_[sessionID].pairs = makeCandidatePairs(local, remote);
  nominationInfo_[sessionID].connectionNominated = false;

  IceSessionTester *agent = nominationInfo_[sessionID].agent;

  QObject::connect(agent,
                   &IceSessionTester::iceSuccess,
                   this,
                   &ICE::handeICESuccess,
                   Qt::DirectConnection);
  QObject::connect(agent,
                   &IceSessionTester::iceFailure,
                   this,
                   &ICE::handleICEFailure,
                   Qt::DirectConnection);


  agent->init(&nominationInfo_[sessionID].pairs, sessionID, STREAM_COMPONENTS);
  agent->start();
}


void ICE::handeICESuccess(QList<std::shared_ptr<ICEPair> > &streams, uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);



  if (streams.at(0) == nullptr ||
      streams.at(1) == nullptr ||
      streams.size() != STREAM_COMPONENTS)
  {
    handleICEFailure(sessionID);
  }
  else 
  {
    QStringList names;
    QStringList values;
    for(auto& stream : streams)
    {
      names.append("Component " + QString::number(stream->local->component));
      values.append(stream->local->address + ":" + QString::number(stream->local->port) + " <-> " +
                    stream->remote->address + ":" + QString::number(stream->remote->port));
    }

    printDebug(DEBUG_IMPORTANT, this, "ICE finished.", names, values);

    nominationInfo_[sessionID].agent->quit();
    nominationInfo_[sessionID].connectionNominated = true;
    nominationInfo_[sessionID].selectedPairs = {streams.at(0), streams.at(1),
                                                streams.at(2), streams.at(3)};
    emit nominationSucceeded(sessionID);
  }
}


void ICE::handleICEFailure(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);
  nominationInfo_[sessionID].agent->quit();
  printDebug(DEBUG_ERROR, "ICE",  "Failed to nominate RTP/RTCP candidates!");
  nominationInfo_[sessionID].connectionNominated = false;
  emit nominationFailed(sessionID);
}


QList<std::shared_ptr<ICEPair>> ICE::getNominated(uint32_t sessionID)
{
  if (nominationInfo_.find(sessionID) != nominationInfo_.end())
  {
    return nominationInfo_[sessionID].selectedPairs;
  }
  printProgramError(this, "No selected ICE candidatse stored");
  return QList<std::shared_ptr<ICEPair>>();
}


void ICE::cleanupSession(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  if (nominationInfo_.contains(sessionID))
  {
    nominationInfo_.remove(sessionID);
  }
}
