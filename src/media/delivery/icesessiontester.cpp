#include "icesessiontester.h"

#include "icecandidatetester.h"

#include "common.h"
#include "logger.h"

#include <QEventLoop>
#include <QTimer>
#include <QThread>

const uint32_t CONTROLLER_SESSION_TIMEOUT_MS = 10000;
const uint32_t NONCONTROLLER_SESSION_TIMEOUT_MS = 20000;


IceSessionTester::IceSessionTester(bool controller):
  pairs_(nullptr),
  isController_(controller),
  timeoutMs_(0),
  components_(0)
{
  if (controller)
  {
    timeoutMs_ = CONTROLLER_SESSION_TIMEOUT_MS;
  }
  else
  {
    timeoutMs_ = NONCONTROLLER_SESSION_TIMEOUT_MS;
  }
}


IceSessionTester::~IceSessionTester()
{}


void IceSessionTester::init(std::vector<std::shared_ptr<ICEPair> > *pairs, uint8_t components)
{
  Q_ASSERT(pairs != nullptr);
  pairs_ = pairs;
  components_ = components;
}


void IceSessionTester::componentSucceeded(std::shared_ptr<ICEPair> connection)
{
  Q_ASSERT(connection != nullptr);

  nominated_mtx.lock();

  if (finished_[connection->local->foundation].find(connection->local->component)
      != finished_[connection->local->foundation].end())
  {
    Logger::getLogger()->printError(this, 
                                    "Component finished, but it has already finished before.");
  }

  finished_[connection->local->foundation][connection->local->component] = connection;

  QString type = "Controller";
  if (!isController_)
  {
    type = "Controllee";
  }

  Logger::getLogger()->printNormal(this, type + " component finished", {"Finished components"},
                                   {QString::number(finished_[connection->local->foundation].size()) + 
                                   "/" + QString::number(components_)});

  // nominated check makes sure only one stream is nominated.
  // if we have received all components, nominate these.
  // TODO: Do some sort of prioritization here.
  if (nominated_.empty() &&
      finished_[connection->local->foundation].size() == components_)
  {
    for (auto& pair : finished_[connection->local->foundation])
    {
      pair->state = PAIR_SUCCEEDED;
      nominated_.push_back(pair);
    }

    emit endTesting();
  }
  nominated_mtx.unlock();
}


void IceSessionTester::waitForEndOfTesting(unsigned long timeoutMs)
{
  QTimer timer;

  // eventloop is an additional thread for receiving udp packets through socket.
  QEventLoop loop;

  timer.setSingleShot(true);

  QObject::connect(
      this,  &IceSessionTester::endTesting,
      &loop, &QEventLoop::quit,
      Qt::DirectConnection);

  QObject::connect(
      &timer, &QTimer::timeout,
      &loop,  &QEventLoop::quit,
      Qt::DirectConnection);

  timer.start(timeoutMs);

  startTime_ = std::chrono::system_clock::now();

  loop.exec();
}


void IceSessionTester::run()
{
  if (pairs_ == nullptr || pairs_->size() == 0)
  {
    Logger::getLogger()->printDebug(DEBUG_ERROR, this,
                                    "Invalid candidates, "
                                    "unable to perform ICE candidate negotiation!");
    emit iceFailure(pairs_);
    return;
  }

  QVector<std::shared_ptr<ICECandidateTester>> candidates;

  QString prevAddr  = "";
  uint16_t prevPort = 0;

  // because we can only bind to a port once (no multithreaded access), we must
  // do the testing based on candidates we gave to peer

  // the candidates should be in order at this point because of how the pairs
  // are formed
  for (auto& pair : *pairs_)
  {
    // move to next candidate
    if (pair->local->address != prevAddr ||
        pair->local->port != prevPort)
    {
      candidates.push_back(createCandidateTester(pair->local));
    }

    candidates.back()->addCandidate(pair);

    prevAddr = pair->local->address;
    prevPort = pair->local->port;
  }

  // start testing for this interface/port combination
  for (auto& interface : candidates)
  {
    if (isController_)
    {
      QObject::connect(
          interface.get(), &ICECandidateTester::controllerPairFound,
          this,            &IceSessionTester::componentSucceeded,
          Qt::DirectConnection);
    }
    else
    {
      QObject::connect(
          interface.get(), &ICECandidateTester::controlleeNominationDone,
          this,            &IceSessionTester::componentSucceeded,
          Qt::DirectConnection);
    }

    interface->startTestingPairs(isController_);
  }

  // now we wait until the connection tests have ended. Wait at most timeout_
  waitForEndOfTesting(timeoutMs_);

  for (auto& interface : candidates)
  {
    interface->endTests();
  }

  nominated_mtx.lock();

  // we did not get nominations and instead we got a timeout
  if (nominated_.empty())
  {
    Logger::getLogger()->printError(this, 
                                    "Nominations from remote were not received in time!",
                                    "Time since start", QString::number(
                                      std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::system_clock::now() - startTime_).count()) + " ms");
    emit iceFailure(pairs_);
    nominated_mtx.unlock();
    return;
  }

  // if we are the controller we must perform the final nomination.
  // non-controller does not reach this point until after nomination (or timeout).
  if (isController_)
  {
    ICECandidateTester tester;
    if (!tester.performNomination(nominated_))
    {
      emit iceFailure(pairs_);
      nominated_mtx.unlock();
      return;
    }
  }

  // announce that we have succeeded in nomination.
  emit iceSuccess(nominated_);
  nominated_mtx.unlock();
}


std::shared_ptr<ICECandidateTester> IceSessionTester::createCandidateTester(
    std::shared_ptr<ICEInfo> local)
{
  std::shared_ptr<ICECandidateTester> tester = std::make_shared<ICECandidateTester>();

  QHostAddress bindAddress = QHostAddress(local->address);
  quint16 bindPort = local->port;

  // use relayport if we are supposed to
  if (local->type != "host")
  {
    if (local->rel_address != "" &&
        local->rel_port != 0)
    {
      bindAddress = QHostAddress(local->rel_address);
      bindPort = local->rel_port;
    }
    else
    {
      Logger::getLogger()->printProgramError(this, "Relay address not set, "
                                                   "when it is supposed to! "
                                                   "Should be checked earlier.");
      return tester;
    }
  }

  // because we cannot modify objects created from child threads we must bind here
  tester->bindInterface(bindAddress, bindPort);

  return tester;
}
