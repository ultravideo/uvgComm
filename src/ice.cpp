#include <QNetworkInterface>
#include <QTime>
#include <memory>

#include "ice.h"
#include "iceflowcontrol.h"

const uint16_t MIN_ICE_PORT   = 22001;
const uint16_t MAX_ICE_PORT   = 22500;
const uint16_t MAX_PORTS      = 100;

ICE::ICE():
  portPair(22000),
  stun_(),
  stun_entry_rtp_(nullptr),
  stun_entry_rtcp_(nullptr),
  parameters_()
{
  parameters_.setPortRange(MIN_ICE_PORT, MAX_ICE_PORT, MAX_PORTS);

  QObject::connect(&stun_, SIGNAL(addressReceived(QHostAddress)), this, SLOT(createSTUNCandidate(QHostAddress)));
  stun_.wantAddress("stun.l.google.com");
}

ICE::~ICE()
{
  delete stun_entry_rtp_;
  delete stun_entry_rtcp_;
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

  foreach (const QHostAddress& address, QNetworkInterface::allAddresses())
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol &&
        (address.toString().startsWith("10.")   ||
         address.toString().startsWith("192.")  ||
         address.toString().startsWith("172.")  ||
         address == QHostAddress(QHostAddress::LocalHost)))
    {
      ICEInfo *entry_rtp  = new ICEInfo;
      ICEInfo *entry_rtcp = new ICEInfo;

      entry_rtp->address  = address.toString();
      entry_rtcp->address = address.toString();

      entry_rtp->port  = parameters_.nextAvailablePortPair();
      entry_rtcp->port = entry_rtp->port + 1;

      // for each RTP/RTCP pair foundation is the same
      const QString foundation = generateFoundation();

      entry_rtp->foundation  = foundation;
      entry_rtcp->foundation = foundation;

      entry_rtp->transport  = "UDP";
      entry_rtcp->transport = "UDP";

      entry_rtp->component  = RTP;
      entry_rtcp->component = RTCP;

      entry_rtp->priority  = calculatePriority(126, 1, RTP);
      entry_rtcp->priority = calculatePriority(126, 1, RTCP);

      entry_rtp->type  = "host";
      entry_rtcp->type = "host";

      candidates.push_back(entry_rtp);
      candidates.push_back(entry_rtcp);
    }
  }

  if (stun_entry_rtp_ != nullptr && stun_entry_rtcp_ != nullptr)
  {
    candidates.push_back(stun_entry_rtp_);
    candidates.push_back(stun_entry_rtcp_);
  }

  return candidates;
}

void ICE::createSTUNCandidate(QHostAddress address)
{
  if (address == QHostAddress(""))
  {
    qDebug() << "WARNING: Failed to resolve public IP! Server-reflexive candidates won't be created!";
    return;
  }

  qDebug() << "Creating STUN candidate...";

  stun_entry_rtp_  = new ICEInfo;
  stun_entry_rtcp_ = new ICEInfo;

  stun_entry_rtp_->address  = address.toString();
  stun_entry_rtcp_->address = address.toString();

  stun_entry_rtp_->port  = portPair++;
  stun_entry_rtcp_->port = portPair++;

  // for each RTP/RTCP pair foundation is the same
  const QString foundation = generateFoundation();

  stun_entry_rtp_->foundation  = foundation;
  stun_entry_rtcp_->foundation = foundation;

  stun_entry_rtp_->transport  = "UDP";
  stun_entry_rtcp_->transport = "UDP";

  stun_entry_rtp_->component  = RTP;
  stun_entry_rtcp_->component = RTCP;

  stun_entry_rtp_->priority  = calculatePriority(126, 0, RTP);
  stun_entry_rtcp_->priority = calculatePriority(126, 0, RTCP);

  stun_entry_rtp_->type  = "srflx";
  stun_entry_rtcp_->type = "srflx";
}

void ICE::printCandidate(ICEInfo *candidate)
{
  qDebug() << candidate->foundation << " " << candidate->priority << ": "
           << candidate->address    << ":" << candidate->port;
}

// TODO sort them by priority/type/whatever first and make sure ports match!!!
QList<ICEPair *> *ICE::makeCandiatePairs(QList<ICEInfo *>& local, QList<ICEInfo *>& remote)
{
  QList<ICEPair *> *pairs = new QList<ICEPair *>;

  // create pairs
  for (int i = 0; i < qMin(local.size(), remote.size()); ++i)
  {
    ICEPair *pair = new ICEPair;

    pair->local    = local[i];
    pair->remote   = remote[i];
    pair->priority = qMin(local[i]->priority, remote[i]->priority);
    pair->state    = PAIR_FROZEN;

    pairs->push_back(pair);
  }

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
  // nomination-related memory is released when handleEndOfNomination() is called
  nominationInfo_[sessionID].controllee = new FlowControllee;

  nominationInfo_[sessionID].caller_mtx = new QMutex;
  nominationInfo_[sessionID].caller_mtx->lock();

  nominationInfo_[sessionID].pairs = makeCandiatePairs(local, remote);
  nominationInfo_[sessionID].connectionNominated = false;

  FlowControllee *caller = nominationInfo_[sessionID].controllee;
  QObject::connect(caller, &FlowControllee::ready, this, &ICE::handleCallerEndOfNomination, Qt::DirectConnection);

  caller->setCandidates(nominationInfo_[sessionID].pairs);
  caller->setSessionID(sessionID);
  caller->start();
}

bool ICE::callerConnectionNominated(uint32_t sessionID)
{
  while (!nominationInfo_[sessionID].caller_mtx->try_lock_for(std::chrono::milliseconds(200)))
  {
  }

  nominationInfo_[sessionID].caller_mtx->unlock();
  delete nominationInfo_[sessionID].caller_mtx;

  return nominationInfo_[sessionID].connectionNominated;
}

bool ICE::calleeConnectionNominated(uint32_t sessionID)
{
  while (!nominationInfo_[sessionID].callee_mtx->try_lock_for(std::chrono::milliseconds(200)))
  {
  }

  nominationInfo_[sessionID].callee_mtx->unlock();
  delete nominationInfo_[sessionID].callee_mtx;

  return nominationInfo_[sessionID].connectionNominated;
}

void ICE::handleEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP, uint32_t sessionID)
{
  nominationInfo_[sessionID].connectionNominated = (candidateRTP != nullptr && candidateRTCP != nullptr);

  if (nominationInfo_[sessionID].connectionNominated)
  {
    nominationInfo_[sessionID].nominatedPair = std::make_pair((ICEPair *)candidateRTP, (ICEPair *)candidateRTCP);
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
      if (pair->local != stun_entry_rtp_ && pair->local != stun_entry_rtcp_ && pair->local->component == RTP)
      {
        parameters_.makePortPairAvailable(pair->local->port);
      }

      delete pair->remote;
      delete pair;
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

std::pair<ICEPair *, ICEPair *> ICE::getNominated(uint32_t sessionID)
{
  if (nominationInfo_.contains(sessionID))
  {
    return nominationInfo_[sessionID].nominatedPair;
  }
  else
  {
    return std::make_pair<ICEPair *, ICEPair *>(nullptr, nullptr);
  }
}
