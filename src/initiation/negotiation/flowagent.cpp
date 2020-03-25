#include <QEventLoop>
#include <QTimer>
#include <QThread>

#include "common.h"
#include "connectiontester.h"
#include "interfacetester.h"
#include "flowagent.h"
#include "ice.h"



FlowAgent::FlowAgent(bool controller, int timeout):
  candidates_(nullptr),
  sessionID_(0),
  controller_(controller),
  timeout_(timeout)
{}


FlowAgent::~FlowAgent()
{}


void FlowAgent::setCandidates(QList<std::shared_ptr<ICEPair>> *candidates)
{
  Q_ASSERT(candidates != nullptr);

  candidates_ = candidates;
}


void FlowAgent::setSessionID(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  sessionID_ = sessionID;
}


void FlowAgent::nominationDone(std::shared_ptr<ICEPair> connection)
{
  Q_ASSERT(connection != nullptr);

  nominated_mtx.lock();

  if (connection->local->component == RTP)
  {
    nominated_[connection->local->address].first = connection;
  }
  else
  {
    nominated_[connection->local->address].second = connection;
  }

  if (nominated_[connection->local->address].first  != nullptr &&
      nominated_[connection->local->address].second != nullptr)
  {
    nominated_rtp_  = nominated_[connection->local->address].first;
    nominated_rtcp_ = nominated_[connection->local->address].second;

    nominated_rtp_->state  = PAIR_NOMINATED;
    nominated_rtcp_->state = PAIR_NOMINATED;

    emit endNomination();
    nominated_mtx.unlock();
    return;
  }

  nominated_mtx.unlock();
}


bool FlowAgent::waitForEndOfNomination(unsigned long timeout)
{
  QTimer timer;
  QEventLoop loop;

  timer.setSingleShot(true);

  QObject::connect(
      this,
      &FlowAgent::endNomination,
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


void FlowAgent::run()
{
  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    printDebug(DEBUG_ERROR, this,
               "Invalid candidates, unable to perform ICE candidate negotiation!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  // because we can only bind to a port once (no multithreaded access), we must divide
  // the candidates into interfaces
  // InterfaceTester then binds to this interface and takes care of sending the STUN Binding requests

  QList<std::shared_ptr<InterfaceTester>> interfaces;

  QString prevAddr  = "";
  uint16_t prevPort = 0;

  for (auto& candidate : *candidates_)
  {
    if (candidate->local->address != prevAddr ||
        candidate->local->port != prevPort)
    {
      interfaces.push_back(std::shared_ptr<InterfaceTester>(new InterfaceTester));

      // because we cannot modify create new objects from child threads (in this case new socket)
      // we must initialize both UDPServer and Stun objects here so that ConnectionTester doesn't have to do
      // anything but to test the connection
      //
      // Binding might fail, if so happens no STUN objects are created for this socket
      if (!interfaces.back()->bindInterface(QHostAddress(candidate->local->address),
            candidate->local->port))
      {
        continue;
      }
    }

    interfaces.back()->addCandidate(candidate);

    // this keeps the code from crashing using magic. Do not move.
    prevAddr = candidate->local->address;
    prevPort = candidate->local->port;
  }

  for (auto& interface : interfaces)
  {
    QObject::connect(
        interface.get(),
        &InterfaceTester::candidateFound,
        this,
        &FlowAgent::nominationDone,
        Qt::DirectConnection
    );

    interface->startTestingPairs(controller_);
  }

  bool nominationSucceeded = waitForEndOfNomination(timeout_);

  for (auto& interface : interfaces)
  {
    interface->endTests();
  }

  // wait for nomination from remote, wait at most 20 seconds
  if (!nominationSucceeded)
  {
    printError(this, "Nomination from remote was not received in time!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  if (controller_)
  {
    ConnectionTester tester(new UDPServer, false);

    if (!tester.sendNominationRequest(nominated_rtp_.get()))
    {
      printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTP candidate!");
      emit ready(nullptr, nullptr, sessionID_);
      return;
    }

    if (!tester.sendNominationRequest(nominated_rtcp_.get()))
    {
      printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTCP candidate!");
      emit ready(nominated_rtp_, nullptr, sessionID_);
      return;
    }

    nominated_rtp_->state  = PAIR_NOMINATED;
    nominated_rtcp_->state = PAIR_NOMINATED;
  }

  printImportant(this, "Nomination finished", {"Winning pair"}, {
                   nominated_rtp_->local->address  + ":" + QString::number(nominated_rtp_->local->port) + " <-> " +
                   nominated_rtp_->remote->address + ":" + QString::number(nominated_rtp_->remote->port)});

  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}
