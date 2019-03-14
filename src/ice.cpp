#include <QNetworkInterface>
#include "ice.h"
#include <QTime>

ICE::ICE():
  portPair(22000),
  connectionNominated_(false),
  nominatingConnection_(false),
  stun_(),
  stun_entry_rtp_(new ICEInfo),
  stun_entry_rtcp_(new ICEInfo)
{
  QObject::connect(&stun_, SIGNAL(addressReceived(QHostAddress)), this, SLOT(createSTUNCandidate(QHostAddress)));
  stun_.wantAddress("stun.l.google.com");
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
  // TODO explain these coefficients
  return (16777216 * type)  + (256 * local) + component;
}

QString ICE::generateFoundation()
{
   const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
   const int randomStringLength = 15; // assuming you want random strings of 12 characters

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
  QList<ICEInfo *> candidates;

  QTime time = QTime::currentTime();
  qsrand((uint)time.msec());

  foreach (const QHostAddress &address, QNetworkInterface::allAddresses())
  {
    if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost))
    {
      ICEInfo *entry_rtp  = new ICEInfo;
      ICEInfo *entry_rtcp = new ICEInfo;

      entry_rtp->address  = address.toString();
      entry_rtcp->address = address.toString();

      entry_rtp->port  = portPair++;
      entry_rtcp->port = portPair++;

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

  // add previously created STUN candidates too
  candidates.push_back(stun_entry_rtp_);
  candidates.push_back(stun_entry_rtcp_);

  return candidates;
}

void ICE::createSTUNCandidate(QHostAddress address)
{
  qDebug() << "Creating STUN candidate...";

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
void ICE::startNomination(QList<ICEInfo *>& local, QList<ICEInfo *>& remote)
{
  qDebug() << "\n\n\nCANDIDATE NOMINATION STARTED";

  connectionNominated_ = false;
  nominating_mtx.lock();

  // this memory is release by FlowController
  QList<ICEPair *> *pairs = makeCandiatePairs(local, remote);

  caller_ = new FlowController;
  QObject::connect(caller_, &FlowController::ready, this, &ICE::handleEndOfNomination, Qt::DirectConnection);

  caller_->setCandidates(pairs);
  caller_->start();
}

// caller (flow controllee [TODO does that expression make sense?])
//
// respondToNominations() spawns a control thread that starts testing all candidates
// It doesn't do any external book keeping as it's responsible for only responding to STUN requets
// When it has gone through all candidate pairs it exits
void ICE::respondToNominations(QList<ICEInfo *>& local, QList<ICEInfo *>& remote)
{
  qDebug() << "\n\n\nCANDIDATE NOMINATION RESPONDING STARTED";

  // this memory is released by FlowControllee
  QList<ICEPair *> *pairs = makeCandiatePairs(local, remote);

  FlowControllee *callee = new FlowControllee;
  QObject::connect(callee, &FlowControllee::ready, this, &ICE::handleEndOfNomination, Qt::DirectConnection);

  callee->setCandidates(pairs);
  callee->start();
}

bool ICE::connectionNominated()
{
  // nominating_mtx will stay locked until handleNomination() is called.
  // handleNomination() is called when FlowController has
  // processing the candidates
  while (!nominating_mtx.try_lock_for(std::chrono::milliseconds(200)))
    ;

  return connectionNominated_;
}

void ICE::handleEndOfNomination(struct ICEPair *candidateRTP, struct ICEPair *candidateRTCP)
{
  connectionNominated_ = (candidateRTP != nullptr && candidateRTCP != nullptr);

  // TODO minne nominatedCandidate kuuluu tallentaa niin että video conferencing toimii??

  if (connectionNominated_)
  {
    qDebug() << "CONNECTION NOMINATION DONE!";
    nominated_rtp  = candidateRTP;
    nominated_rtcp = candidateRTCP;
  }

  nominating_mtx.unlock();
}

std::pair<ICEPair *, ICEPair *> ICE::getNominated()
{
  return std::make_pair(nominated_rtp, nominated_rtcp);
}
