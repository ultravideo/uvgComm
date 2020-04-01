#include <QNetworkInterface>
#include <QTime>
#include <QSettings>
#include <memory>

#include "common.h"
#include "ice.h"
#include "icesessiontester.h"


ICE::ICE()
{}

ICE::~ICE()
{}


/* @param type - 0 for relayed, 126 for host
 * @param local - local preference for selecting candidates
 * @param component - 1 for RTP, 2 for RTCP */
int ICE::calculatePriority(CandidateType type, quint16 local, uint8_t component)
{
  return (16777216 * type) + (256 * local) + component;
}


QList<std::shared_ptr<ICEInfo>> ICE::generateICECandidates(
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > localCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > globalCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > stunCandidates,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > stunBindings,
    std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > turnCandidates)
{
  printDebug(DEBUG_NORMAL, this, "Start Generating ICE candidates", {
               "Local", "Global", "STUN", "STUN bindings", "TURN"},
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
    addCandidates(stunCandidates, stunBindings, foundation, SERVER_REFLEXIVE, 65535, iceCandidates);
  }

  // TODO: relay needs bindings
  addCandidates(turnCandidates, nullptr, foundation, RELAY, 0, iceCandidates);

  return iceCandidates;
}


void ICE::addCandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > addresses,
                        std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > relayAddresses,
                        quint32& foundation, CandidateType type, quint16 localPriority,
                        QList<std::shared_ptr<ICEInfo>>& candidates)
{
  int currentIndex = 0;
  bool includeRelayAddress = relayAddresses != nullptr && addresses->size() == relayAddresses->size();

  QHostAddress relayAddress = QHostAddress("");
  quint16 relayPort = 0;

  if (addresses->size() >= currentIndex + 1)
  {
    if (includeRelayAddress)
    {
      relayAddress = relayAddresses->at(currentIndex).first;
      relayPort = relayAddresses->at(currentIndex).second;
    }

    candidates.push_back(makeCandidate(foundation, type, 1,
                                       addresses->at(currentIndex).first,
                                       addresses->at(currentIndex).second,
                                       relayAddress, relayPort, localPriority));

    candidates.push_back(makeCandidate(foundation, type, 2,
                                       addresses->at(currentIndex).first,
                                       addresses->at(currentIndex).second + 1,
                                       relayAddress, relayPort + 1, localPriority));

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
      // TODO: What restriction should the pairings have?
      // the global addresses should at least be matched to stun addresses

      // type (host/server reflexive) and component (RTP/RTCP) has to match
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
  QObject::connect(
      agent,
      &IceSessionTester::ready,
      this,
      &ICE::handleEndOfNomination,
      Qt::DirectConnection
  );

  agent->init(&nominationInfo_[sessionID].pairs, sessionID, 2);
  agent->start();
}


void ICE::handleEndOfNomination(QList<std::shared_ptr<ICEPair> > &streams, uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  if (streams.size() != 2 ||
      streams.at(0) == nullptr ||
      streams.at(1) == nullptr)
  {
    printDebug(DEBUG_ERROR, "ICE",  "Failed to nominate RTP/RTCP candidates!");
    nominationInfo_[sessionID].connectionNominated = false;
    emit nominationFailed(sessionID);
  }
  else 
  {
    nominationInfo_[sessionID].connectionNominated = true;

    // Create opus candidate on the fly. When this candidate (rtp && rtcp) was created
    // we intentionally allocated 4 ports instead of 2 for use.
    //
    // This is because we don't actually need to test that both HEVC and Opus work separately. Instead we can just
    // test HEVC and if that works we can assume that Opus works too (no reason why it shouldn't)
    std::shared_ptr<ICEPair> opusPairRTP  = std::make_shared<ICEPair>();
    std::shared_ptr<ICEPair> opusPairRTCP = std::make_shared<ICEPair>();

    opusPairRTP->local  = std::make_shared<ICEInfo>();
    opusPairRTP->remote = std::make_shared<ICEInfo>();

    memcpy(opusPairRTP->local.get(),  streams.at(0)->local.get(),  sizeof(ICEInfo));
    memcpy(opusPairRTP->remote.get(), streams.at(0)->remote.get(), sizeof(ICEInfo));

    opusPairRTCP->local  = std::make_shared<ICEInfo>();
    opusPairRTCP->remote = std::make_shared<ICEInfo>();

    memcpy(opusPairRTCP->local.get(),  streams.at(1)->local.get(),  sizeof(ICEInfo));
    memcpy(opusPairRTCP->remote.get(), streams.at(1)->remote.get(), sizeof(ICEInfo));

    opusPairRTP->local->port  += 2; // hevc rtp, hevc rtcp and then opus rtp
    opusPairRTCP->local->port += 2; // hevc rtp, hevc, rtcp, opus rtp and then opus rtcp

    opusPairRTP->remote->port  += 2; // hevc rtp, hevc rtcp and then opus rtp
    opusPairRTCP->remote->port += 2; // hev rtp, hevc, rtcp, opus rtp and then opus rtcp

    nominationInfo_[sessionID].selectedPairs = {streams.at(0), streams.at(1),
                                                opusPairRTP, opusPairRTCP};
    emit nominationSucceeded(sessionID);
  }

  // TODO: Please call this before emitting the signal that we are ready
  // This way the UDP sending process stops before media is created.
  nominationInfo_[sessionID].agent->quit();
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
