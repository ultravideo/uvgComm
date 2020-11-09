#include "icesessiontester.h"

#include "icecandidatetester.h"
#include "ice.h"
#include "common.h"

#include <QEventLoop>
#include <QTimer>
#include <QThread>


IceSessionTester::IceSessionTester(bool controller, int timeout):
  pairs_(nullptr),
  sessionID_(0),
  controller_(controller),
  timeout_(timeout),
  components_(0)
{}


IceSessionTester::~IceSessionTester()
{}


void IceSessionTester::init(QList<std::shared_ptr<ICEPair>> *pairs,
                            uint32_t sessionID, uint8_t components)
{
  Q_ASSERT(pairs != nullptr);
  Q_ASSERT(sessionID != 0);
  pairs_ = pairs;
  sessionID_ = sessionID;
  components_ = components;
}


void IceSessionTester::componentSucceeded(std::shared_ptr<ICEPair> connection)
{
  Q_ASSERT(connection != nullptr);

  nominated_mtx.lock();

  if (finished_[connection->local->foundation].find(connection->local->component)
      != finished_[connection->local->foundation].end())
  {
    printError(this, "Component finished, but it has already finished before.");
  }

  finished_[connection->local->foundation][connection->local->component] = connection;

  QString type = "Controller";
  if (!controller_)
  {
    type = "Controllee";
  }

  printNormal(this, type + " component finished", {"Finished components"},
              {QString::number(finished_[connection->local->foundation].size()) + "/" +
              QString::number(components_)});

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


void IceSessionTester::waitForEndOfTesting(unsigned long timeout)
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

  timer.start(timeout);
  loop.exec();
}


void IceSessionTester::run()
{
  if (pairs_ == nullptr || pairs_->size() == 0)
  {
    printDebug(DEBUG_ERROR, this,
               "Invalid candidates, unable to perform ICE candidate negotiation!");
    emit iceFailure(sessionID_);
    return;
  }

  QList<std::shared_ptr<IceCandidateTester>> candidates;

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
    if (controller_)
    {
      QObject::connect(
          interface.get(), &IceCandidateTester::controllerPairFound,
          this,            &IceSessionTester::componentSucceeded,
          Qt::DirectConnection);
    }
    else
    {
      QObject::connect(
          interface.get(), &IceCandidateTester::controlleeNominationDone,
          this,            &IceSessionTester::componentSucceeded,
          Qt::DirectConnection);
    }

    interface->startTestingPairs(controller_);
  }

  // TODO: This is not technically according to specification. The nomination
  // should only be done for once per component. The current implementation
  // is who can get all their components finished fastest.


  // now we wait until the connection tests have ended. Wait at most timeout_
  waitForEndOfTesting(timeout_);

  for (auto& interface : candidates)
  {
    interface->endTests();
  }

  nominated_mtx.lock();

  // we did not get nominations and instead we got a timeout
  if (nominated_.empty())
  {
    printError(this, "Nominations from remote were not received in time!");
    emit iceFailure(sessionID_);
    nominated_mtx.unlock();
    return;
  }

  // if we are the controller we must perform the final nomination.
  // non-controller does not reach this point until after nomination (or timeout).
  if (controller_)
  {
    IceCandidateTester tester;
    if (!tester.performNomination(nominated_))
    {
      emit iceFailure(sessionID_);
      nominated_mtx.unlock();
      return;
    }
  }

  // announce that we have succeeded in nomination.
  emit iceSuccess(nominated_, sessionID_);
  nominated_mtx.unlock();
}


std::shared_ptr<IceCandidateTester> IceSessionTester::createCandidateTester(
    std::shared_ptr<ICEInfo> local)
{
  std::shared_ptr<IceCandidateTester> tester = std::make_shared<IceCandidateTester>();

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
      printProgramError(this, "Relay address not set, when it is supposed to! "
                              "Should be checked earlier.");
      return tester;
    }
  }

  // because we cannot modify create new objects from child threads we must bind here
  tester->bindInterface(bindAddress, bindPort);

  return tester;
}
