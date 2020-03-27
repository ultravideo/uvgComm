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
int ICE::calculatePriority(int type, int local, int component)
{
  Q_ASSERT(type      == HOST || type      == RELAYED);
  Q_ASSERT(component == RTP  || component == RTCP);

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

  addCandidates(localCandidates, "host", iceCandidates);
  addCandidates(globalCandidates, "host", iceCandidates);

  if (stunCandidates->size() == stunBindings->size())
  {
    for (int i = 0; i < stunCandidates->size(); ++i)
    {
      // rtp
      stunBindings_.push_back(std::shared_ptr<STUNBinding>(
                                new STUNBinding{stunCandidates->at(i).first, stunCandidates->at(i).second,
                                                stunBindings->at(i).first,   stunBindings->at(i).second}));

      // rtcp
      stunBindings_.push_back(std::shared_ptr<STUNBinding>(
                                new STUNBinding{stunCandidates->at(i).first, stunCandidates->at(i).second,
                                                stunBindings->at(i).first,   stunBindings->at(i).second}));
      stunBindings_.back()->bindPort += 1;
      stunBindings_.back()->stunPort += 1;
    }

    addCandidates(stunCandidates, "srflx", iceCandidates);
  }
  addCandidates(turnCandidates, "relay", iceCandidates);

  return iceCandidates;
}


void ICE::addCandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t> > > addresses,
                        const QString &candidatesType,
                        QList<std::shared_ptr<ICEInfo>>& candidates)
{
  for (auto& address : *addresses)
  {
    std::pair<std::shared_ptr<ICEInfo>, std::shared_ptr<ICEInfo>> rtpCandidate
        = makeCandidate(address, candidatesType);

    candidates.push_back(rtpCandidate.first);
    candidates.push_back(rtpCandidate.second);
  }
}


std::pair<std::shared_ptr<ICEInfo>, std::shared_ptr<ICEInfo>>
  ICE::makeCandidate(const std::pair<QHostAddress, uint16_t>& addressPort, QString type)
{
  std::shared_ptr<ICEInfo> entry_rtp  = std::make_shared<ICEInfo>();
  std::shared_ptr<ICEInfo> entry_rtcp = std::make_shared<ICEInfo>();

  entry_rtp->address  = addressPort.first.toString();
  entry_rtcp->address = addressPort.first.toString();

  entry_rtp->port  = addressPort.second;
  entry_rtcp->port = entry_rtp->port + 1;

  // for each RTP/RTCP pair foundation is the same
  const QString foundation = generateRandomString(15);

  entry_rtp->foundation  = foundation;
  entry_rtcp->foundation = foundation;

  entry_rtp->transport  = "UDP";
  entry_rtcp->transport = "UDP";

  entry_rtp->component  = RTP;
  entry_rtcp->component = RTCP;

  if (type == "host")
  {
    entry_rtp->priority  = calculatePriority(126, 1, RTP);
    entry_rtcp->priority = calculatePriority(126, 1, RTCP);
  }
  else
  {
    entry_rtp->priority  = calculatePriority(0, 1, RTP);
    entry_rtcp->priority = calculatePriority(0, 1, RTCP);
  }

  entry_rtp->type  = type;
  entry_rtcp->type = type;

  return std::make_pair(entry_rtp, entry_rtcp);
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

  // modify values if we use STUN ports, because they require
  // different binding for their address.
  transformBindingCandidates(nominationInfo_[sessionID].pairs);

  agent->init(&nominationInfo_[sessionID].pairs, sessionID);
  agent->start();
}


void ICE::handleEndOfNomination(QList<std::shared_ptr<ICEPair>>& streams,
                                uint32_t sessionID)
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
    nominationInfo_[sessionID].nominatedVideo = std::make_pair(streams.at(0), streams.at(1));

    // Create opus candidate on the fly. When this candidate (rtp && rtcp) was created
    // we intentionally allocated 4 ports instead of 2 for use.
    //
    // This is because we don't actually need to test that both HEVC and Opus work separately. Insted we can just
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

    nominationInfo_[sessionID].nominatedAudio =
      std::make_pair(opusPairRTP, opusPairRTCP);

    emit nominationSucceeded(sessionID);
  }

  // TODO: Please call this before emitting the signal that we are ready
  // This way the UDP sending process stops before media is created.
  nominationInfo_[sessionID].agent->quit();
}


ICEMediaInfo ICE::getNominated(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  if (nominationInfo_.contains(sessionID))
  {
    return {
        nominationInfo_[sessionID].nominatedVideo,
        nominationInfo_[sessionID].nominatedAudio
    };
  }

  return {
    std::make_pair(nullptr, nullptr),
    std::make_pair(nullptr, nullptr)
  };
}


void ICE::cleanupSession(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  if (nominationInfo_.contains(sessionID))
  {
    nominationInfo_.remove(sessionID);
  }
}


void ICE::transformBindingCandidates(QList<std::shared_ptr<ICEPair>>& pairs)
{
  for  (auto& pair : pairs)
  {
    for (auto& binding : stunBindings_)
    {
      if (pair->local->address == binding->stunAddress.toString() &&
          pair->local->port == binding->stunPort)
      {
        pair->local->address = binding->bindAddress.toString();
        pair->local->port = binding->bindPort;
        printNormal(this, "Found stun binding, using different address", {"Change"},
        {binding->stunAddress.toString() + ":" + QString::number(binding->stunPort) + " >> " +
         binding->bindAddress.toString() + ":" + QString::number(binding->bindPort)});
      }
    }
  }
}
