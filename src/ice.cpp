#include <QNetworkInterface>
#include <QTime>
#include <QSettings>
#include <memory>

#include "ice.h"
#include "iceflowcontrol.h"

const uint16_t MIN_ICE_PORT   = 22001;
const uint16_t MAX_ICE_PORT   = 22500;
const uint16_t MAX_PORTS      = 100;

ICE::ICE():
  stun_(),
  stunAddress_(QHostAddress("")),
  parameters_()
{
  parameters_.setPortRange(MIN_ICE_PORT, MAX_ICE_PORT, MAX_PORTS);

  QObject::connect(&stun_, SIGNAL(addressReceived(QHostAddress)), this, SLOT(createSTUNCandidate(QHostAddress)));
  stun_.wantAddress("stun.l.google.com");

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  int ice = settings.value("sip/ice").toInt();

  iceDisabled_ = (ice == 1) ? true : false;
}

ICE::~ICE()
{
}

// TODO lue speksi uudelleen tämä osalta
/* @param type - 0 for relayed, 126 for host (always 126 TODO is it really?)
 * @param local - local preference for selecting candidates (ie.
 * @param component - */
int ICE::calculatePriority(int type, int local, int component)
{
  // TODO speksin mukaan local pitää olal 0xffff ipv4-only hosteille
  // TODO explain these coefficients
  return (16777216 * type) + (256 * local) + component;
}

QString ICE::generateFoundation()
{
  const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
  const int randomStringLength = 15;

  QString randomString;

  for (int i = 0; i < randomStringLength; ++i)
  {
    int index = qrand() % possibleCharacters.length();
    QChar nextChar = possibleCharacters.at(index);
    randomString.append(nextChar);
  }

  return randomString;
}

QList<ICEInfo *> ICE::generateICECandidates()
{
  QTime time = QTime::currentTime();
  qsrand((uint)time.msec());

  QList<ICEInfo *> candidates;
  std::pair<ICEInfo *, ICEInfo *> candidate;

  foreach (const QHostAddress& address, QNetworkInterface::allAddresses())
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol &&
        (address.toString().startsWith("10.")   ||
         address.toString().startsWith("192.")  ||
         address.toString().startsWith("172.")))
    {
      candidate = makeCandidate(address, "host");

      candidates.push_back(candidate.first);
      candidates.push_back(candidate.second);
    }
  }

  if (stunAddress_ != QHostAddress(""))
  {
    candidate = makeCandidate(stunAddress_, "srflx");

    candidates.push_back(candidate.first);
    candidates.push_back(candidate.second);
  }

#if 0
    candidate = makeCandidate(QHostAddress("172.17.0.1"), "host");

    candidates.push_back(candidate.first);
    candidates.push_back(candidate.second);
#endif

  return candidates;
}

