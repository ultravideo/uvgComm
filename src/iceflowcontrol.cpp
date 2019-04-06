#include <QEventLoop>
#include <QTimer>

#include "connectiontester.h"
#include "iceflowcontrol.h"
#include "ice.h"

FlowAgent::FlowAgent():
  candidates_(nullptr),
  sessionID_(0)
{
}

FlowAgent::~FlowAgent()
{
  delete candidates_;
}

void FlowAgent::run() { }

void FlowAgent::setCandidates(QList<ICEPair *> *candidates)
{
  candidates_ = candidates;
}

void FlowAgent::setSessionID(uint32_t sessionID)
{
  sessionID_ = sessionID;
}

void FlowAgent::nominationDone(ICEPair *rtp, ICEPair *rtcp)
{
  if (nominated_mtx.try_lock())
  {
    nominated_rtp_  = rtp;
    nominated_rtcp_ = rtcp;
  }

  emit endNomination();
}

bool FlowAgent::waitForResponses(unsigned long timeout)
{
  QTimer timer;
  QEventLoop loop;

  timer.setSingleShot(true);

  connect(this,   &FlowAgent::endNomination, &loop, &QEventLoop::quit);
  connect(&timer, &QTimer::timeout,          &loop, &QEventLoop::quit);

  timer.start(20000);
  loop.exec();

  return timer.isActive();
}

void FlowController::run()
{
  Stun stun;

  // TODO how long should we sleep???
  /* for (volatile int i = 0; i < 10000000; ++i) */
  /*   ; */

  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    qDebug() << "ERROR: invalid candidates, unable to perform ICE candidate negotiation!";
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  std::vector<std::unique_ptr<ConnectionTester>> workerThreads;

  for (int i = 0; i < candidates_->size(); i += 2)
  {
    workerThreads.push_back(std::make_unique<ConnectionTester>());

    connect(workerThreads.back().get(), &ConnectionTester::testingDone, this, &FlowAgent::nominationDone, Qt::DirectConnection);

    workerThreads.back()->setCandidatePair(candidates_->at(i), candidates_->at(i + 1));
    workerThreads.back()->isController(true);
    workerThreads.back()->start();
  }

  bool nominationSucceeded = waitForResponses(10000);

  // we got a response, suspend all threads and start nomination
  for (size_t i = 0; i < workerThreads.size(); ++i)
  {
    workerThreads[i]->quit();
    workerThreads[i]->wait();
  }

  // we've spawned threads for each candidate, wait for responses at most 10 seconds
  if (!nominationSucceeded)
  {
    qDebug() << "Remote didn't respond to our request in time!";
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  if (!stun.sendNominationRequest(nominated_rtp_))
  {
    qDebug() << "Failed to nominate RTP candidate!";
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  if (!stun.sendNominationRequest(nominated_rtcp_))
  {
    qDebug() << "Failed to nominate RTCP candidate!";
    emit ready(nominated_rtp_, nullptr, sessionID_);
    return;
  }

  nominated_rtp_->state  = PAIR_NOMINATED;
  nominated_rtcp_->state = PAIR_NOMINATED;

  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}

void FlowControllee::run()
{
  QTimer timer;
  QEventLoop loop;

  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    qDebug() << "ERROR: invalid candidates, unable to perform ICE candidate negotiation!";
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  std::vector<std::unique_ptr<ConnectionTester>> workerThreads;

  for (int i = 0; i < candidates_->size(); i += 2)
  {
    workerThreads.push_back(std::make_unique<ConnectionTester>());

    connect(workerThreads.back().get(), &ConnectionTester::testingDone, this, &FlowAgent::nominationDone, Qt::DirectConnection);

    workerThreads.back()->setCandidatePair(candidates_->at(i), candidates_->at(i + 1));
    workerThreads.back()->isController(false);
    workerThreads.back()->start();
  }


  bool nominationSucceeded = waitForResponses(20000);

  // kill all threads, regardless of whether nomination succeeded or not
  for (size_t i = 0; i < workerThreads.size(); ++i)
  {
    workerThreads[i]->quit();
    workerThreads[i]->wait();
  }

  // wait for nomination from remote, wait at most 20 seconds
  if (!nominationSucceeded)
  {
    qDebug() << "Nomination from remote was not received in time!";
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  // media transmission can be started
  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}
