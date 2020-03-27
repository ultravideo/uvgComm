#include <QEventLoop>
#include <QTimer>
#include <QThread>

#include "common.h"
#include "icecandidatetester.h"
#include "icesessiontester.h"
#include "ice.h"



IceSessionTester::IceSessionTester(bool controller, int timeout):
  candidates_(nullptr),
  sessionID_(0),
  controller_(controller),
  timeout_(timeout)
{}


IceSessionTester::~IceSessionTester()
{}


void IceSessionTester::init(QList<std::shared_ptr<ICEPair>> *candidates,
                            uint32_t sessionID)
{
  Q_ASSERT(candidates != nullptr);
  Q_ASSERT(sessionID != 0);
  candidates_ = candidates;
  sessionID_ = sessionID;
}


void IceSessionTester::nominationDone(std::shared_ptr<ICEPair> connection)
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
  if (candidates_ == nullptr || candidates_->size() == 0)
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

  QList<std::shared_ptr<IceCandidateTester>> interfaces;

  QString prevAddr  = "";
  uint16_t prevPort = 0;

  for (auto& candidate : *candidates_)
  {
    if (candidate->local->address != prevAddr ||
        candidate->local->port != prevPort)
    {
      interfaces.push_back(std::shared_ptr<IceCandidateTester>(new IceCandidateTester));

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
        &IceCandidateTester::candidateFound,
        this,
        &IceSessionTester::nominationDone,
        Qt::DirectConnection
    );

    interface->startTestingPairs(controller_);
  }

  bool nominationSucceeded = waitForEndOfNomination(timeout_);

  for (auto& interface : interfaces)
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
    if (!tester.performNomination(nominated_rtp_, nominated_rtcp_))
    {
      emit ready(streams, sessionID_);
      return;
    }
  }

  printImportant(this, "Nomination finished", {"Winning pair"}, {
                   nominated_rtp_->local->address  + ":" +
                   QString::number(nominated_rtp_->local->port) + " <-> " +
                   nominated_rtp_->remote->address + ":" +
                   QString::number(nominated_rtp_->remote->port)});

  streams = {nominated_rtp_, nominated_rtcp_};
  emit ready(streams, sessionID_);
}