std::pair<ICEInfo *, ICEInfo *> ICE::makeCandidate(QHostAddress address, QString type)
{
  ICEInfo *entry_rtp  = new ICEInfo;
  ICEInfo *entry_rtcp = new ICEInfo;

  entry_rtp->address  = address.toString();
  entry_rtcp->address = address.toString();

  entry_rtp->port  = parameters_.allocateMediaPorts();
  entry_rtcp->port = entry_rtp->port + 1;

  // for each RTP/RTCP pair foundation is the same
  const QString foundation = generateFoundation();

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

void ICE::createSTUNCandidate(QHostAddress address)
{
  if (address == QHostAddress(""))
  {
    qDebug() << "WARNING: Failed to resolve public IP! Server-reflexive candidates won't be created!";
  }

  stunAddress_ = address;
}

void ICE::printCandidate(ICEInfo *candidate)
{
  qDebug() << candidate->foundation << " " << candidate->priority << ": "
           << candidate->address    << ":" << candidate->port;
}

QList<ICEPair *> *ICE::makeCandiatePairs(QList<ICEInfo *>& local, QList<ICEInfo *>& remote)
{
  QList<ICEPair *> *pairs = new QList<ICEPair *>;

  // match all host candidates with remote (remote does the same)
  for (int i = 0; i < local.size(); ++i)
  {
    for (int k = 0; k < remote.size(); ++k)
    {
      // type (host/server reflexive) and component (RTP/RTCP) has to match
      if (local[i]->type == remote[k]->type &&
          local[i]->component == remote[k]->component)
      {
        ICEPair *pair = new ICEPair;

        pair->local    = local[i];
        pair->remote   = remote[k];
        pair->priority = qMin(local[i]->priority, remote[k]->priority); // TODO spec
        pair->state    = PAIR_FROZEN;

        pairs->push_back(pair);
      }
    }
  }

#if 0
  for (int i = 0; i < pairs->size(); ++i)
  {
    qDebug() << pairs->at(i)->local->type << ":" << pairs->at(i)->local->address << ":" << pairs->at(i)->local->port
             << pairs->at(i)->remote->address << ":" << pairs->at(i)->remote->port;
  }
#endif

  return pairs;
}


// callee (flow controller)
// 
// this function spawns a control thread and exist right away so the 200 OK
// response can be set as fast as possible and the remote can start respoding to our requests
//
// Thread spawned by startNomination() must keep track of which candidates failed and which succeeded
void ICE::startNomination(QList<ICEInfo *>& local, QList<ICEInfo *>& remote, uint32_t sessionID)
{
  if (iceDisabled_ == true)
  {
    return;
  }

  // nomination-related memory is released when handleEndOfNomination() is called
  nominationInfo_[sessionID].controller = new FlowController;

  nominationInfo_[sessionID].callee_mtx = new QMutex;
  nominationInfo_[sessionID].callee_mtx->lock();

  nominationInfo_[sessionID].pairs = makeCandiatePairs(local, remote);
  nominationInfo_[sessionID].connectionNominated = false;

  FlowController *callee = nominationInfo_[sessionID].controller;
  QObject::connect(callee, &FlowController::ready, this, &ICE::handleCalleeEndOfNomination, Qt::DirectConnection);

  callee->setCandidates(nominationInfo_[sessionID].pairs);
  callee->setSessionID(sessionID);
  callee->start();
}

// caller (flow controllee)
//
// respondToNominations() spawns a control thread that starts testing all candidates
// It doesn't do any external book keeping as it's responsible for only responding to STUN requets
// When it has gone through all candidate pairs it exits
void ICE::respondToNominations(QList<ICEInfo *>& local, QList<ICEInfo *>& remote, uint32_t sessionID)
{
  if (iceDisabled_ == true)
  {
    return;
  }

  // nomination-related memory is released when handleEndOfNomination() is called
  nominationInfo_[sessionID].controllee = new FlowControllee;

  nominationInfo_[sessionID].caller_mtx = new QMutex;
  nominationInfo_[sessionID].caller_mtx->lock();

  nominationInfo_[sessionID].pairs = makeCandiatePairs(local, remote);
  nominationInfo_[sessionID].connectionNominated = false;

  FlowControllee *caller = nominationInfo_[sessionID].controllee;
  QObject::connect(caller, &FlowControllee::ready, this, &ICE::handleCallerEndOfNomination, Qt::DirectConnection);

  /* caller->setHevcCandidates( */
  caller->setCandidates(nominationInfo_[sessionID].pairs);
  caller->setSessionID(sessionID);
  caller->start();
}

bool ICE::callerConnectionNominated(uint32_t sessionID)
{
  // return immediately if ICE was disabled
  if (iceDisabled_ == true)
  {
    return true;
  }

  while (!nominationInfo_[sessionID].caller_mtx->try_lock_for(std::chrono::milliseconds(200)))
  {
  }

  nominationInfo_[sessionID].caller_mtx->unlock();
  delete nominationInfo_[sessionID].caller_mtx;

  return nominationInfo_[sessionID].connectionNominated;
}

bool ICE::calleeConnectionNominated(uint32_t sessionID)
{
  // return immediately if ICE was disabled
  if (iceDisabled_ == true)
  {
    return true;
  }

  while (!nominationInfo_[sessionID].callee_mtx->try_lock_for(std::chrono::milliseconds(200)))
  {
  }

  nominationInfo_[sessionID].callee_mtx->unlock();
  delete nominationInfo_[sessionID].callee_mtx;

  return nominationInfo_[sessionID].connectionNominated;
}

void ICE::handleEndOfNomination(ICEPair *candidateRTP, ICEPair *candidateRTCP, uint32_t sessionID)
{
  // nothing needs to be cleaned if ICE was disabled
  if (iceDisabled_ == true)
  {
    return;
  }

  if (candidateRTP == nullptr || candidateRTCP == nullptr)
  {
    qDebug() << "ERROR: Nomination failed! Unable to start call.";
    nominationInfo_[sessionID].connectionNominated = false;
  }
  else 
  {
    nominationInfo_[sessionID].connectionNominated = true;
    nominationInfo_[sessionID].nominatedHEVC = std::make_pair((ICEPair *)candidateRTP, (ICEPair *)candidateRTCP);

    // Create opus candidate on the fly. When this candidate (candidateRTP && candidateRTCP) was created
    // we intentionally allocated 4 ports instead of 2 for use.
    //
    // This is because we don't actually need to test that both HEVC and Opus work separately. Insted we can just
    // test HEVC and if that works we can assume that Opus works too (no reason why it shouldn't)
    ICEPair *opusPairRTP  = new ICEPair;
    opusPairRTP->local    = new ICEInfo;
    opusPairRTP->remote   = new ICEInfo;

    ICEPair *opusPairRTCP = new ICEPair;
    opusPairRTCP->local   = new ICEInfo;
    opusPairRTCP->remote  = new ICEInfo;

    memcpy(opusPairRTP->local,  candidateRTP->local,  sizeof(ICEInfo));
    memcpy(opusPairRTP->remote, candidateRTP->remote, sizeof(ICEInfo));

    memcpy(opusPairRTCP->local,  candidateRTCP->local,  sizeof(ICEInfo));
    memcpy(opusPairRTCP->remote, candidateRTCP->remote, sizeof(ICEInfo));

    opusPairRTP->local->port  += 2; // hevc rtp, hevc rtcp and then opus rtp
    opusPairRTCP->local->port += 2; // hevc rtp, hevc, rtcp, opus rtp and then opus rtcp

    // same for remote candidate
    opusPairRTP->remote->port  += 2; // hevc rtp, hevc rtcp and then opus rtp
    opusPairRTCP->remote->port += 2; // hev rtp, hevc, rtcp, opus rtp and then opus rtcp

    nominationInfo_[sessionID].nominatedOpus = std::make_pair((ICEPair *)opusPairRTP, (ICEPair *)opusPairRTCP);
  }

  foreach (ICEPair *pair, *nominationInfo_[sessionID].pairs)
  {
    if (pair->state != PAIR_NOMINATED)
    {
      // pair->local cannot be freed here because we send final SDP
      // to remote which contains our candidate offers but we can release
      // pair->local->port (unless local is stun_entry_rtp_ or stun_entry_rtcp_)
      //
      // because ports are allocated in pairs (RTP is port X and RTCP is port X + 1),
      // we need to call makePortPairAvailable() only for RTP candidate
      if (pair->local->component == RTP)
      {
        parameters_.deallocateMediaPorts(pair->local->port);
      }

      /* delete pair->remote; */
      /* delete pair; */
    }
  }

  delete nominationInfo_[sessionID].pairs;
}

void ICE::handleCallerEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP, uint32_t sessionID)
{
  nominationInfo_[sessionID].caller_mtx->unlock();
  nominationInfo_[sessionID].controllee->quit();

  this->handleEndOfNomination(candidateRTP, candidateRTCP, sessionID);
}

void ICE::handleCalleeEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP, uint32_t sessionID)
{
  nominationInfo_[sessionID].callee_mtx->unlock();
  nominationInfo_[sessionID].controller->quit();

  this->handleEndOfNomination(candidateRTP, candidateRTCP, sessionID);
}

ICEMediaInfo ICE::getNominated(uint32_t sessionID)
{
  if (nominationInfo_.contains(sessionID) && iceDisabled_ == false)
  {
    return {
        nominationInfo_[sessionID].nominatedHEVC,
        nominationInfo_[sessionID].nominatedOpus
    };
  }

  return {
    std::make_pair(nullptr, nullptr),
    std::make_pair(nullptr, nullptr)
  };
}
