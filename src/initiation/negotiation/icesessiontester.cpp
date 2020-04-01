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


void IceSessionTester::nominationDone(std::shared_ptr<ICEPair> connection)
{
  Q_ASSERT(connection != nullptr);

  nominated_mtx.lock();
  finished_[connection->local->foundation][connection->local->component] = connection;

  // nominated check makes sure only one stream is nominated.
  // if we have received all components, nominate these.
  // TODO: Do some sort of prioritization here
  if (nominated_.empty() && finished_[connection->local->foundation].size() == components_)
  {
    for (auto& pair : finished_[connection->local->foundation])
    {
      pair->state = PAIR_SUCCEEDED;
      nominated_.push_back(pair);
    }

    emit endNomination();
  }
  nominated_mtx.unlock();
}


bool IceSessionTester::waitForEndOfNomination(unsigned long timeout)
{
  QTimer timer;
  QEventLoop loop;

  timer.setSingleShot(true);

  QObject::connect(
      this,
      &IceSessionTester::endNomination,
      &loop,
      &QEventLoop::quit,
      Qt::DirectConnection
  );

  QObject::connect(
      &timer,
      &QTimer::timeout,
      &loop,
      &QEventLoop::quit,
      Qt::DirectConnection
  );

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}


void IceSessionTester::run()
{
  if (pairs_ == nullptr || pairs_->size() == 0)
  {
    printDebug(DEBUG_ERROR, this,
               "Invalid candidates, unable to perform ICE candidate negotiation!");
    QList<std::shared_ptr<ICEPair>> no_streams;
    emit ready(no_streams, sessionID_);
    return;
  }

  // because we can only bind to a port once (no multithreaded access), we must divide
  // the candidates into interfaces
  // InterfaceTester then binds to this interface and takes care of sending the STUN Binding requests

  QList<std::shared_ptr<IceCandidateTester>> candidates;

  QString prevAddr  = "";
  uint16_t prevPort = 0;

  for (auto& pair : *pairs_)
  {
    // move to next candidate
    if (pair->local->address != prevAddr ||
        pair->local->port != prevPort)
    {
      candidates.push_back(createCandidateTester(pair->local));
    }

    candidates.back()->addCandidate(pair);

    // this keeps the code from crashing using magic. Do not move.
    prevAddr = pair->local->address;
    prevPort = pair->local->port;
  }

  for (auto& interface : candidates)
  {
    QObject::connect(
        interface.get(),
        &IceCandidateTester::candidateFound,
        this,
        &IceSessionTester::nominationDone,
        Qt::DirectConnection
    );

    interface->startTestingPairs(controller_);
  }

  bool nominationSucceeded = waitForEndOfNomination(timeout_);

  for (auto& interface : candidates)
  {
    interface->endTests();
  }

  QList<std::shared_ptr<ICEPair>> streams;

  // wait for nomination from remote, wait at most 20 seconds
  if (!nominationSucceeded)
  {
    printError(this, "Nomination from remote was not received in time!");
    emit ready(streams, sessionID_);
    return;
  }

  if (controller_)
  {
    IceCandidateTester tester;
    if (!tester.performNomination(nominated_))
    {
      emit ready(streams, sessionID_);
      return;
    }
  }

/*
  printImportant(this, "Nomination finished", {"Winning pair"}, {
                   nominated_rtp_->local->address  + ":" +
                   QString::number(nominated_rtp_->local->port) + " <-> " +
                   nominated_rtp_->remote->address + ":" +
                   QString::number(nominated_rtp_->remote->port)});
*/
  emit ready(nominated_, sessionID_);
}


std::shared_ptr<IceCandidateTester> IceSessionTester::createCandidateTester(std::shared_ptr<ICEInfo> local)
{
  std::shared_ptr<IceCandidateTester> tester = std::make_shared<IceCandidateTester>();

  // because we cannot modify create new objects from child threads (in this case new socket)
  // we must initialize both UDPServer and Stun objects here so that ConnectionTester doesn't have to do
  // anything but to test the connection

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
      printProgramError(this, "Relay address not set, when it is supposed to! Should be checked earlier.");
      return tester;
    }
  }

  tester->bindInterface(bindAddress, bindPort);

  return tester;
}
