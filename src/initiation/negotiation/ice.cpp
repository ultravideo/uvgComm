#include <QNetworkInterface>
#include <QTime>
#include <QSettings>
#include <memory>

#include "common.h"
#include "ice.h"
#include "flowagent.h"


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
  addCandidates(stunCandidates, "srflx", iceCandidates);
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


void ICE::printCandidate(ICEInfo *candidate)
{
  Q_ASSERT(candidate != nullptr);
  printNormal(this, "Candidate: " + candidate->foundation + " " + candidate->priority + ": "
              + candidate->address    + ":" + candidate->port);
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

        pair->local    = local[i];
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
    nominationInfo_[sessionID].agent = new FlowAgent(true, 10000);
  }
  else
  {
    nominationInfo_[sessionID].agent = new FlowAgent(false, 20000);
  }


  nominationInfo_[sessionID].pairs = makeCandidatePairs(local, remote);
  nominationInfo_[sessionID].connectionNominated = false;

  FlowAgent *agent = nominationInfo_[sessionID].agent;
  QObject::connect(
      agent,
      &FlowAgent::ready,
      this,
      &ICE::handleEndOfNomination,
      Qt::DirectConnection
  );

  agent->setCandidates(&nominationInfo_[sessionID].pairs);
  agent->setSessionID(sessionID);
  agent->start();
}


void ICE::handleEndOfNomination(
    std::shared_ptr<ICEPair> rtp,
    std::shared_ptr<ICEPair> rtcp,
    uint32_t sessionID
)
{
  Q_ASSERT(sessionID != 0);

  if (rtp == nullptr || rtcp == nullptr)
  {
    printDebug(DEBUG_ERROR, "ICE",  "Failed to nominate RTP/RTCP candidates!");
    nominationInfo_[sessionID].connectionNominated = false;
    emit nominationFailed(sessionID);
  }
  else 
  {
    nominationInfo_[sessionID].connectionNominated = true;
    nominationInfo_[sessionID].nominatedVideo = std::make_pair(rtp, rtcp);

    // Create opus candidate on the fly. When this candidate (rtp && rtcp) was created
    // we intentionally allocated 4 ports instead of 2 for use.
    //
    // This is because we don't actually need to test that both HEVC and Opus work separately. Insted we can just
    // test HEVC and if that works we can assume that Opus works too (no reason why it shouldn't)
    std::shared_ptr<ICEPair> opusPairRTP  = std::make_shared<ICEPair>();
    std::shared_ptr<ICEPair> opusPairRTCP = std::make_shared<ICEPair>();

    opusPairRTP->local  = std::make_shared<ICEInfo>();
    opusPairRTP->remote = std::make_shared<ICEInfo>();

    memcpy(opusPairRTP->local.get(),  rtp->local.get(),  sizeof(ICEInfo));
    memcpy(opusPairRTP->remote.get(), rtp->remote.get(), sizeof(ICEInfo));

    opusPairRTCP->local  = std::make_shared<ICEInfo>();
    opusPairRTCP->remote = std::make_shared<ICEInfo>();

    memcpy(opusPairRTCP->local.get(),  rtcp->local.get(),  sizeof(ICEInfo));
    memcpy(opusPairRTCP->remote.get(), rtcp->remote.get(), sizeof(ICEInfo));

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

